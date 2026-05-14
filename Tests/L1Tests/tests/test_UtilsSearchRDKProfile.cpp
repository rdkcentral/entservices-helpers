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

// NOTE: UtilsSearchRDKProfile.h defines a file-scoped global 'profileType'
// and the function 'searchRdkProfile'. Include it in exactly ONE translation
// unit to avoid ODR violations.

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include "UtilsSearchRDKProfile.h"

// Helper to write a temporary device.properties-like file and return its path.
// searchRdkProfile() always reads /etc/device.properties, so we test the
// function behavior via the real system file (if present) or verify the
// NOT_FOUND path when the file is absent.

// ===========================================================================
// Enum value sanity
// ===========================================================================

TEST(UtilsSearchRDKProfileTest, EnumValues_DistinctAndKnown)
{
    EXPECT_EQ(NOT_FOUND, -1);
    EXPECT_EQ(STB, 0);
    EXPECT_EQ(TV, 1);
    EXPECT_LT(NOT_FOUND, STB);
    EXPECT_LT(STB, TV);
    EXPECT_LT(TV, MAX);
}

// ===========================================================================
// profileType global — initial value is NOT_FOUND
// ===========================================================================

TEST(UtilsSearchRDKProfileTest, GlobalProfileType_InitialValue_IsNotFound)
{
    EXPECT_EQ(profileType, NOT_FOUND);
}

// ===========================================================================
// searchRdkProfile — returns a valid profile_t
// ===========================================================================

TEST(UtilsSearchRDKProfileTest, SearchRdkProfile_ReturnsValidProfile)
{
    profile_t result = searchRdkProfile();
    // Result must be one of the defined enum values
    EXPECT_TRUE(result == NOT_FOUND || result == STB || result == TV);
}

TEST(UtilsSearchRDKProfileTest, SearchRdkProfile_ReturnTypeIsProfileT)
{
    profile_t result = searchRdkProfile();
    // Must be in the range [-1, MAX)
    EXPECT_GE(static_cast<int>(result), static_cast<int>(NOT_FOUND));
    EXPECT_LT(static_cast<int>(result), static_cast<int>(MAX));
}

// ===========================================================================
// searchRdkProfile — idempotent (two calls yield the same result)
// ===========================================================================

TEST(UtilsSearchRDKProfileTest, SearchRdkProfile_TwoCalls_SameResult)
{
    profile_t r1 = searchRdkProfile();
    profile_t r2 = searchRdkProfile();
    EXPECT_EQ(r1, r2);
}

// ===========================================================================
// PROFILE macros — string values are as expected
// ===========================================================================

TEST(UtilsSearchRDKProfileTest, ProfileMacro_TV_StringValue)
{
    EXPECT_STREQ(PROFILE_TV, "TV");
}

TEST(UtilsSearchRDKProfileTest, ProfileMacro_STB_StringValue)
{
    EXPECT_STREQ(PROFILE_STB, "STB");
}

TEST(UtilsSearchRDKProfileTest, RDKProfileMacro_StartsWithRDK)
{
    std::string macro(RDK_PROFILE);
    EXPECT_EQ(macro, "RDK_PROFILE=");
}
