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
#include "UtilsCStr.h"

// ---------------------------------------------------------------------------
// C_STR macro — must expand to .c_str() on a std::string
// ---------------------------------------------------------------------------
TEST(UtilsCStrTest, CStr_BasicString_ReturnsCString)
{
    std::string s = "hello";
    const char* cs = C_STR(s);
    EXPECT_STREQ(cs, "hello");
}

TEST(UtilsCStrTest, CStr_EmptyString_ReturnEmptyCharPtr)
{
    std::string s = "";
    const char* cs = C_STR(s);
    EXPECT_STREQ(cs, "");
}

TEST(UtilsCStrTest, CStr_StringWithSpaces)
{
    std::string s = "hello world";
    EXPECT_STREQ(C_STR(s), "hello world");
}

TEST(UtilsCStrTest, CStr_ResultIsNullTerminated)
{
    std::string s = "abc";
    const char* cs = C_STR(s);
    EXPECT_EQ(cs[3], '\0');
}

TEST(UtilsCStrTest, CStr_SpecialCharacters)
{
    std::string s = "line1\nline2\ttab";
    const char* cs = C_STR(s);
    EXPECT_EQ(std::string(cs), s);
}
