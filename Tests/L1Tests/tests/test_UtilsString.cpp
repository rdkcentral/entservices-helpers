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

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "UtilsString.h"

using namespace Utils::String;

// ===========================================================================
// find_substr_ci
// ===========================================================================

TEST(UtilsStringFindSubstrCITest, FoundAtStart)
{
    std::string s = "Hello World";
    std::string sub = "hello";
    EXPECT_EQ(find_substr_ci(s, sub), 0);
}

TEST(UtilsStringFindSubstrCITest, FoundInMiddle)
{
    std::string s = "say Hello there";
    std::string sub = "HELLO";
    EXPECT_EQ(find_substr_ci(s, sub), 4);
}

TEST(UtilsStringFindSubstrCITest, FoundAtEnd)
{
    std::string s = "say hello";
    std::string sub = "HELLO";
    EXPECT_EQ(find_substr_ci(s, sub), 4);
}

TEST(UtilsStringFindSubstrCITest, NotFound_ReturnsMinusOne)
{
    std::string s = "Hello World";
    std::string sub = "xyz";
    EXPECT_EQ(find_substr_ci(s, sub), -1);
}

TEST(UtilsStringFindSubstrCITest, EmptySubstring_ReturnsZero)
{
    std::string s = "Hello";
    std::string sub = "";
    EXPECT_EQ(find_substr_ci(s, sub), 0);
}

TEST(UtilsStringFindSubstrCITest, EmptyString_NotFound)
{
    std::string s = "";
    std::string sub = "a";
    EXPECT_EQ(find_substr_ci(s, sub), -1);
}

TEST(UtilsStringFindSubstrCITest, BothEmpty_ReturnsZero)
{
    std::string s = "";
    std::string sub = "";
    EXPECT_EQ(find_substr_ci(s, sub), 0);
}

// ===========================================================================
// contains (template<T>, T overload)
// ===========================================================================

TEST(UtilsStringContainsTest, ContainsSubstring_ReturnsTrue)
{
    std::string s = "Hello World";
    std::string sub = "world";
    EXPECT_TRUE(contains(s, sub));
}

TEST(UtilsStringContainsTest, DoesNotContain_ReturnsFalse)
{
    std::string s = "Hello World";
    std::string sub = "xyz";
    EXPECT_FALSE(contains(s, sub));
}

TEST(UtilsStringContainsTest, CaseInsensitive_ReturnsTrue)
{
    std::string s = "RDK management";
    std::string sub = "MANAGEMENT";
    EXPECT_TRUE(contains(s, sub));
}

TEST(UtilsStringContainsTest, EmptySubstring_ReturnsTrue)
{
    std::string s = "hello";
    std::string sub = "";
    EXPECT_TRUE(contains(s, sub));
}

// contains(T, const char*) overload
TEST(UtilsStringContainsTest, CStrOverload_Found_ReturnsTrue)
{
    std::string s = "grep -i SomeProcess";
    EXPECT_TRUE(contains(s, "grep -i"));
}

TEST(UtilsStringContainsTest, CStrOverload_NotFound_ReturnsFalse)
{
    std::string s = "grep -i SomeProcess";
    EXPECT_FALSE(contains(s, "awk"));
}

// ===========================================================================
// equal
// ===========================================================================

TEST(UtilsStringEqualTest, EqualStrings_ReturnsTrue)
{
    std::string s1 = "Hello";
    std::string s2 = "Hello";
    EXPECT_TRUE(equal(s1, s2));
}

TEST(UtilsStringEqualTest, EqualCaseInsensitive_ReturnsTrue)
{
    std::string s1 = "CRYPTANIUM";
    std::string s2 = "cryptanium";
    EXPECT_TRUE(equal(s1, s2));
}

TEST(UtilsStringEqualTest, DifferentLength_ReturnsFalse)
{
    std::string s1 = "Hello";
    std::string s2 = "Hello World";
    EXPECT_FALSE(equal(s1, s2));
}

TEST(UtilsStringEqualTest, DifferentContent_ReturnsFalse)
{
    std::string s1 = "Hello";
    std::string s2 = "World";
    EXPECT_FALSE(equal(s1, s2));
}

TEST(UtilsStringEqualTest, BothEmpty_ReturnsTrue)
{
    std::string s1 = "";
    std::string s2 = "";
    EXPECT_TRUE(equal(s1, s2));
}

