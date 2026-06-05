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
 * @file test_UtilsFile.cpp
 *
 * L1 unit tests for helpers/UtilsFile.h
 *
 * Functions under test:
 *
 *   Utils::MoveFile(const string& from, const string& to) -> bool
 *     Uses WPEFramework::Core::File / Directory to copy bytes and then
 *     destroy the source. Returns false if:
 *       - source does not exist
 *       - destination already exists
 *       - source cannot be opened (Open(true) = read-only open fails)
 *       - destination cannot be created
 *
 *   Utils::getLastLine(const string& input, string& res_str) -> bool
 *     Splits input on '\n' via stringstream + getline and returns the
 *     last non-empty line. Returns false if no non-empty line is found.
 *
 * Wrapped syscall notes:
 *   Only ::unlink() is intercepted in the helpers test build.
 *   WPEFramework::Core::File::Destroy() calls its own internal unlink
 *   through libc (NOT wrapped), so it works normally.
 *   Our fixture calls ::unlink() for cleanup → must route through
 *   WrapsImplMock → __real_unlink.
 */

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Module.h"
#include "UtilsFile.h"      /* MoveFile, getLastLine */
#include "UtilsLogging.h"
#include "WrapsMock.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int __real_unlink(const char* path);

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

/* ===========================================================
 * Base fixture — registers WrapsImplMock for ::unlink()
 * =========================================================== */
class UtilsFileFixture : public ::testing::Test {
protected:
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };

    void SetUp() override
    {
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(_))
            .WillByDefault(Invoke(__real_unlink));
    }

    void TearDown() override
    {
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }

    /* Helper: return true iff path exists on disk. */
    static bool pathExists(const std::string& path)
    {
        struct stat st;
        return ::stat(path.c_str(), &st) == 0;
    }

    /* Helper: write content to a file, creating it if absent. */
    static void writeFile(const std::string& path, const std::string& content)
    {
        std::ofstream f(path, std::ios::out | std::ios::trunc);
        ASSERT_TRUE(f.is_open()) << "Cannot write to " << path;
        f << content;
    }
};

/* ===========================================================
 * MoveFile tests
 * =========================================================== */
class MoveFileTest : public UtilsFileFixture {
protected:
    std::string srcFile;
    std::string dstFile;

    void SetUp() override
    {
        UtilsFileFixture::SetUp();

        /* Unique source path */
        mode_t old_umask = ::umask(0077); /* Set restrictive umask: owner-only access */
        char src[] = "/tmp/utilsmovefile_src_XXXXXX";
        int fd = ::mkstemp(src);
        ::umask(old_umask); /* Restore original umask */
        ASSERT_NE(-1, fd) << strerror(errno);
        if (fd >= 0) {
            ::close(fd);
        }
        srcFile = src;

        /* Destination path (not yet created) */
        dstFile = std::string(src) + "_dst";
        (void)::unlink(dstFile.c_str()); /* ensure absent */
    }

    void TearDown() override
    {
        /* Best-effort cleanup */
        (void)::unlink(srcFile.c_str());
        (void)::unlink(dstFile.c_str());
        UtilsFileFixture::TearDown();
    }
};

/* ---- source does not exist ---- */

TEST_F(MoveFileTest, SourceDoesNotExistReturnsFalse)
{
    LOGINFO("Test: MoveFile returns false when source file does not exist");
    (void)::unlink(srcFile.c_str()); /* remove source */
    ASSERT_FALSE(pathExists(srcFile));

    bool result = Utils::MoveFile(srcFile, dstFile);

    EXPECT_FALSE(result);
    EXPECT_FALSE(pathExists(dstFile)) << "Destination must not be created on failure";
}

/* ---- destination already exists ---- */

