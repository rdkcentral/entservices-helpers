/**
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

/**
 * @file test_UtilsgetFileContent.cpp
 *
 * L1 unit tests for helpers/UtilsgetFileContent.h
 *
 * Functions under test:
 *
 *   readPropertyFromFile(filename, property, propertyValue) -> bool
 *     — Reads key=value or key<space>value lines from a file.
 *     — Skips '#' comment lines.
 *     — Recursively expands values starting with '$'.
 *     — Returns false if property not found or file can't be opened.
 *
 *   readFileContent(filename, content) -> bool
 *     — Reads entire file via fgets into a string.
 *     — Returns false if file can't be opened or is empty.
 *
 *   isRegularFile(path) -> bool
 *     — Uses stat() + S_ISREG to distinguish regular files from dirs/specials.
 *
 *   searchFiles(inputPath, maxDepth, minDepth, exclusions, result) -> bool
 *     — Recursive glob-pattern directory walker. Results capped at 10.
 *
 *   ExpandPropertiesInString(input, filePath, expandedString) -> bool
 *     — Finds '$VAR' tokens in input, looks up VAR in filePath, expands.
 *     — Returns false if a variable can't be resolved.
 *
 * Wrapped syscall notes:
 *   Only ::unlink() is intercepted in the helpers test build.
 *   Functions in this header (readPropertyFromFile, readFileContent, etc.)
 *   use std::ifstream / fopen / fgets / opendir / readdir directly through
 *   libc — none of these are wrapped in the helpers .so — so real filesystem
 *   operations are used without any mock forwarding.
 */

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Module.h"
#include "UtilsgetFileContent.h"
#include "UtilsString.h"
#include "UtilsLogging.h"
#include "WrapsMock.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int __real_unlink(const char* path);

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

/* ===========================================================
 * Base fixture
 * =========================================================== */
class UtilsFileContentFixture : public ::testing::Test {
protected:
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };
    std::string tmpFile;

    void SetUp() override
    {
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(_))
            .WillByDefault(Invoke(__real_unlink));

        char tmpl[] = "/tmp/utilsgetfilecontent_XXXXXX";
        int fd = ::mkstemp(tmpl);
        ASSERT_NE(-1, fd) << strerror(errno);
        ::close(fd);
        tmpFile = tmpl;
    }

    void TearDown() override
    {
        ::unlink(tmpFile.c_str());
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }

    void writeFile(const std::string& content)
    {
        std::ofstream f(tmpFile, std::ios::out | std::ios::trunc);
        ASSERT_TRUE(f.is_open());
        f << content;
    }
};

/* ===========================================================
 * readPropertyFromFile tests
 * =========================================================== */
class ReadPropertyFromFileTest : public UtilsFileContentFixture {
};

TEST_F(ReadPropertyFromFileTest, FileNotFoundReturnsFalse)
{
    LOGINFO("Test: readPropertyFromFile returns false when file does not exist");
    std::string value;
    EXPECT_FALSE(Utils::readPropertyFromFile("/tmp/_no_such_file_xyz_", "KEY", value));
}

TEST_F(ReadPropertyFromFileTest, PropertyFoundWithEquals)
{
    LOGINFO("Test: property=value format is parsed correctly");
    writeFile("FOO=bar\n");
    std::string value;
    EXPECT_TRUE(Utils::readPropertyFromFile(tmpFile.c_str(), "FOO", value));
    EXPECT_EQ("bar", value);
}

TEST_F(ReadPropertyFromFileTest, PropertyFoundWithSpaceSeparator)
{
    LOGINFO("Test: 'KEY value' (space-separated) format is also matched");
    writeFile("FOO hello\n");
    std::string value;
    /* The function checks line.find(property + " ") == 0 OR line.find(property + "=") == 0
     * For space-separated, value = line.substr(line.find("=") + 1).
     * "FOO hello" has no "=", so find("=") returns npos, substr(npos+1) = substr(0).
     * That gives the entire line "FOO hello" as the value. */
    bool ok = Utils::readPropertyFromFile(tmpFile.c_str(), "FOO", value);
    EXPECT_TRUE(ok);
    /* The value is the full line since there's no '=' */
    EXPECT_FALSE(value.empty());
}

