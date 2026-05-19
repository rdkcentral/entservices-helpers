/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
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

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Module.h"
#include "WrapsMock.h"
#include "UtilsString.h"

#include <fstream>
#include <string>
#include <vector>

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

// __real_unlink is the genuine libc unlink, bypassing the --wrap redirect.
extern "C" int __real_unlink(const char* path);

// ---------------------------------------------------------------------------
// Pure string tests — no file I/O, no LOGINFO, no mocks needed
// ---------------------------------------------------------------------------

TEST(UtilsStringTest, FindSubstrCiCaseInsensitiveMatch)
{
    std::string haystack = "Hello World";
    std::string needle   = "world";
    EXPECT_EQ(6, Utils::String::find_substr_ci(haystack, needle));
}

TEST(UtilsStringTest, FindSubstrCiNotFound)
{
    std::string haystack = "Hello World";
    std::string needle   = "xyz";
    EXPECT_EQ(-1, Utils::String::find_substr_ci(haystack, needle));
}

TEST(UtilsStringTest, ContainsString)
{
    std::string s    = "FooBarBaz";
    std::string sub  = "bar";
    EXPECT_TRUE(Utils::String::contains(s, sub));
}

TEST(UtilsStringTest, ContainsStringNotPresent)
{
    std::string s   = "FooBarBaz";
    std::string sub = "qux";
    EXPECT_FALSE(Utils::String::contains(s, sub));
}

TEST(UtilsStringTest, ContainsCString)
{
    std::string s = "FooBarBaz";
    EXPECT_TRUE(Utils::String::contains(s, "BAZ"));
}

TEST(UtilsStringTest, EqualStrings)
{
    std::string a = "Hello";
    std::string b = "HELLO";
    EXPECT_TRUE(Utils::String::equal(a, b));
}

TEST(UtilsStringTest, EqualStringsDifferent)
{
    std::string a = "Hello";
    std::string b = "World";
    EXPECT_FALSE(Utils::String::equal(a, b));
}

TEST(UtilsStringTest, EqualCString)
{
    std::string a = "hello";
    EXPECT_TRUE(Utils::String::equal(a, "HELLO"));
}

TEST(UtilsStringTest, LtrimRemovesLeadingSpaces)
{
    std::string s = "   hello";
    Utils::String::ltrim(s);
    EXPECT_EQ("hello", s);
}

TEST(UtilsStringTest, LtrimNoLeadingSpaces)
{
    std::string s = "hello";
    Utils::String::ltrim(s);
    EXPECT_EQ("hello", s);
}

TEST(UtilsStringTest, RtrimRemovesTrailingSpaces)
{
    std::string s = "hello   ";
    Utils::String::rtrim(s);
    EXPECT_EQ("hello", s);
}

TEST(UtilsStringTest, TrimRemovesBothEnds)
{
    std::string s = "  hello  ";
    Utils::String::trim(s);
    EXPECT_EQ("hello", s);
}

TEST(UtilsStringTest, TrimAllWhitespaceBecomesEmpty)
{
    std::string s = "   ";
    Utils::String::trim(s);
    EXPECT_EQ("", s);
}

TEST(UtilsStringTest, ToUpper)
{
    std::string s = "hello world";
    Utils::String::toUpper(s);
    EXPECT_EQ("HELLO WORLD", s);
}

TEST(UtilsStringTest, ToLower)
{
    std::string s = "HELLO WORLD";
    Utils::String::toLower(s);
    EXPECT_EQ("hello world", s);
}

TEST(UtilsStringTest, StringContainsFound)
{
    EXPECT_TRUE(Utils::String::stringContains("FooBarBaz", "BAR"));
}

TEST(UtilsStringTest, StringContainsNotFound)
{
    EXPECT_FALSE(Utils::String::stringContains("FooBarBaz", "qux"));
}

TEST(UtilsStringTest, StringContainsCString)
{
    EXPECT_TRUE(Utils::String::stringContains("Hello World", "WORLD"));
}

TEST(UtilsStringTest, SplitBasic)
{
    std::vector<std::string> parts;
    std::string s = "a,b,c";
    Utils::String::split(parts, s, ",");
    ASSERT_EQ(3u, parts.size());
    EXPECT_EQ("a", parts[0]);
    EXPECT_EQ("b", parts[1]);
    EXPECT_EQ("c", parts[2]);
}

