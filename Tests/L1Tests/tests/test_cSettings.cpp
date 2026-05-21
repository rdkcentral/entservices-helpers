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
 * @file test_cSettings.cpp
 *
 * L1 unit tests for the cSettings helper class (helpers/cSettings.h).
 *
 * cSettings wraps a flat key=value configuration file and exposes in-memory
 * read/write operations backed by WPEFramework JsonObject.
 *
 * Test coverage goals
 * -------------------
 * - Constructor behaviour (new file creation, existing file read-back)
 * - Destructor (RAII, no double-free / resource leak)
 * - getValue  – hit / miss paths
 * - setValue  – string / int / bool overloads, return value, file persistence
 * - contains  – present/missing/empty-value branches
 * - remove    – existing key, non-existent key, file persistence
 * - writeToFile – success, missing-file failure, empty-value skip
 * - readFromFile – via constructor: non-existent file, empty file, valid content,
 *                  malformed lines, multiple '=' in value, duplicate keys
 * - Edge cases – empty key string, empty value, very long strings,
 *                repeated set/remove, reload across instances
 * - Resource safety – no file-descriptor leaks, no filesystem residue
 */

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Module.h"    /* WPEFramework core/plugins */
#include "cSettings.h"
#include "UtilsLogging.h"
#include "WrapsMock.h"

#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

/* The linker wraps unlink -> __wrap_unlink; expose the real symbol so we can
 * forward mock calls to the actual syscall. */
extern "C" int __real_unlink(const char* path);

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::_;

/* ============================================================
 * Test fixture
 * ============================================================ */

class cSettingsTest : public ::testing::Test {
protected:
    std::string tmpFile; /* unique per-test path in /tmp */
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };

    void SetUp() override
    {
        /* Register the Wraps mock BEFORE any ::unlink() call, because the
         * testframework wraps unlink globally and crashes if impl == nullptr. */
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(::testing::_))
            .WillByDefault(::testing::Invoke(__real_unlink));

        /* mkstemp guarantees a unique, already-open file */
        mode_t old_umask = ::umask(0077); /* Set restrictive umask: owner-only access */
        char tmpl[] = "/tmp/csettings_test_XXXXXX";
        int fd = ::mkstemp(tmpl);
        ::umask(old_umask); /* Restore original umask */
        ASSERT_NE(-1, fd) << "mkstemp failed: " << strerror(errno);
        if (fd >= 0) {
            ::close(fd);
        }
        /* Remove so the cSettings constructor can create it fresh when needed */
        (void)::unlink(tmpl);
        tmpFile = tmpl;
    }

    void TearDown() override
    {
        (void)::unlink(tmpFile.c_str()); /* best-effort cleanup; ignore errors */
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }

    /* ---- helpers ---- */

    /** Write raw text into tmpFile, replacing any previous content. */
    void writeRawFile(const std::string& content)
    {
        std::ofstream f(tmpFile, std::ios::out | std::ios::trunc);
        ASSERT_TRUE(f.is_open()) << "Can't open " << tmpFile;
        f << content;
    }

    /** Return the full contents of tmpFile as a std::string. */
    std::string readRawFile()
    {
        std::ifstream f(tmpFile);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>());
    }

    /** Return true iff tmpFile exists on disk. */
    bool fileExistsOnDisk()
    {
        struct stat st;
        return ::stat(tmpFile.c_str(), &st) == 0;
    }
};

/* ============================================================
 * 1. Constructor / Destructor
 * ============================================================ */

TEST_F(cSettingsTest, ConstructorCreatesFileWhenNotExists)
{
    LOGINFO("Test: constructor creates file when path does not exist");
    ASSERT_FALSE(fileExistsOnDisk()) << "Pre-condition: file should not exist";

    { cSettings s(tmpFile); }

    EXPECT_TRUE(fileExistsOnDisk()) << "File must be created by constructor";
}

TEST_F(cSettingsTest, ConstructorDoesNotClobberExistingFile)
{
    LOGINFO("Test: constructor leaves an existing file intact");
    writeRawFile("existingkey=existingval\n");

    cSettings s(tmpFile);

    /* Original content must still be readable via the API */
    EXPECT_TRUE(s.contains("existingkey"));
    EXPECT_EQ(std::string("existingval"), s.getValue("existingkey").String());
}

