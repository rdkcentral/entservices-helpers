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
 * @file test_UtilsgetRFCConfig.cpp
 *
 * L1 unit tests for helpers/UtilsgetRFCConfig.h
 *
 * Function under test:
 *   Utils::getRFCConfig(const char* paramName, RFC_ParamData_t& paramOutput) -> bool
 *
 * Implementation:
 *   WDMP_STATUS wdmpStatus = getRFCParameter(nullptr, paramName, &paramOutput);
 *   return (wdmpStatus == WDMP_SUCCESS || wdmpStatus == WDMP_ERR_DEFAULT_VALUE);
 *
 * getRFCParameter is a global function pointer defined in Rfc.h and populated
 * by the mock library.  We control it via RfcApi::setImpl(RfcApiImplMock*).
 *
 * Branches to cover:
 *   - WDMP_SUCCESS           → true
 *   - WDMP_ERR_DEFAULT_VALUE → true
 *   - WDMP_FAILURE           → false
 *   - Any other error code   → false
 *   - Verify paramName is passed verbatim to getRFCParameter
 *   - Verify paramOutput is the struct passed by address
 */

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Module.h"
#include "UtilsgetRFCConfig.h"
#include "UtilsLogging.h"
#include "WrapsMock.h"
#include "RfcApiMock.h"

#include <cstring>
#include <unistd.h>

extern "C" int __real_unlink(const char* path);

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

/* ===========================================================
 * Fixture
 * =========================================================== */
class GetRFCConfigTest : public ::testing::Test {
protected:
    NiceMock<WrapsImplMock>*   p_wrapsImplMock  { nullptr };
    NiceMock<RfcApiImplMock>*  p_rfcApiImplMock { nullptr };

    void SetUp() override
    {
        /* Wraps mock — needed so any lingering ::unlink() calls don't crash */
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(_))
            .WillByDefault(Invoke(__real_unlink));

        /* RFC mock — controls the getRFCParameter function pointer */
        p_rfcApiImplMock = new NiceMock<RfcApiImplMock>;
        RfcApi::setImpl(p_rfcApiImplMock);
    }

    void TearDown() override
    {
        RfcApi::setImpl(nullptr);
        delete p_rfcApiImplMock;
        p_rfcApiImplMock = nullptr;

        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }
};

/* ===========================================================
 * Success paths
 * =========================================================== */

TEST_F(GetRFCConfigTest, WdmpSuccessReturnsTrue)
{
    LOGINFO("Test: WDMP_SUCCESS → getRFCConfig returns true");
    RFC_ParamData_t output{};

    EXPECT_CALL(*p_rfcApiImplMock,
                getRFCParameter(nullptr, StrEq("Device.TestParam"), _))
        .WillOnce(Return(WDMP_SUCCESS));

    EXPECT_TRUE(Utils::getRFCConfig("Device.TestParam", output));
}

TEST_F(GetRFCConfigTest, WdmpErrDefaultValueReturnsTrue)
{
    LOGINFO("Test: WDMP_ERR_DEFAULT_VALUE → getRFCConfig returns true");
    RFC_ParamData_t output{};

    EXPECT_CALL(*p_rfcApiImplMock,
                getRFCParameter(nullptr, StrEq("Device.DefaultParam"), _))
        .WillOnce(Return(WDMP_ERR_DEFAULT_VALUE));

    EXPECT_TRUE(Utils::getRFCConfig("Device.DefaultParam", output));
}

/* ===========================================================
 * Failure paths
 * =========================================================== */

TEST_F(GetRFCConfigTest, WdmpFailureReturnsFalse)
{
    LOGINFO("Test: WDMP_FAILURE → getRFCConfig returns false");
    RFC_ParamData_t output{};

    EXPECT_CALL(*p_rfcApiImplMock,
                getRFCParameter(_, _, _))
        .WillOnce(Return(WDMP_FAILURE));

    EXPECT_FALSE(Utils::getRFCConfig("Device.Param", output));
}

TEST_F(GetRFCConfigTest, WdmpErrNotExistReturnsFalse)
{
    LOGINFO("Test: WDMP_ERR_NOT_EXIST → getRFCConfig returns false");
    RFC_ParamData_t output{};

    EXPECT_CALL(*p_rfcApiImplMock,
                getRFCParameter(_, _, _))
        .WillOnce(Return(WDMP_ERR_NOT_EXIST));

    EXPECT_FALSE(Utils::getRFCConfig("Device.NoParam", output));
}

TEST_F(GetRFCConfigTest, WdmpErrInvalidParameterNameReturnsFalse)
{
    LOGINFO("Test: WDMP_ERR_INVALID_PARAMETER_NAME → false");
    RFC_ParamData_t output{};

    EXPECT_CALL(*p_rfcApiImplMock,
                getRFCParameter(_, _, _))
        .WillOnce(Return(WDMP_ERR_INVALID_PARAMETER_NAME));

    EXPECT_FALSE(Utils::getRFCConfig("BadParam", output));
}

/* ===========================================================
 * Output struct is populated by the mock
 * =========================================================== */

TEST_F(GetRFCConfigTest, OutputStructIsPopulatedOnSuccess)
{
    LOGINFO("Test: paramOutput struct receives value filled by getRFCParameter");
    RFC_ParamData_t output{};

    EXPECT_CALL(*p_rfcApiImplMock,
                getRFCParameter(nullptr, StrEq("Device.Value"), _))
        .WillOnce(DoAll(
            Invoke([](char*, const char*, RFC_ParamData_t* out) {
                snprintf(out->value, MAX_PARAM_LEN, "42");
                out->type = WDMP_STRING;
            }),
            Return(WDMP_SUCCESS)));

    EXPECT_TRUE(Utils::getRFCConfig("Device.Value", output));
    EXPECT_STREQ("42", output.value);
    EXPECT_EQ(WDMP_STRING, output.type);
}