// equal(T, const char*) overload
TEST(UtilsStringEqualTest, CStrOverload_Equal_ReturnsTrue)
{
    std::string s = "CRYPTANIUM";
    EXPECT_TRUE(equal(s, "CRYPTANIUM"));
}

TEST(UtilsStringEqualTest, CStrOverload_CaseInsensitive_ReturnsTrue)
{
    std::string s = "TV";
    EXPECT_TRUE(equal(s, "tv"));
}

TEST(UtilsStringEqualTest, CStrOverload_DifferentContent_ReturnsFalse)
{
    std::string s = "STB";
    EXPECT_FALSE(equal(s, "TV"));
}

// ===========================================================================
// ltrim
// ===========================================================================

TEST(UtilsStringTrimTest, LtrimRemovesLeadingSpaces)
{
    std::string s = "   hello";
    ltrim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, LtrimRemovesLeadingNewlines)
{
    std::string s = "\n\t  hello";
    ltrim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, LtrimNoLeadingSpaces_NoChange)
{
    std::string s = "hello";
    ltrim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, LtrimEmptyString_NoChange)
{
    std::string s = "";
    ltrim(s);
    EXPECT_EQ(s, "");
}

TEST(UtilsStringTrimTest, LtrimAllSpaces_EmptyResult)
{
    std::string s = "   ";
    ltrim(s);
    EXPECT_EQ(s, "");
}

// ===========================================================================
// rtrim
// ===========================================================================

TEST(UtilsStringTrimTest, RtrimRemovesTrailingSpaces)
{
    std::string s = "hello   ";
    rtrim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, RtrimRemovesTrailingNewlines)
{
    std::string s = "hello\n\r";
    rtrim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, RtrimNoTrailingSpaces_NoChange)
{
    std::string s = "hello";
    rtrim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, RtrimEmptyString_NoChange)
{
    std::string s = "";
    rtrim(s);
    EXPECT_EQ(s, "");
}

// ===========================================================================
// trim
// ===========================================================================

TEST(UtilsStringTrimTest, TrimBothSides)
{
    std::string s = "  hello world  ";
    trim(s);
    EXPECT_EQ(s, "hello world");
}