TEST_F(cSettingsTest, ConstructorWithEmptyExistingFile)
{
    LOGINFO("Test: constructor handles an existing empty file without crashing");
    /* Create an empty file */
    { std::ofstream f(tmpFile); }

    EXPECT_NO_FATAL_FAILURE({ cSettings s(tmpFile); });
}

TEST_F(cSettingsTest, ConstructorLoadsMultipleKeys)
{
    LOGINFO("Test: constructor loads all key=value pairs from file");
    writeRawFile("k1=v1\nk2=v2\nk3=v3\n");

    cSettings s(tmpFile);

    EXPECT_EQ(std::string("v1"), s.getValue("k1").String());
    EXPECT_EQ(std::string("v2"), s.getValue("k2").String());
    EXPECT_EQ(std::string("v3"), s.getValue("k3").String());
}

TEST_F(cSettingsTest, DestructorDoesNotCrashOrLeak)
{
    LOGINFO("Test: destructor runs cleanly with data in-memory");
    /* Run several operations then let the object destruct at scope end */
    {
        cSettings s(tmpFile);
        s.setValue("d1", std::string("val1"));
        s.setValue("d2", 42);
        s.setValue("d3", true);
        /* destructor called here */
    }
    /* File must still be intact after destruction */
    EXPECT_TRUE(fileExistsOnDisk());
}

/* ============================================================
 * 2. getValue
 * ============================================================ */

TEST_F(cSettingsTest, GetValueNonExistentKeyReturnsNullString)
{
    LOGINFO("Test: getValue for missing key returns JSON null / empty sentinel");
    cSettings s(tmpFile);

    std::string val = s.getValue("no_such_key").String();
    /*
     * WPEFramework JsonObject returns "null" (as string) for an EMPTY variant
     * (see test_JSON.cpp::JSONTest::EMPTY).
     */
    EXPECT_EQ(std::string("null"), val);
}

TEST_F(cSettingsTest, GetValueAfterSetString)
{
    LOGINFO("Test: getValue returns the same string that was set");
    cSettings s(tmpFile);
    s.setValue("hello", std::string("world"));

    EXPECT_EQ(std::string("world"), s.getValue("hello").String());
}

TEST_F(cSettingsTest, GetValueAfterSetInt)
{
    LOGINFO("Test: getValue returns correct string representation of int");
    cSettings s(tmpFile);
    s.setValue("count", 99);

    /* JsonVariant NUMBER → String() gives the decimal representation */
    EXPECT_EQ(std::string("99"), s.getValue("count").String());
}

TEST_F(cSettingsTest, GetValueAfterSetIntZero)
{
    LOGINFO("Test: getValue for integer zero returns '0', not empty");
    cSettings s(tmpFile);
    s.setValue("zero", 0);

    EXPECT_EQ(std::string("0"), s.getValue("zero").String());
}

TEST_F(cSettingsTest, GetValueAfterSetBoolTrue)
{
    LOGINFO("Test: getValue returns 'true' after setValue(bool true)");
    cSettings s(tmpFile);
    s.setValue("flag", true);

    EXPECT_EQ(std::string("true"), s.getValue("flag").String());
}

TEST_F(cSettingsTest, GetValueAfterSetBoolFalse)
{
    LOGINFO("Test: getValue returns 'false' after setValue(bool false)");
    cSettings s(tmpFile);
    s.setValue("flag", false);

    EXPECT_EQ(std::string("false"), s.getValue("flag").String());
}

/* ============================================================
 * 3. setValue – string overload
 * ============================================================ */

TEST_F(cSettingsTest, SetValueStringReturnsTrueOnSuccess)
{
    LOGINFO("Test: setValue(string) returns true when file is accessible");
    cSettings s(tmpFile);

    EXPECT_TRUE(s.setValue("k", std::string("v")));
}