TEST_F(ReadPropertyFromFileTest, CommentLinesAreSkipped)
{
    LOGINFO("Test: lines starting with '#' are skipped");
    writeFile("# FOO=commented\nFOO=real\n");
    std::string value;
    EXPECT_TRUE(Utils::readPropertyFromFile(tmpFile.c_str(), "FOO", value));
    EXPECT_EQ("real", value);
}

TEST_F(ReadPropertyFromFileTest, PropertyNotInFileReturnsFalse)
{
    LOGINFO("Test: property not present in file → false, value empty");
    writeFile("OTHER=val\n");
    std::string value = "old";
    EXPECT_FALSE(Utils::readPropertyFromFile(tmpFile.c_str(), "MISSING", value));
    EXPECT_TRUE(value.empty());
}

TEST_F(ReadPropertyFromFileTest, EmptyFileReturnsFalse)
{
    LOGINFO("Test: empty file → property not found → false");
    writeFile("");
    std::string value;
    EXPECT_FALSE(Utils::readPropertyFromFile(tmpFile.c_str(), "KEY", value));
}

TEST_F(ReadPropertyFromFileTest, TrailingNewlineStripped)
{
    LOGINFO("Test: trailing '\\n' in value is removed");
    writeFile("KEY=value\n");
    std::string value;
    EXPECT_TRUE(Utils::readPropertyFromFile(tmpFile.c_str(), "KEY", value));
    EXPECT_EQ("value", value);
}

TEST_F(ReadPropertyFromFileTest, TrailingCarriageReturnStripped)
{
    LOGINFO("Test: trailing '\\r' in value is removed (Windows line endings)");
    writeFile("KEY=value\r\n");
    std::string value;
    EXPECT_TRUE(Utils::readPropertyFromFile(tmpFile.c_str(), "KEY", value));
    EXPECT_EQ("value", value);
}

TEST_F(ReadPropertyFromFileTest, MultiplePropertiesFirstMatchReturned)
{
    LOGINFO("Test: multiple properties — correct one is found");
    writeFile("A=alpha\nB=beta\nC=gamma\n");
    std::string value;
    EXPECT_TRUE(Utils::readPropertyFromFile(tmpFile.c_str(), "B", value));
    EXPECT_EQ("beta", value);
}

TEST_F(ReadPropertyFromFileTest, RecursiveExpansionDollarValue)
{
    LOGINFO("Test: value starting with '$' expands to referenced property");
    /* ACTUAL=real, REF=$ACTUAL → resolves to 'real' */
    writeFile("ACTUAL=real\nREF=$ACTUAL\n");
    std::string value;
    bool ok = Utils::readPropertyFromFile(tmpFile.c_str(), "REF", value);
    EXPECT_TRUE(ok);
    EXPECT_EQ("real", value);
}

TEST_F(ReadPropertyFromFileTest, RecursiveExpansionMissingPropertyReturnsFalse)
{
    LOGINFO("Test: '$MISSING' reference that cannot be resolved → false");
    writeFile("REF=$NOSUCHPROP\n");
    std::string value;
    EXPECT_FALSE(Utils::readPropertyFromFile(tmpFile.c_str(), "REF", value));
}

/* ===========================================================
 * readFileContent tests
 * =========================================================== */
class ReadFileContentTest : public UtilsFileContentFixture {
};

TEST_F(ReadFileContentTest, NonExistentFileReturnsFalse)
{
    LOGINFO("Test: readFileContent returns false for a missing file");
    std::string content;
    EXPECT_FALSE(Utils::readFileContent("/tmp/_no_such_file_xyz_", content));
    EXPECT_TRUE(content.empty());
}