TEST(UtilsStringTest, SplitNoDelimiterFound)
{
    std::vector<std::string> parts;
    std::string s = "abc";
    Utils::String::split(parts, s, ",");
    ASSERT_EQ(1u, parts.size());
    EXPECT_EQ("abc", parts[0]);
}

TEST(UtilsStringTest, SplitMultipleDelimiters)
{
    std::vector<std::string> parts;
    std::string s = "a:b;c";
    Utils::String::split(parts, s, ":;");
    ASSERT_EQ(3u, parts.size());
    EXPECT_EQ("a", parts[0]);
    EXPECT_EQ("b", parts[1]);
    EXPECT_EQ("c", parts[2]);
}

TEST(UtilsStringTest, ImageEncoderEmptyInput)
{
    std::string result;
    Utils::String::imageEncoder(nullptr, 0, true, result);
    EXPECT_EQ("", result);
}

TEST(UtilsStringTest, ImageEncoderThreeBytesPaddingNotNeeded)
{
    // "Man" encodes to "TWFu" in base64
    const uint8_t input[] = { 'M', 'a', 'n' };
    std::string result;
    Utils::String::imageEncoder(input, 3, true, result);
    EXPECT_EQ("TWFu", result);
}

TEST(UtilsStringTest, ImageEncoderTwoBytesWithPadding)
{
    // "Ma" encodes to "TWE=" in base64
    const uint8_t input[] = { 'M', 'a' };
    std::string result;
    Utils::String::imageEncoder(input, 2, true, result);
    EXPECT_EQ("TWE=", result);
}

TEST(UtilsStringTest, ImageEncoderOneByteWithPadding)
{
    // "M" encodes to "TQ==" in base64
    const uint8_t input[] = { 'M' };
    std::string result;
    Utils::String::imageEncoder(input, 1, true, result);
    EXPECT_EQ("TQ==", result);
}

TEST(UtilsStringTest, ImageEncoderTwoBytesNoPadding)
{
    const uint8_t input[] = { 'M', 'a' };
    std::string result;
    Utils::String::imageEncoder(input, 2, false, result);
    EXPECT_EQ("TWE", result); // no '=' appended when padding=false
}

TEST(UtilsStringTest, RemoveExtraWhitespacesEmpty)
{
    std::string in  = "";
    std::string out = "";
    EXPECT_FALSE(Utils::String::removeExtraWhitespaces(in, out));
    EXPECT_EQ("", out);
}

TEST(UtilsStringTest, RemoveExtraWhitespacesNoneToRemove)
{
    std::string in  = "hello world";
    std::string out = "";
    EXPECT_TRUE(Utils::String::removeExtraWhitespaces(in, out));
    EXPECT_EQ("hello world", out);
}

TEST(UtilsStringTest, RemoveExtraWhitespacesDoubleSpace)
{
    std::string in  = "hello  world";
    std::string out = "";
    EXPECT_TRUE(Utils::String::removeExtraWhitespaces(in, out));
    EXPECT_EQ("hello world", out);
}

TEST(UtilsStringTest, RemoveExtraWhitespacesMultipleSpaces)
{
    std::string in  = "a   b   c";
    std::string out = "";
    EXPECT_TRUE(Utils::String::removeExtraWhitespaces(in, out));
    EXPECT_EQ("a b c", out);
}

TEST(UtilsStringTest, ReplaceStringBasic)
{
    EXPECT_EQ("hello there", Utils::String::replaceString("hello world", "world", "there"));
}

TEST(UtilsStringTest, ReplaceStringAllOccurrences)
{
    EXPECT_EQ("bbb", Utils::String::replaceString("aaa", "a", "b"));
}

TEST(UtilsStringTest, ReplaceStringEmptyOldStringReturnsUnchanged)
{
    EXPECT_EQ("hello", Utils::String::replaceString("hello", "", "x"));
}

TEST(UtilsStringTest, ReplaceStringNotFound)
{
    EXPECT_EQ("hello", Utils::String::replaceString("hello", "world", "x"));
}

TEST(UtilsStringTest, ReplaceStringMultipleOccurrences)
{
    EXPECT_EQ("xyxy", Utils::String::replaceString("abab", "ab", "xy"));
}