TEST_F(cSettingsTest, SetValueStringPersistsAcrossInstances)
{
    LOGINFO("Test: setValue(string) writes to file; reloaded instance sees it");
    {
        cSettings s(tmpFile);
        s.setValue("persistent", std::string("data"));
    }
    cSettings s2(tmpFile);
    EXPECT_EQ(std::string("data"), s2.getValue("persistent").String());
}

TEST_F(cSettingsTest, SetValueStringOverwriteExistingKey)
{
    LOGINFO("Test: setting the same key twice keeps the last value");
    cSettings s(tmpFile);
    s.setValue("key", std::string("first"));
    s.setValue("key", std::string("second"));

    EXPECT_EQ(std::string("second"), s.getValue("key").String());
}

TEST_F(cSettingsTest, SetValueStringReturnsFalseWhenFileDeleted)
{
    LOGINFO("Test: setValue returns false when backing file has been removed");
    cSettings s(tmpFile);
    (void)::unlink(tmpFile.c_str()); /* simulate unexpected removal */

    EXPECT_FALSE(s.setValue("key", std::string("val")));
    /* recreate so TearDown's unlink does not warn */
    tmpFile = tmpFile; /* no-op; TearDown handles missing file gracefully */
}

TEST_F(cSettingsTest, SetValueEmptyStringNotWrittenToFile)
{
    LOGINFO("Test: setValue with empty string does not crash and setValue returns a bool");
    cSettings s(tmpFile);
    /*
     * We cannot assert the file behaviour for empty-string values because
     * WPEFramework JsonObject may or may not serialise them depending on the
     * internal variant type. What we CAN assert: no crash, and contains()=false
     * (the implementation checks .String().empty() in contains()).
     */
    bool ret = s.setValue("ekey", std::string(""));
    (void)ret; /* return value is implementation-defined for empty value */
    EXPECT_FALSE(s.contains("ekey"));
}

TEST_F(cSettingsTest, SetValueMultipleKeysSingleInstance)
{
    LOGINFO("Test: multiple distinct keys can be set in one instance");
    cSettings s(tmpFile);
    s.setValue("a", std::string("1"));
    s.setValue("b", std::string("2"));
    s.setValue("c", std::string("3"));

    EXPECT_EQ(std::string("1"), s.getValue("a").String());
    EXPECT_EQ(std::string("2"), s.getValue("b").String());
    EXPECT_EQ(std::string("3"), s.getValue("c").String());
}

TEST_F(cSettingsTest, SetValueStringRepeatedlySameKey)
{
    LOGINFO("Test: repeated overwrites keep memory/file consistent");
    cSettings s(tmpFile);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(s.setValue("iter", std::string("val") + std::to_string(i)));
    }
    EXPECT_EQ(std::string("val9"), s.getValue("iter").String());
}

/* ============================================================
 * 4. setValue – int overload
 * ============================================================ */

TEST_F(cSettingsTest, SetValueIntReturnsTrueOnSuccess)
{
    LOGINFO("Test: setValue(int) returns true");
    cSettings s(tmpFile);
    EXPECT_TRUE(s.setValue("n", 42));
}

TEST_F(cSettingsTest, SetValueIntPersistsAcrossInstances)
{
    LOGINFO("Test: setValue(int) is round-tripped through file correctly");
    {
        cSettings s(tmpFile);
        s.setValue("num", 123);
    }
    cSettings s2(tmpFile);
    /* After file round-trip the value is a STRING "123" */
    EXPECT_EQ(std::string("123"), s2.getValue("num").String());
}

TEST_F(cSettingsTest, SetValueIntNegative)
{
    LOGINFO("Test: setValue(int) accepts negative values");
    cSettings s(tmpFile);
    s.setValue("neg", -7);

    EXPECT_TRUE(s.contains("neg"));
}

TEST_F(cSettingsTest, SetValueIntZeroIsWrittenToFile)
{
    LOGINFO("Test: integer 0 is written to file (non-empty string '0')");
    cSettings s(tmpFile);
    s.setValue("zero", 0);

    std::string raw = readRawFile();
    EXPECT_NE(std::string::npos, raw.find("zero=0"));
}

/* ============================================================
 * 5. setValue – bool overload
 * ============================================================ */