TEST_F(MoveFileTest, DestinationAlreadyExistsReturnsFalse)
{
    LOGINFO("Test: MoveFile returns false when destination already exists");
    writeFile(srcFile, "source content");
    writeFile(dstFile, "existing content");
    ASSERT_TRUE(pathExists(dstFile));

    bool result = Utils::MoveFile(srcFile, dstFile);

    EXPECT_FALSE(result);
    /* Original destination content must be intact */
    std::ifstream df(dstFile);
    std::string content((std::istreambuf_iterator<char>(df)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ("existing content", content);
}

/* ---- successful move ---- */

TEST_F(MoveFileTest, SuccessfulMoveReturnsTrueContentCopiedSourceGone)
{
    LOGINFO("Test: MoveFile copies content to destination and deletes source");
    const std::string data = "hello from MoveFile test";
    writeFile(srcFile, data);
    ASSERT_FALSE(pathExists(dstFile));

    bool result = Utils::MoveFile(srcFile, dstFile);

    EXPECT_TRUE(result);

    /* Destination must exist with identical content */
    ASSERT_TRUE(pathExists(dstFile));
    std::ifstream df(dstFile);
    std::string got((std::istreambuf_iterator<char>(df)),
                     std::istreambuf_iterator<char>());
    EXPECT_EQ(data, got);

    /* Source must be gone */
    EXPECT_FALSE(pathExists(srcFile));
}

TEST_F(MoveFileTest, SuccessfulMoveEmptyFileReturnsTrueSourceGone)
{
    LOGINFO("Test: MoveFile handles an empty source file");
    /* srcFile is already created empty by mkstemp */
    writeFile(srcFile, "");
    ASSERT_FALSE(pathExists(dstFile));

    bool result = Utils::MoveFile(srcFile, dstFile);

    EXPECT_TRUE(result);
    EXPECT_TRUE(pathExists(dstFile));
    EXPECT_FALSE(pathExists(srcFile));
}

TEST_F(MoveFileTest, SuccessfulMoveMultilineContent)
{
    LOGINFO("Test: MoveFile preserves multiline file content exactly");
    std::string data = "line1\nline2\nline3\n";
    writeFile(srcFile, data);

    bool result = Utils::MoveFile(srcFile, dstFile);

    EXPECT_TRUE(result);
    std::ifstream df(dstFile);
    std::string got((std::istreambuf_iterator<char>(df)),
                     std::istreambuf_iterator<char>());
    EXPECT_EQ(data, got);
}

/* ---- destination path in subdirectory ---- */

TEST_F(MoveFileTest, DestinationInSubdirectoryCreatesPath)
{
    LOGINFO("Test: MoveFile creates intermediate directories via Directory::CreatePath");
    const std::string subDir  = "/tmp/utilsmovefile_subdir_test";
    const std::string dstSub  = subDir + "/moved_file.txt";

    /* Ensure clean state */
    (void)::unlink(dstSub.c_str());
    ::rmdir(subDir.c_str());

    writeFile(srcFile, "subdir content");

    bool result = Utils::MoveFile(srcFile, dstSub);

    /* Cleanup regardless of result to avoid residue */
    (void)::unlink(dstSub.c_str());
    ::rmdir(subDir.c_str());

    EXPECT_TRUE(result);
}

/* ===========================================================
 * getLastLine tests
 * =========================================================== */
class GetLastLineTest : public UtilsFileFixture {
};

/* ---- empty input ---- */

TEST_F(GetLastLineTest, EmptyInputReturnsFalse)
{
    LOGINFO("Test: getLastLine returns false for empty string");
    std::string result;
    EXPECT_FALSE(Utils::getLastLine("", result));
}

/* ---- single line without trailing newline ---- */

TEST_F(GetLastLineTest, SingleLineNoNewlineReturnsTrueWithLine)
{
    LOGINFO("Test: getLastLine returns the only line when there is no trailing newline");
    std::string result;
    bool ok = Utils::getLastLine("onlyline", result);

    EXPECT_TRUE(ok);
    EXPECT_EQ("onlyline", result);
}

/* ---- single line with trailing newline ---- */

TEST_F(GetLastLineTest, SingleLineWithTrailingNewlineReturnsTrueWithLine)
{
    LOGINFO("Test: trailing newline produces empty last segment — result is the line before it");
    std::string result;
    bool ok = Utils::getLastLine("onlyline\n", result);

    EXPECT_TRUE(ok);
    EXPECT_EQ("onlyline", result);
}

/* ---- multiple lines ---- */

TEST_F(GetLastLineTest, MultipleLineReturnsLast)
{
    LOGINFO("Test: getLastLine returns the last non-empty line of multi-line input");
    std::string result;
    bool ok = Utils::getLastLine("first\nsecond\nthird", result);

    EXPECT_TRUE(ok);
    EXPECT_EQ("third", result);
}

TEST_F(GetLastLineTest, MultipleLineWithTrailingNewlineReturnsLastNonEmpty)
{
    LOGINFO("Test: trailing newline is ignored; last non-empty line returned");
    std::string result;
    bool ok = Utils::getLastLine("first\nsecond\nthird\n", result);

    EXPECT_TRUE(ok);
    EXPECT_EQ("third", result);
}

/* ---- all-empty lines → no non-empty line found ---- */

TEST_F(GetLastLineTest, AllEmptyLinesReturnsFalse)
{
    LOGINFO("Test: input with only newlines has no non-empty lines → false");
    std::string result;
    EXPECT_FALSE(Utils::getLastLine("\n\n\n", result));
}

TEST_F(GetLastLineTest, OnlySingleNewlineReturnsFalse)
{
    LOGINFO("Test: input of just '\\n' has no non-empty line → false");
    std::string result;
    EXPECT_FALSE(Utils::getLastLine("\n", result));
}

/* ---- mixed empty and non-empty lines ---- */

TEST_F(GetLastLineTest, EmptyLinesInterspersedReturnsLastNonEmpty)
{
    LOGINFO("Test: empty lines between content lines — last non-empty wins");
    std::string result;
    bool ok = Utils::getLastLine("first\n\nsecond\n\n", result);

    EXPECT_TRUE(ok);
    EXPECT_EQ("second", result);
}

/* ---- line with spaces is non-empty ---- */

TEST_F(GetLastLineTest, WhitespaceOnlyLineIsNonEmpty)
{
    LOGINFO("Test: a line of spaces is treated as non-empty (not trimmed by getLastLine)");
    std::string result;
    bool ok = Utils::getLastLine("real\n   ", result);

    EXPECT_TRUE(ok);
    /* '   ' is a non-empty string, so it replaces 'real' */
    EXPECT_EQ("   ", result);
}
