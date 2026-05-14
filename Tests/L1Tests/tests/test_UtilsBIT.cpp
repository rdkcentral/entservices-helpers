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
#include "UtilsBIT.h"

// ---------------------------------------------------------------------------
// BIT_SET
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitSet_SetsCorrectBit)
{
    uint32_t val = 0;
    BIT_SET(val, 0);
    EXPECT_EQ(val, 1u);

    BIT_SET(val, 3);
    EXPECT_EQ(val, 9u);    // 0b1001
}

TEST(UtilsBITTest, BitSet_AlreadySetBit_NoChange)
{
    uint32_t val = 0xFF;
    BIT_SET(val, 3);
    EXPECT_EQ(val, 0xFFu);
}

TEST(UtilsBITTest, BitSet_HighBit)
{
    uint64_t val = 0;
    BIT_SET(val, 63);
    EXPECT_EQ(val, (1ULL << 63));
}

// ---------------------------------------------------------------------------
// BIT_CLEAR
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitClear_ClearsCorrectBit)
{
    uint32_t val = 0xFF;
    BIT_CLEAR(val, 0);
    EXPECT_EQ(val, 0xFEu);

    BIT_CLEAR(val, 7);
    EXPECT_EQ(val, 0x7Eu);
}

TEST(UtilsBITTest, BitClear_AlreadyClearedBit_NoChange)
{
    uint32_t val = 0;
    BIT_CLEAR(val, 5);
    EXPECT_EQ(val, 0u);
}

// ---------------------------------------------------------------------------
// BIT_FLIP
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitFlip_FlipsZeroToOne)
{
    uint32_t val = 0;
    BIT_FLIP(val, 4);
    EXPECT_EQ(val, 16u);
}

TEST(UtilsBITTest, BitFlip_FlipsOneToZero)
{
    uint32_t val = 0xFF;
    BIT_FLIP(val, 0);
    EXPECT_EQ(val, 0xFEu);
}

TEST(UtilsBITTest, BitFlip_DoubleFlip_RestoresOriginal)
{
    uint32_t val = 0xA5;
    uint32_t orig = val;
    BIT_FLIP(val, 2);
    BIT_FLIP(val, 2);
    EXPECT_EQ(val, orig);
}

// ---------------------------------------------------------------------------
// BIT_CHECK
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitCheck_ReturnsOneWhenSet)
{
    uint32_t val = 0b10100;
    EXPECT_EQ(BIT_CHECK(val, 2), 1);
    EXPECT_EQ(BIT_CHECK(val, 4), 1);
}

TEST(UtilsBITTest, BitCheck_ReturnsZeroWhenClear)
{
    uint32_t val = 0b10100;
    EXPECT_EQ(BIT_CHECK(val, 0), 0);
    EXPECT_EQ(BIT_CHECK(val, 1), 0);
    EXPECT_EQ(BIT_CHECK(val, 3), 0);
}

TEST(UtilsBITTest, BitCheck_ReturnsOnlyZeroOrOne)
{
    uint32_t val = 0xFFFFFFFF;
    int result = BIT_CHECK(val, 15);
    EXPECT_TRUE(result == 0 || result == 1);
    EXPECT_EQ(result, 1);
}

// ---------------------------------------------------------------------------
// BITMASK_SET
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitmaskSet_SetsAllMaskedBits)
{
    uint32_t val = 0;
    BITMASK_SET(val, 0xF0);
    EXPECT_EQ(val, 0xF0u);
}

TEST(UtilsBITTest, BitmaskSet_PartialOverlap)
{
    uint32_t val = 0x0F;
    BITMASK_SET(val, 0xF0);
    EXPECT_EQ(val, 0xFFu);
}

// ---------------------------------------------------------------------------
// BITMASK_CLEAR
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitmaskClear_ClearsAllMaskedBits)
{
    uint32_t val = 0xFF;
    BITMASK_CLEAR(val, 0x0F);
    EXPECT_EQ(val, 0xF0u);
}

TEST(UtilsBITTest, BitmaskClear_NoMatchingBits_NoChange)
{
    uint32_t val = 0xF0;
    BITMASK_CLEAR(val, 0x0F);
    EXPECT_EQ(val, 0xF0u);
}

// ---------------------------------------------------------------------------
// BITMASK_FLIP
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitmaskFlip_FlipsAllMaskedBits)
{
    uint32_t val = 0xFF;
    BITMASK_FLIP(val, 0x0F);
    EXPECT_EQ(val, 0xF0u);
}

TEST(UtilsBITTest, BitmaskFlip_DoubleFlip_RestoresOriginal)
{
    uint32_t val = 0xA5;
    uint32_t orig = val;
    BITMASK_FLIP(val, 0xFF);
    BITMASK_FLIP(val, 0xFF);
    EXPECT_EQ(val, orig);
}

// ---------------------------------------------------------------------------
// BITMASK_CHECK_ALL
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitmaskCheckAll_AllMaskedBitsSet_ReturnsTrue)
{
    uint32_t val = 0xFF;
    EXPECT_TRUE(BITMASK_CHECK_ALL(val, 0x0F));
    EXPECT_TRUE(BITMASK_CHECK_ALL(val, 0xFF));
}

TEST(UtilsBITTest, BitmaskCheckAll_NotAllMaskedBitsSet_ReturnsFalse)
{
    uint32_t val = 0xF0;
    EXPECT_FALSE(BITMASK_CHECK_ALL(val, 0xFF));
    EXPECT_FALSE(BITMASK_CHECK_ALL(val, 0x0F));
}

TEST(UtilsBITTest, BitmaskCheckAll_ZeroMask_ReturnsTrue)
{
    uint32_t val = 0;
    EXPECT_TRUE(BITMASK_CHECK_ALL(val, 0));
}

// ---------------------------------------------------------------------------
// BITMASK_CHECK_ANY
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, BitmaskCheckAny_AnyMaskedBitSet_ReturnsNonZero)
{
    uint32_t val = 0x01;
    EXPECT_NE(BITMASK_CHECK_ANY(val, 0x0F), 0u);
}

TEST(UtilsBITTest, BitmaskCheckAny_NoMaskedBitSet_ReturnsZero)
{
    uint32_t val = 0xF0;
    EXPECT_EQ(BITMASK_CHECK_ANY(val, 0x0F), 0u);
}

// ---------------------------------------------------------------------------
// GET_BITMASK
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, GetBitmask_Bit0_ReturnsOne)
{
    EXPECT_EQ(GET_BITMASK(0), 1);
}

TEST(UtilsBITTest, GetBitmask_Bit3_ReturnsEight)
{
    EXPECT_EQ(GET_BITMASK(3), 8);
}

TEST(UtilsBITTest, GetBitmask_Bit7_Returns128)
{
    EXPECT_EQ(GET_BITMASK(7), 128);
}

// ---------------------------------------------------------------------------
// Combined set/check round-trip
// ---------------------------------------------------------------------------
TEST(UtilsBITTest, SetThenCheck_AllBits)
{
    uint32_t val = 0;
    for (int i = 0; i < 32; ++i)
    {
        BIT_SET(val, i);
        EXPECT_EQ(BIT_CHECK(val, i), 1) << "bit " << i;
    }
}

TEST(UtilsBITTest, SetThenClearThenCheck_AllBits)
{
    uint32_t val = 0xFFFFFFFF;
    for (int i = 0; i < 32; ++i)
    {
        BIT_CLEAR(val, i);
        EXPECT_EQ(BIT_CHECK(val, i), 0) << "bit " << i;
    }
}