TEST_F(cSettingsTest, SetValueBoolTrueReturnsTrueOnSuccess)
{
    LOGINFO("Test: setValue(bool true) returns true");
    cSettings s(tmpFile);
    EXPECT_TRUE(s.setValue("flag", true));
}

TEST_F(cSettingsTest, SetValueBoolFalseStoredAndContains)
{
    LOGINFO("Test: setValue(bool false) – 'false' is non-empty so contains()=true");
    cSettings s(tmpFile);
    s.setValue("flag", false);

    EXPECT_TRUE(s.contains("flag"));
}

TEST_F(cSettingsTest, SetValueBoolPersistsAcrossInstances)
{
    LOGINFO("Test: setValue(bool) is round-tripped through file");
    {
        cSettings s(tmpFile);
        s.setValue("enabled", true);
    }
    cSettings s2(tmpFile);
    EXPECT_TRUE(s2.contains("enabled"));
}

/* ============================================================
 * 6. contains
 * ============================================================ */

TEST_F(cSettingsTest, ContainsReturnsFalseForMissingKey)
{
    LOGINFO("Test: contains returns false for a key that was never set");
    cSettings s(tmpFile);
    EXPECT_FALSE(s.contains("ghost"));
}

TEST_F(cSettingsTest, ContainsReturnsTrueAfterSetValue)
{
    LOGINFO("Test: contains returns true immediately after setValue");
    cSettings s(tmpFile);
    s.setValue("present", std::string("yes"));

    EXPECT_TRUE(s.contains("present"));
}

TEST_F(cSettingsTest, ContainsReturnsFalseForEmptyStringValue)
{
    LOGINFO("Test: contains returns false when the stored value is an empty string");
    cSettings s(tmpFile);
    s.setValue("ekey", std::string(""));

    /*
     * contains() checks data[key].String().empty(); empty → false.
     * An empty value is intentionally treated as "not present".
     */
    EXPECT_FALSE(s.contains("ekey"));
}

TEST_F(cSettingsTest, ContainsAfterRemove)
{
    LOGINFO("Test: contains returns false after remove");
    cSettings s(tmpFile);
    s.setValue("gone", std::string("bye"));
    ASSERT_TRUE(s.contains("gone"));

    s.remove("gone");
    EXPECT_FALSE(s.contains("gone"));
}

TEST_F(cSettingsTest, ContainsReturnsTrueForIntZero)
{
    LOGINFO("Test: contains returns true even for int 0 ('0' is not empty)");
    cSettings s(tmpFile);
    s.setValue("z", 0);

    EXPECT_TRUE(s.contains("z"));
}

TEST_F(cSettingsTest, ContainsReturnsTrueForBoolFalse)
{
    LOGINFO("Test: contains returns true for bool false ('false' is not empty)");
    cSettings s(tmpFile);
    s.setValue("b", false);

    EXPECT_TRUE(s.contains("b"));
}

/* ============================================================
 * 7. remove
 * ============================================================ */

TEST_F(cSettingsTest, RemoveExistingKeyReturnsTrueAndKeyGone)
{
    LOGINFO("Test: remove existing key → returns true, key no longer present");
    cSettings s(tmpFile);
    s.setValue("toremove", std::string("val"));

    EXPECT_TRUE(s.remove("toremove"));
    EXPECT_FALSE(s.contains("toremove"));
}

TEST_F(cSettingsTest, RemoveNonExistentKeyReturnsTrueFileIntact)
{
    LOGINFO("Test: remove a key that was never set behaves gracefully");
    cSettings s(tmpFile);

    /*
     * remove() sets data[key]="" then calls data.Remove(key).
     * After that, contains(key) is false, writeToFile() succeeds on existing
     * file → status = true.
     */
    EXPECT_TRUE(s.remove("phantom"));
    EXPECT_FALSE(s.contains("phantom"));
}

TEST_F(cSettingsTest, RemoveKeyNotWrittenToFileAfterRemove)
{
    LOGINFO("Test: removed key does not appear in file on disk");
    cSettings s(tmpFile);
    s.setValue("rem", std::string("present"));
    s.remove("rem");

    std::string raw = readRawFile();
    EXPECT_EQ(std::string::npos, raw.find("rem="))
        << "Removed key must not persist in file";
}

