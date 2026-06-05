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
#include "UtilsisValidInt.h"

// isValidInt and isValidUnsignedInt are pure char-check functions with no
// LOGINFO calls, so no WrapsImplMock is required.

// ---------------------------------------------------------------------------
// Utils::isValidInt
// ---------------------------------------------------------------------------

TEST(IsValidIntTest, PositiveInteger)
{
    char buf[] = "123";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, NegativeInteger)
{
    char buf[] = "-123";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, Zero)
{
    char buf[] = "0";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, NegativeZero)
{
    char buf[] = "-0";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, SingleDigit)
{
    char buf[] = "9";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, LargePositiveNumber)
{
    char buf[] = "2147483647";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, LargeNegativeNumber)
{
    char buf[] = "-2147483648";
    EXPECT_TRUE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, EmptyStringReturnsFalse)
{
    char buf[] = "";
    EXPECT_FALSE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, JustMinusReturnsFalse)
{
    char buf[] = "-";
    EXPECT_FALSE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, NonDigitCharReturnsFalse)
{
    char buf[] = "12a";
    EXPECT_FALSE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, LeadingNonDigitReturnsFalse)
{
    char buf[] = "a12";
    EXPECT_FALSE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, FloatStringReturnsFalse)
{
    char buf[] = "1.5";
    EXPECT_FALSE(Utils::isValidInt(buf));
}

TEST(IsValidIntTest, LeadingSpaceReturnsFalse)
{
    char buf[] = " 1";
    EXPECT_FALSE(Utils::isValidInt(buf));
}

// ---------------------------------------------------------------------------
// Utils::isValidUnsignedInt
// ---------------------------------------------------------------------------

TEST(IsValidUnsignedIntTest, PositiveInteger)
{
    char buf[] = "123";
    EXPECT_TRUE(Utils::isValidUnsignedInt(buf));
}

TEST(IsValidUnsignedIntTest, Zero)
{
    char buf[] = "0";
    EXPECT_TRUE(Utils::isValidUnsignedInt(buf));
}

TEST(IsValidUnsignedIntTest, LargeUnsignedNumber)
{
    char buf[] = "4294967295";
    EXPECT_TRUE(Utils::isValidUnsignedInt(buf));
}

TEST(IsValidUnsignedIntTest, NegativeIntReturnsFalse)
{
    char buf[] = "-123";
    EXPECT_FALSE(Utils::isValidUnsignedInt(buf));
}

TEST(IsValidUnsignedIntTest, EmptyStringReturnsFalse)
{
    char buf[] = "";
    EXPECT_FALSE(Utils::isValidUnsignedInt(buf));
}

TEST(IsValidUnsignedIntTest, NonDigitReturnsFalse)
{
    char buf[] = "1a";
    EXPECT_FALSE(Utils::isValidUnsignedInt(buf));
}

TEST(IsValidUnsignedIntTest, FloatStringReturnsFalse)
{
    char buf[] = "3.14";
    EXPECT_FALSE(Utils::isValidUnsignedInt(buf));
}