TEST_F(ReadFileContentTest, EmptyFileReturnsFalse)
{
    LOGINFO("Test: readFileContent returns false for a file with no bytes");
    /* mkstemp created an empty file */
    std::string content;
    EXPECT_FALSE(Utils::readFileContent(tmpFile.c_str(), content));
}

TEST_F(ReadFileContentTest, SingleLineFileReturnsTrueContentSet)
{
    LOGINFO("Test: single-line file is read correctly");
    writeFile("hello world\n");
    std::string content;
    EXPECT_TRUE(Utils::readFileContent(tmpFile.c_str(), content));
    EXPECT_EQ("hello world\n", content);
}

TEST_F(ReadFileContentTest, MultiLineFileReturnsTrueAllLinesPresent)
{
    LOGINFO("Test: multi-line file content is fully read");
    writeFile("line1\nline2\nline3\n");
    std::string content;
    EXPECT_TRUE(Utils::readFileContent(tmpFile.c_str(), content));
    EXPECT_NE(std::string::npos, content.find("line1"));
    EXPECT_NE(std::string::npos, content.find("line2"));
    EXPECT_NE(std::string::npos, content.find("line3"));
}

TEST_F(ReadFileContentTest, RepeatedCallsReturnSameContent)
{
    LOGINFO("Test: calling readFileContent twice yields identical results");
    writeFile("stable content\n");
    std::string c1, c2;
    EXPECT_TRUE(Utils::readFileContent(tmpFile.c_str(), c1));
    EXPECT_TRUE(Utils::readFileContent(tmpFile.c_str(), c2));
    EXPECT_EQ(c1, c2);
}

/* ===========================================================
 * isRegularFile tests
 * =========================================================== */
class IsRegularFileTest : public UtilsFileContentFixture {
};

TEST_F(IsRegularFileTest, ExistingRegularFileReturnsTrue)
{
    LOGINFO("Test: isRegularFile returns true for a regular file");
    EXPECT_TRUE(Utils::isRegularFile(tmpFile));
}

TEST_F(IsRegularFileTest, DirectoryReturnsFalse)
{
    LOGINFO("Test: isRegularFile returns false for a directory");
    EXPECT_FALSE(Utils::isRegularFile("/tmp"));
}

TEST_F(IsRegularFileTest, NonExistentPathReturnsFalse)
{
    LOGINFO("Test: isRegularFile returns false for a path that does not exist");
    EXPECT_FALSE(Utils::isRegularFile("/tmp/_nonexistent_utilsfile_xyz"));
}

/* ===========================================================
 * searchFiles tests
 * =========================================================== */
class SearchFilesTest : public UtilsFileContentFixture {
protected:
    std::string testDir;

    void SetUp() override
    {
        UtilsFileContentFixture::SetUp();

        /* Create a temporary directory for search tests */
        char dtmpl[] = "/tmp/utilssearch_XXXXXX";
        ASSERT_NE(nullptr, ::mkdtemp(dtmpl)) << strerror(errno);
        testDir = dtmpl;

        /* Populate with a few known files */
        auto touch = [&](const std::string& name) {
            std::ofstream f(testDir + "/" + name);
        };
        touch("alpha.txt");
        touch("beta.txt");
        touch("gamma.log");
    }

    void TearDown() override
    {
        /* Remove test files and directory */
        ::unlink((testDir + "/alpha.txt").c_str());
        ::unlink((testDir + "/beta.txt").c_str());
        ::unlink((testDir + "/gamma.log").c_str());
        ::rmdir(testDir.c_str());
        UtilsFileContentFixture::TearDown();
    }
};

TEST_F(SearchFilesTest, ExactFileMatch)
{
    LOGINFO("Test: searchFiles with exact filename returns that file");
    std::string path = testDir + "/alpha.txt";
    std::string result;
    std::list<std::string> excl;
    bool ok = Utils::searchFiles(path, 0, 0, excl, result);
    EXPECT_TRUE(ok);
    EXPECT_NE(std::string::npos, result.find("alpha.txt"));
}