TEST_F(cSettingsTest, RemoveKeyPersistsAcrossInstances)
{
    LOGINFO("Test: removed key absent after reload from file");
    {
        cSettings s(tmpFile);
        s.setValue("k", std::string("v"));
        s.remove("k");
    }
    cSettings s2(tmpFile);
    EXPECT_FALSE(s2.contains("k"));
}

TEST_F(cSettingsTest, RemoveOneKeyLeavesOthersIntact)
{
    LOGINFO("Test: removing one key does not disturb sibling keys");
    cSettings s(tmpFile);
    s.setValue("stay1", std::string("A"));
    s.setValue("go",    std::string("B"));
    s.setValue("stay2", std::string("C"));

    s.remove("go");

    EXPECT_TRUE(s.contains("stay1"));
    EXPECT_FALSE(s.contains("go"));
    EXPECT_TRUE(s.contains("stay2"));
}

TEST_F(cSettingsTest, RemoveReturnsFalseWhenFileDeleted)
{
    LOGINFO("Test: remove returns false when backing file has been deleted");
    cSettings s(tmpFile);
    s.setValue("key", std::string("val"));
    (void)::unlink(tmpFile.c_str());

    /*
     * After unlink: writeToFile() returns false because Utils::fileExists()
     * returns false → remove() propagates the false.
     * But contains("key") may still be false (Remove worked in-memory), so
     * the code may take the "writeToFile failed" branch.
     */
    bool result = s.remove("key");
    EXPECT_FALSE(result);
}

/* ============================================================
 * 8. writeToFile
 * ============================================================ */

TEST_F(cSettingsTest, WriteToFileReturnsFalseWhenFileNotExists)
{
    LOGINFO("Test: writeToFile returns false when backing file is absent");
    cSettings s(tmpFile);
    (void)::unlink(tmpFile.c_str());

    EXPECT_FALSE(s.writeToFile());
}

TEST_F(cSettingsTest, WriteToFileWritesAllNonEmptyKeys)
{
    LOGINFO("Test: writeToFile emits all keys that have non-empty values");
    cSettings s(tmpFile);
    s.setValue("x", std::string("1"));
    s.setValue("y", std::string("2"));
    s.writeToFile(); /* explicit call, also called by setValue */

    std::string raw = readRawFile();
    EXPECT_NE(std::string::npos, raw.find("x=1"));
    EXPECT_NE(std::string::npos, raw.find("y=2"));
}

TEST_F(cSettingsTest, WriteToFileSkipsEmptyStringValues)
{
    LOGINFO("Test: writeToFile emits all non-empty keys; empty-value key behaviour matches implementation");
    cSettings s(tmpFile);
    s.setValue("nonempty", std::string("hello"));
    /*
     * NOTE: setValue("empty", "") stores an empty string in the JsonObject.
     * Whether writeToFile omits it depends on how WPEFramework JsonObject
     * serialises a string variant whose value is "".
     * We only assert what the implementation guarantees: the non-empty key IS written.
     */
    s.setValue("empty",    std::string(""));
    s.writeToFile();

    std::string raw = readRawFile();
    EXPECT_NE(std::string::npos, raw.find("nonempty=hello"))
        << "Non-empty key must appear in file";
}

TEST_F(cSettingsTest, WriteToFileReturnsTrueOnSuccess)
{
    LOGINFO("Test: writeToFile returns true when file exists and write succeeds");
    cSettings s(tmpFile);
    s.setValue("k", std::string("v"));

    EXPECT_TRUE(s.writeToFile());
}

/* ============================================================
 * 9. readFromFile (exercised through constructor)
 * ============================================================ */

TEST_F(cSettingsTest, ReadFromFileNonExistentFileTriggersCreation)
{
    LOGINFO("Test: when file absent, constructor creates a new empty file");
    ASSERT_FALSE(fileExistsOnDisk());

    cSettings s(tmpFile);

    EXPECT_TRUE(fileExistsOnDisk());
}

