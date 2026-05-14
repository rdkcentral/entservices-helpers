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
#include <cstring>
#include "UtilsisValidInt.h"

// ---------------------------------------------------------------------------
// isValidInt — returns true for valid signed integers
// ---------------------------------------------------------------------------

TEST(UtilsIsValidIntTest, PositiveInteger_ReturnsTrue)
{
    char s[] = "12345";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, Zero_ReturnsTrue)
{
    char s[] = "0";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, NegativeInteger_ReturnsTrue)
{
    char s[] = "-42";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, NegativeZero_ReturnsTrue)
{
    char s[] = "-0";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, SingleDigit_ReturnsTrue)
{
    char s[] = "9";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, AlphaString_ReturnsFalse)
{
    char s[] = "abc";
    EXPECT_FALSE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, AlphanumericString_ReturnsFalse)
{
    char s[] = "123abc";
    EXPECT_FALSE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, FloatString_ReturnsFalse)
{
    char s[] = "3.14";
    EXPECT_FALSE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, StringWithSpaces_ReturnsFalse)
{
    char s[] = " 123";
    EXPECT_FALSE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, LargeNumber_ReturnsTrue)
{
    char s[] = "2147483647";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, NegativeLargeNumber_ReturnsTrue)
{
    char s[] = "-2147483648";
    EXPECT_TRUE(Utils::isValidInt(s));
}

TEST(UtilsIsValidIntTest, PlusSign_ReturnsFalse)
{
    char s[] = "+5";
    EXPECT_FALSE(Utils::isValidInt(s));
}

// ---------------------------------------------------------------------------
// isValidUnsignedInt — returns true for non-negative digit-only strings
// ---------------------------------------------------------------------------

TEST(UtilsIsValidUnsignedIntTest, PositiveInteger_ReturnsTrue)
{
    char s[] = "12345";
    EXPECT_TRUE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, Zero_ReturnsTrue)
{
    char s[] = "0";
    EXPECT_TRUE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, NegativeInteger_ReturnsFalse)
{
    char s[] = "-42";
    EXPECT_FALSE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, AlphaString_ReturnsFalse)
{
    char s[] = "xyz";
    EXPECT_FALSE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, AlphanumericString_ReturnsFalse)
{
    char s[] = "100abc";
    EXPECT_FALSE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, FloatString_ReturnsFalse)
{
    char s[] = "1.5";
    EXPECT_FALSE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, LargeUnsigned_ReturnsTrue)
{
    char s[] = "4294967295";
    EXPECT_TRUE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, SingleDigit_ReturnsTrue)
{
    char s[] = "7";
    EXPECT_TRUE(Utils::isValidUnsignedInt(s));
}

TEST(UtilsIsValidUnsignedIntTest, StringWithSpace_ReturnsFalse)
{
    char s[] = " 5";
    EXPECT_FALSE(Utils::isValidUnsignedInt(s));
}