// ---------------------------------------------------------------------------
// File-based tests — updateSystemModeFile / getSystemModePropertyValue
// These call LOGINFO (-> syslog wrapped -> EXPECT_NE) so need WrapsImplMock.
// They also write /tmp/SystemMode.txt which must be cleaned up via ::unlink
// (unlink is wrapped, so also needs WrapsImplMock).
// ---------------------------------------------------------------------------

class UtilsStringFileTest : public ::testing::Test {
protected:
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };

    void SetUp() override
    {
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(_))
            .WillByDefault(Invoke(__real_unlink));
        // Ensure a clean starting state
        ::unlink(SYSTEM_MODE_FILE);
    }

    void TearDown() override
    {
        ::unlink(SYSTEM_MODE_FILE);
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }
};

TEST_F(UtilsStringFileTest, UpdateSystemModeFileEmptySystemModeReturnsEarly)
{
    // Empty systemMode: function returns immediately (LOGINFO + return)
    Utils::String::updateSystemModeFile("", "currentstate", "VIDEO", "add");
    // File should not have been created
    std::ifstream f(SYSTEM_MODE_FILE);
    EXPECT_FALSE(f.good());
}

TEST_F(UtilsStringFileTest, UpdateSystemModeFileInvalidActionReturnsEarly)
{
    Utils::String::updateSystemModeFile("MODE", "currentstate", "VIDEO", "invalidaction");
    std::ifstream f(SYSTEM_MODE_FILE);
    EXPECT_FALSE(f.good());
}

TEST_F(UtilsStringFileTest, UpdateSystemModeFileAddCreatesFile)
{
    Utils::String::updateSystemModeFile("MYMODE", "currentstate", "VIDEO", "add");
    std::ifstream f(SYSTEM_MODE_FILE);
    EXPECT_TRUE(f.good());
}

TEST_F(UtilsStringFileTest, GetSystemModePropertyValueAfterAdd)
{
    // Add a value and read it back
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "AUDIO", "add");
    std::string value;
    bool found = Utils::String::getSystemModePropertyValue("TESTMODE", "currentstate", value);
    EXPECT_TRUE(found);
    EXPECT_EQ("AUDIO", value);
}

TEST_F(UtilsStringFileTest, UpdateSystemModeFileCheckAndAdd)
{
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "VIDEO", "checkandadd");
    std::string value;
    bool found = Utils::String::getSystemModePropertyValue("TESTMODE", "currentstate", value);
    EXPECT_TRUE(found);
    EXPECT_EQ("VIDEO", value);
}

TEST_F(UtilsStringFileTest, UpdateSystemModeFileAddReplacesExistingValue)
{
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "VIDEO", "add");
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "AUDIO", "add");
    std::string value;
    bool found = Utils::String::getSystemModePropertyValue("TESTMODE", "currentstate", value);
    EXPECT_TRUE(found);
    EXPECT_EQ("AUDIO", value);
}

TEST_F(UtilsStringFileTest, UpdateSystemModeFileDeleteClearsCurrentstate)
{
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "VIDEO", "add");
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "", "delete");
    std::string value;
    bool found = Utils::String::getSystemModePropertyValue("TESTMODE", "currentstate", value);
    // After delete the line is cleared so value should be empty or not found
    EXPECT_FALSE(found);
}

TEST_F(UtilsStringFileTest, UpdateSystemModeFileDeleteAllRemovesLine)
{
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "VIDEO", "add");
    Utils::String::updateSystemModeFile("TESTMODE", "currentstate", "", "deleteall");
    std::string value;
    bool found = Utils::String::getSystemModePropertyValue("TESTMODE", "currentstate", value);
    EXPECT_FALSE(found);
}

TEST_F(UtilsStringFileTest, GetSystemModePropertyValueEmptySystemModeReturnsFalse)
{
    std::string value;
    EXPECT_FALSE(Utils::String::getSystemModePropertyValue("", "currentstate", value));
}

TEST_F(UtilsStringFileTest, GetSystemModePropertyValueFileNotExistsReturnsFalse)
{
    std::string value;
    EXPECT_FALSE(Utils::String::getSystemModePropertyValue("MISSINGMODE", "currentstate", value));
}