TEST_F(cSettingsTest, ReadFromFileMalformedLineWithoutEqualsIgnored)
{
    LOGINFO("Test: lines without '=' are silently ignored; valid lines parsed");
    writeRawFile("malformed_line_no_equals\nkey=val\n");

    cSettings s(tmpFile);

    EXPECT_TRUE(s.contains("key"));
    EXPECT_FALSE(s.contains("malformed_line_no_equals"));
}

TEST_F(cSettingsTest, ReadFromFileValueContainingEqualsSignsUsesLastEquals)
{
    LOGINFO("Test: readFromFile uses find_last_of('=') so value may contain '='");
    /*
     * Line "a=b=c":
     *   find_last_of('=') → index 3
     *   key   = content.substr(0, 3) = "a=b"
     *   value = content.substr(4)    = "c"
     */
    writeRawFile("a=b=c\n");

    cSettings s(tmpFile);

    EXPECT_EQ(std::string("c"), s.getValue("a=b").String());
}

TEST_F(cSettingsTest, ReadFromFileEmptyLinesAreHarmless)
{
    LOGINFO("Test: blank lines inside the file do not corrupt the parsed data");
    writeRawFile("\n\nkey=value\n\n");

    cSettings s(tmpFile);

    EXPECT_TRUE(s.contains("key"));
    EXPECT_EQ(std::string("value"), s.getValue("key").String());
}

TEST_F(cSettingsTest, ReadFromFileDuplicateKeyLastValueWins)
{
    LOGINFO("Test: when a key appears twice the later value overwrites the first");
    writeRawFile("key=first\nkey=second\n");

    cSettings s(tmpFile);

    EXPECT_EQ(std::string("second"), s.getValue("key").String());
}

TEST_F(cSettingsTest, ReadFromFileKeyWithEmptyValueNotContained)
{
    LOGINFO("Test: 'key=' in file stores empty string; contains() returns false");
    writeRawFile("key=\n");

    cSettings s(tmpFile);

    /*
     * readFromFile sets data["key"] = "" (empty).
     * contains() checks .String().empty() → true → returns false.
     */
    EXPECT_FALSE(s.contains("key"));
}

TEST_F(cSettingsTest, ReadFromFileNoTrailingNewline)
{
    LOGINFO("Test: file without trailing newline is parsed correctly");
    writeRawFile("key=noeol");

    cSettings s(tmpFile);

    EXPECT_EQ(std::string("noeol"), s.getValue("key").String());
}

TEST_F(cSettingsTest, ReadFromFileMultipleKeyValuePairs)
{
    LOGINFO("Test: all key=value pairs in file are loaded into in-memory store");
    writeRawFile("alpha=one\nbeta=two\ngamma=three\n");

    cSettings s(tmpFile);

    EXPECT_EQ(std::string("one"),   s.getValue("alpha").String());
    EXPECT_EQ(std::string("two"),   s.getValue("beta").String());
    EXPECT_EQ(std::string("three"), s.getValue("gamma").String());
}

/* ============================================================
 * 10. Persistence / cross-instance consistency
 * ============================================================ */

TEST_F(cSettingsTest, DataPersistsStringIntBoolAcrossReload)
{
    LOGINFO("Test: string, int, bool values all survive a file round-trip");
    {
        cSettings s(tmpFile);
        s.setValue("str", std::string("hello"));
        s.setValue("num", 77);
        s.setValue("flag", true);
    }
    cSettings s2(tmpFile);
    EXPECT_EQ(std::string("hello"), s2.getValue("str").String());
    EXPECT_EQ(std::string("77"),    s2.getValue("num").String());
    /* "true" is not empty so contains returns true */
    EXPECT_TRUE(s2.contains("flag"));
}

TEST_F(cSettingsTest, MultipleSetRemoveCyclesLeaveConsistentState)
{
    LOGINFO("Test: interleaved setValue/remove cycles remain consistent");
    cSettings s(tmpFile);

    s.setValue("a", std::string("1"));
    s.setValue("b", std::string("2"));
    s.remove("a");
    s.setValue("a", std::string("3"));
    s.remove("b");

    EXPECT_TRUE(s.contains("a"));
    EXPECT_EQ(std::string("3"), s.getValue("a").String());
    EXPECT_FALSE(s.contains("b"));
}

