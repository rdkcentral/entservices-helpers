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
#include "UtilsFile.h"

// getLastLine is pure C++ (no WPEFramework dependency in its body)
// MoveFile requires WPEFramework::Core::File — tested separately

// ===========================================================================
// getLastLine
// ===========================================================================

TEST(UtilsFileGetLastLineTest, SingleNonEmptyLine_ReturnsThatLine)
{
    std::string input = "hello";
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "hello");
}

TEST(UtilsFileGetLastLineTest, MultipleLines_ReturnsLastNonEmpty)
{
    std::string input = "first\nsecond\nthird";
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "third");
}

TEST(UtilsFileGetLastLineTest, TrailingNewline_ReturnsLastNonEmptyLine)
{
    std::string input = "first\nsecond\nthird\n";
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "third");
}

TEST(UtilsFileGetLastLineTest, EmptyInput_ReturnsFalse)
{
    std::string input = "";
    std::string result = "unchanged";
    EXPECT_FALSE(Utils::getLastLine(input, result));
}

TEST(UtilsFileGetLastLineTest, OnlyNewlines_ReturnsFalse)
{
    std::string input = "\n\n\n";
    std::string result = "unchanged";
    EXPECT_FALSE(Utils::getLastLine(input, result));
}

TEST(UtilsFileGetLastLineTest, CarriageReturnLines_ReturnsLastNonEmpty)
{
    // Simulate Windows-style line endings converted by 'tr'
    std::string input = "line1\rline2\rline3";
    std::string result;
    // getLastLine splits on '\n'; '\r'-separated content is one "line"
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "line1\rline2\rline3");
}

TEST(UtilsFileGetLastLineTest, MixedEmptyAndNonEmptyLines_ReturnsLastNonEmpty)
{
    std::string input = "alpha\n\nbeta\n\n";
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "beta");
}

TEST(UtilsFileGetLastLineTest, SingleCharacterLine)
{
    std::string input = "X\n";
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "X");
}

TEST(UtilsFileGetLastLineTest, LineWithSpaces_Preserved)
{
    std::string input = "  spaced line  \n";
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, "  spaced line  ");
}

TEST(UtilsFileGetLastLineTest, LongLastLine)
{
    std::string longLine(1024, 'z');
    std::string input = "short\n" + longLine;
    std::string result;
    EXPECT_TRUE(Utils::getLastLine(input, result));
    EXPECT_EQ(result, longLine);
}