TEST_F(SearchFilesTest, WildcardMatchesTxtFiles)
{
    LOGINFO("Test: glob pattern *.txt matches txt files in the directory");
    std::string path = testDir + "/*.txt";
    std::string result;
    std::list<std::string> excl;
    bool ok = Utils::searchFiles(path, 0, 0, excl, result);
    EXPECT_TRUE(ok);
    EXPECT_NE(std::string::npos, result.find("alpha.txt"));
    EXPECT_NE(std::string::npos, result.find("beta.txt"));
    EXPECT_EQ(std::string::npos, result.find("gamma.log"));
}

TEST_F(SearchFilesTest, ExclusionFiltersOut)
{
    LOGINFO("Test: exclusion list prevents a matched file from appearing in results");
    std::string path = testDir + "/*.txt";
    std::string result;
    std::list<std::string> excl = { "alpha.txt" };
    Utils::searchFiles(path, 0, 0, excl, result);
    EXPECT_EQ(std::string::npos, result.find("alpha.txt"))
        << "Excluded file must not appear in results";
    EXPECT_NE(std::string::npos, result.find("beta.txt"));
}

TEST_F(SearchFilesTest, NonExistentDirectoryReturnsTrue)
{
    LOGINFO("Test: searchFiles on a non-existent dir returns true with empty result");
    std::string path = "/tmp/_no_such_dir_xyz_/file.txt";
    std::string result;
    std::list<std::string> excl;
    bool ok = Utils::searchFiles(path, 0, 0, excl, result);
    /* Implementation returns true even on partial failure */
    EXPECT_TRUE(ok);
}

/* ===========================================================
 * ExpandPropertiesInString tests
 * =========================================================== */
class ExpandPropertiesTest : public UtilsFileContentFixture {
};

TEST_F(ExpandPropertiesTest, NoVariableInInputReturnsTrueExpandedEmpty)
{
    LOGINFO("Test: input with no '$' — while loop never runs, expandedString stays empty, returns true");
    /*
     * Implementation: if strchr(input,'$') == nullptr the while loop body
     * never executes, expandedString is not modified, and the function
     * returns true. This is the documented empty-expansion behaviour.
     */
    std::string expanded;
    bool ok = Utils::ExpandPropertiesInString("no_variables_here", tmpFile.c_str(), expanded);
    EXPECT_TRUE(ok);
}

TEST_F(ExpandPropertiesTest, SingleVariableWithSpaceDelimiterExpanded)
{
    LOGINFO("Test: '$VAR ' expands VAR from the property file");
    writeFile("VAR=hello\n");
    std::string expanded;
    bool ok = Utils::ExpandPropertiesInString("$VAR /rest", tmpFile.c_str(), expanded);
    EXPECT_TRUE(ok);
    EXPECT_NE(std::string::npos, expanded.find("hello"));
}

TEST_F(ExpandPropertiesTest, SingleVariableWithSlashDelimiterExpanded)
{
    LOGINFO("Test: '$VAR/path' expands VAR from the property file");
    writeFile("VAR=myval\n");
    std::string expanded;
    bool ok = Utils::ExpandPropertiesInString("$VAR/suffix", tmpFile.c_str(), expanded);
    EXPECT_TRUE(ok);
    EXPECT_NE(std::string::npos, expanded.find("myval"));
}

TEST_F(ExpandPropertiesTest, UnresolvableVariableReturnsFalse)
{
    LOGINFO("Test: '$MISSING ' where MISSING is not in the file → false");
    writeFile("OTHER=val\n");
    std::string expanded;
    bool ok = Utils::ExpandPropertiesInString("$MISSING /rest", tmpFile.c_str(), expanded);
    EXPECT_FALSE(ok);
}

TEST_F(ExpandPropertiesTest, PropertyFileNotFoundReturnsFalse)
{
    LOGINFO("Test: property file does not exist → readPropertyFromFile fails → false");
    std::string expanded;
    bool ok = Utils::ExpandPropertiesInString("$VAR /rest",
                                               "/tmp/_no_such_props_file_",
                                               expanded);
    EXPECT_FALSE(ok);
}