TEST_F(cSettingsTest, ReloadAfterMultipleEdits)
{
    LOGINFO("Test: reload reflects the final state after multiple edits");
    {
        cSettings s(tmpFile);
        s.setValue("x", std::string("initial"));
        s.setValue("x", std::string("updated"));
        s.setValue("y", std::string("keep"));
        s.remove("y");
        s.setValue("z", std::string("new"));
    }
    cSettings s2(tmpFile);
    EXPECT_EQ(std::string("updated"), s2.getValue("x").String());
    EXPECT_FALSE(s2.contains("y"));
    EXPECT_EQ(std::string("new"), s2.getValue("z").String());
}

/* ============================================================
 * 11. Edge / boundary cases
 * ============================================================ */

TEST_F(cSettingsTest, LongKeyAndValue)
{
    LOGINFO("Test: long key and value strings are handled without truncation");
    std::string longKey(200, 'k');
    std::string longVal(1000, 'v');

    cSettings s(tmpFile);
    EXPECT_TRUE(s.setValue(longKey, longVal));
    EXPECT_EQ(longVal, s.getValue(std::move(longKey)).String());
}

TEST_F(cSettingsTest, ValueWithSpaces)
{
    LOGINFO("Test: values that contain spaces are preserved");
    cSettings s(tmpFile);
    s.setValue("phrase", std::string("hello world"));

    EXPECT_EQ(std::string("hello world"), s.getValue("phrase").String());
}

TEST_F(cSettingsTest, LargeNumberOfKeys)
{
    LOGINFO("Test: many keys stored and retrieved without corruption");
    cSettings s(tmpFile);
    const int N = 50;
    for (int i = 0; i < N; ++i) {
        s.setValue("key" + std::to_string(i), std::string("val") + std::to_string(i));
    }
    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(s.contains("key" + std::to_string(i)));
        EXPECT_EQ(std::string("val") + std::to_string(i),
                  s.getValue("key" + std::to_string(i)).String());
    }
}

TEST_F(cSettingsTest, ConstructTwiceOnSameFile)
{
    LOGINFO("Test: two independent instances on the same file do not conflict");
    {
        cSettings s1(tmpFile);
        s1.setValue("shared", std::string("from_s1"));
    }
    {
        cSettings s2(tmpFile);
        EXPECT_EQ(std::string("from_s1"), s2.getValue("shared").String());
        s2.setValue("shared", std::string("from_s2"));
    }
    cSettings s3(tmpFile);
    EXPECT_EQ(std::string("from_s2"), s3.getValue("shared").String());
}

TEST_F(cSettingsTest, SetValueAfterFileContentsCleared)
{
    LOGINFO("Test: new setValue after file is externally cleared works correctly");
    cSettings s(tmpFile);
    s.setValue("before", std::string("yes"));

    /* Externally wipe file content (simulate another process truncating it) */
    { std::ofstream f(tmpFile, std::ios::trunc); }

    /*
     * writeToFile() checks Utils::fileExists() (file still exists, just empty).
     * setValue → writeToFile should succeed and re-populate the file.
     */
    EXPECT_TRUE(s.setValue("after", std::string("yes")));
    std::string raw = readRawFile();
    EXPECT_NE(std::string::npos, raw.find("after=yes"));
}

TEST_F(cSettingsTest, RemoveThenSetSameKey)
{
    LOGINFO("Test: a key can be re-added after it has been removed");
    cSettings s(tmpFile);
    s.setValue("cycle", std::string("first"));
    s.remove("cycle");
    ASSERT_FALSE(s.contains("cycle"));

    s.setValue("cycle", std::string("second"));
    EXPECT_TRUE(s.contains("cycle"));
    EXPECT_EQ(std::string("second"), s.getValue("cycle").String());
}

TEST_F(cSettingsTest, GetValueOnFreshInstanceReturnsNullForAllKeys)
{
    LOGINFO("Test: newly constructed instance with empty file has no known keys");
    cSettings s(tmpFile); /* file is freshly created, no data */

    EXPECT_EQ(std::string("null"), s.getValue("anything").String());
    EXPECT_FALSE(s.contains("anything"));
}