TEST(UtilsStringTrimTest, TrimOnlyLeft)
{
    std::string s = "   hello";
    trim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, TrimOnlyRight)
{
    std::string s = "hello   ";
    trim(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringTrimTest, TrimEmpty_NoChange)
{
    std::string s = "";
    trim(s);
    EXPECT_EQ(s, "");
}

TEST(UtilsStringTrimTest, TrimAllWhitespace_EmptyResult)
{
    std::string s = "  \t\n  ";
    trim(s);
    EXPECT_EQ(s, "");
}

TEST(UtilsStringTrimTest, TrimNoWhitespace_NoChange)
{
    std::string s = "hello";
    trim(s);
    EXPECT_EQ(s, "hello");
}

// ===========================================================================
// toUpper
// ===========================================================================

TEST(UtilsStringCaseTest, ToUpper_AllLower)
{
    std::string s = "hello";
    toUpper(s);
    EXPECT_EQ(s, "HELLO");
}

TEST(UtilsStringCaseTest, ToUpper_MixedCase)
{
    std::string s = "HeLLo WoRlD";
    toUpper(s);
    EXPECT_EQ(s, "HELLO WORLD");
}

TEST(UtilsStringCaseTest, ToUpper_AlreadyUpper)
{
    std::string s = "HELLO";
    toUpper(s);
    EXPECT_EQ(s, "HELLO");
}

TEST(UtilsStringCaseTest, ToUpper_EmptyString)
{
    std::string s = "";
    toUpper(s);
    EXPECT_EQ(s, "");
}

TEST(UtilsStringCaseTest, ToUpper_WithDigitsAndSpecialChars)
{
    std::string s = "abc123!@#";
    toUpper(s);
    EXPECT_EQ(s, "ABC123!@#");
}

// ===========================================================================
// toLower
// ===========================================================================

TEST(UtilsStringCaseTest, ToLower_AllUpper)
{
    std::string s = "HELLO";
    toLower(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringCaseTest, ToLower_MixedCase)
{
    std::string s = "HeLLo WoRlD";
    toLower(s);
    EXPECT_EQ(s, "hello world");
}

TEST(UtilsStringCaseTest, ToLower_AlreadyLower)
{
    std::string s = "hello";
    toLower(s);
    EXPECT_EQ(s, "hello");
}

TEST(UtilsStringCaseTest, ToLower_EmptyString)
{
    std::string s = "";
    toLower(s);
    EXPECT_EQ(s, "");
}

// ===========================================================================
// stringContains (inline, case-insensitive search)
// ===========================================================================

TEST(UtilsStringContainsInlineTest, ContainsSubstr_ReturnsTrue)
{
    EXPECT_TRUE(stringContains("Hello World", "world"));
}

TEST(UtilsStringContainsInlineTest, ContainsCaseSensitiveMatch_ReturnsTrue)
{
    EXPECT_TRUE(stringContains("Hello World", "Hello"));
}

TEST(UtilsStringContainsInlineTest, DoesNotContain_ReturnsFalse)
{
    EXPECT_FALSE(stringContains("Hello World", "xyz"));
}

TEST(UtilsStringContainsInlineTest, EmptySubstr_ReturnsTrue)
{
    EXPECT_TRUE(stringContains("hello", ""));
}

TEST(UtilsStringContainsInlineTest, EmptyString_EmptySubstr_ReturnsTrue)
{
    EXPECT_TRUE(stringContains("", ""));
}

// CStr overload
TEST(UtilsStringContainsInlineTest, CStrOverload_Found_ReturnsTrue)
{
    EXPECT_TRUE(stringContains(std::string("Hello World"), "WORLD"));
}

TEST(UtilsStringContainsInlineTest, CStrOverload_NotFound_ReturnsFalse)
{
    EXPECT_FALSE(stringContains(std::string("Hello"), "xyz"));
}

// ===========================================================================
// split
// ===========================================================================

TEST(UtilsStringSplitTest, SplitByComma)
{
    std::vector<std::string> result;
    std::string s = "a,b,c";
    split(result, s, ",");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsStringSplitTest, SplitBySlash)
{
    std::vector<std::string> result;
    std::string s = "/usr/local/bin";
    split(result, s, "/");
    // results include empty first element for leading /
    EXPECT_GE(result.size(), 3u);
}

TEST(UtilsStringSplitTest, SplitNoDelimiter_SingleElement)
{
    std::vector<std::string> result;
    std::string s = "hello";
    split(result, s, ",");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello");
}

TEST(UtilsStringSplitTest, SplitMultipleDelimiters)
{
    std::vector<std::string> result;
    std::string s = "a,b;c";
    split(result, s, ",;");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsStringSplitTest, SplitEmptyString_ProducesOneEmptyToken)
{
    std::vector<std::string> result;
    std::string s = "";
    split(result, s, ",");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "");
}

TEST(UtilsStringSplitTest, SplitConsecutiveDelimiters_EmptyTokens)
{
    std::vector<std::string> result;
    std::string s = "a,,b";
    split(result, s, ",");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "");
    EXPECT_EQ(result[2], "b");
}

// ===========================================================================
// imageEncoder (base64)
// ===========================================================================

TEST(UtilsStringImageEncoderTest, SingleByteInput)
{
    uint8_t data[] = {0x4D}; // 'M'
    std::string result;
    imageEncoder(data, 1, true, result);
    EXPECT_EQ(result, "TQ==");
}

TEST(UtilsStringImageEncoderTest, TwoByteInput)
{
    uint8_t data[] = {0x4D, 0x61}; // "Ma"
    std::string result;
    imageEncoder(data, 2, true, result);
    EXPECT_EQ(result, "TWE=");
}

TEST(UtilsStringImageEncoderTest, ThreeByteInput_ManString)
{
    uint8_t data[] = {0x4D, 0x61, 0x6E}; // "Man"
    std::string result;
    imageEncoder(data, 3, true, result);
    EXPECT_EQ(result, "TWFu");
}

TEST(UtilsStringImageEncoderTest, EmptyInput_EmptyResult)
{
    std::string result;
    imageEncoder(nullptr, 0, true, result);
    EXPECT_EQ(result, "");
}

TEST(UtilsStringImageEncoderTest, NoPadding_TwoBytes)
{
    uint8_t data[] = {0x4D, 0x61};
    std::string result;
    imageEncoder(data, 2, false, result);
    // Without padding the trailing '=' should not appear
    EXPECT_EQ(result.find('='), std::string::npos);
}

TEST(UtilsStringImageEncoderTest, MultipleThreeByteGroups)
{
    // "Hello!" = 0x48 0x65 0x6C 0x6C 0x6F 0x21
    uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21};
    std::string result;
    imageEncoder(data, 6, true, result);
    EXPECT_EQ(result, "SGVsbG8h");
}
