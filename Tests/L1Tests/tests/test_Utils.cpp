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
#include <gmock/gmock.h>
#include <unistd.h>

#include "IarmBusMock.h"
#include "utils.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Fixture shared by IARM::isConnected and IARM::init tests
// ---------------------------------------------------------------------------
class UtilsIARMTest : public ::testing::Test
{
protected:
    NiceMock<IarmBusImplMock>* p_iarmBusImplMock = nullptr;

    void SetUp() override
    {
        p_iarmBusImplMock = new NiceMock<IarmBusImplMock>();
        IarmBus::setImpl(p_iarmBusImplMock);
    }

    void TearDown() override
    {
        IarmBus::setImpl(nullptr);
        delete p_iarmBusImplMock;
        p_iarmBusImplMock = nullptr;
    }
};


// isConnected() returns true when IARM_Bus_IsConnected sets isRegistered = 1
TEST_F(UtilsIARMTest, IsConnectedReturnsTrueWhenRegistered)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;
            return IARM_RESULT_SUCCESS;
        }));

    bool result = Helpers::IARM::isConnected();
    EXPECT_TRUE(result);
}

// isConnected() returns false when IARM_Bus_IsConnected sets isRegistered = 0
TEST_F(UtilsIARMTest, IsConnectedReturnsFalseWhenNotRegistered)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }));

    bool result = Helpers::IARM::isConnected();
    EXPECT_FALSE(result);
}

// isConnected() uses NAME = "Thunder_Plugins" as the member name argument
TEST_F(UtilsIARMTest, IsConnectedPassesCorrectMemberName)
{
    EXPECT_CALL(*p_iarmBusImplMock,
                IARM_Bus_IsConnected(::testing::StrEq("Thunder_Plugins"), _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;
            return IARM_RESULT_SUCCESS;
        }));

    Helpers::IARM::isConnected();
}

// isConnected() result depends only on isRegistered; even a non-SUCCESS
// IARM_Result_t still returns true if isRegistered was set to 1
TEST_F(UtilsIARMTest, IsConnectedIgnoresIARMResultCode)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;              // registered
            return IARM_RESULT_INVALID_STATE; // non-success result — ignored
        }));

    bool result = Helpers::IARM::isConnected();
    EXPECT_TRUE(result);
}


// init() returns true immediately when already connected (no Init/Connect calls)
TEST_F(UtilsIARMTest, InitReturnsTrueWhenAlreadyConnected)
{
    // isConnected() → registered = 1 → true on first call
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;
            return IARM_RESULT_SUCCESS;
        }));

    // Init and Connect must NOT be called
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_)).Times(0);
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect()).Times(0);

    bool result = Helpers::IARM::init();
    EXPECT_TRUE(result);
}

// init() runs Init+Connect when not yet connected; returns true on success
TEST_F(UtilsIARMTest, InitReturnsTrueAfterSuccessfulInitAndConnect)
{
    // Call 1: isConnected() → not registered (triggers Init path)
    // Call 2: isConnected() at end → registered (init succeeded)
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    bool result = Helpers::IARM::init();
    EXPECT_TRUE(result);
}

// init() treats INVALID_STATE from IARM_Bus_Init as acceptable (already inited)
TEST_F(UtilsIARMTest, InitTreatsInvalidStateFromInitAsAcceptable)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;
            return IARM_RESULT_SUCCESS;
        }));

    // IARM_RESULT_INVALID_STATE = already initialised — treated as success
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_INVALID_STATE));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    bool result = Helpers::IARM::init();
    EXPECT_TRUE(result);
}

// init() treats INVALID_STATE from IARM_Bus_Connect as acceptable (already connected)
TEST_F(UtilsIARMTest, InitTreatsInvalidStateFromConnectAsAcceptable)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 1;
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    // IARM_RESULT_INVALID_STATE = already connected — treated as success
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_INVALID_STATE));

    bool result = Helpers::IARM::init();
    EXPECT_TRUE(result);
}

// init() returns false when IARM_Bus_Init fails (not SUCCESS or INVALID_STATE)
TEST_F(UtilsIARMTest, InitReturnsFalseWhenInitFails)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    // Connect must NOT be called if Init failed
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect()).Times(0);

    bool result = Helpers::IARM::init();
    EXPECT_FALSE(result);
}

// init() returns false when IARM_Bus_Connect fails
TEST_F(UtilsIARMTest, InitReturnsFalseWhenConnectFails)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_IPCCORE_FAIL));

    bool result = Helpers::IARM::init();
    EXPECT_FALSE(result);
}

// init() returns false when Init succeeds but isConnected() returns false
// even after Connect succeeds (edge case: bus reports not-connected post-connect)
TEST_F(UtilsIARMTest, InitReturnsFalseWhenStillNotConnectedAfterInitAndConnect)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_IsConnected(_, _))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0;
            return IARM_RESULT_SUCCESS;
        }))
        .WillOnce(Invoke([](const char*, int* isRegistered) -> IARM_Result_t {
            *isRegistered = 0; // still not connected after connect
            return IARM_RESULT_SUCCESS;
        }));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(_))
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillOnce(Return(IARM_RESULT_SUCCESS));

    bool result = Helpers::IARM::init();
    EXPECT_FALSE(result);
}


TEST(FormatIARMResultTest, ReturnsCorrectStringForSuccess)
{
    std::string result = Helpers::formatIARMResult(IARM_RESULT_SUCCESS);
    EXPECT_EQ(result, "IARM_RESULT_SUCCESS [success]");
}

TEST(FormatIARMResultTest, ReturnsCorrectStringForInvalidParam)
{
    std::string result = Helpers::formatIARMResult(IARM_RESULT_INVALID_PARAM);
    EXPECT_EQ(result, "IARM_RESULT_INVALID_PARAM [invalid input parameter]");
}

TEST(FormatIARMResultTest, ReturnsCorrectStringForInvalidState)
{
    std::string result = Helpers::formatIARMResult(IARM_RESULT_INVALID_STATE);
    EXPECT_EQ(result, "IARM_RESULT_INVALID_STATE [invalid state encountered]");
}

TEST(FormatIARMResultTest, ReturnsCorrectStringForIpcCoreFail)
{
    // NOTE: production code returns "IARM_RESULT_IPCORE_FAIL" (one C)
    // even though the enum is IARM_RESULT_IPCCORE_FAIL (two C's).
    // The test asserts the actual production output.
    std::string result = Helpers::formatIARMResult(IARM_RESULT_IPCCORE_FAIL);
    EXPECT_EQ(result, "IARM_RESULT_IPCORE_FAIL [underlying IPC failure]");
}

TEST(FormatIARMResultTest, ReturnsCorrectStringForOOM)
{
    std::string result = Helpers::formatIARMResult(IARM_RESULT_OOM);
    EXPECT_EQ(result, "IARM_RESULT_OOM [out of memory]");
}

TEST(FormatIARMResultTest, ReturnsUnknownStringForUnrecognisedValue)
{
    // Cast an out-of-range value to exercise the default branch
    IARM_Result_t unknownVal = static_cast<IARM_Result_t>(999);
    std::string result = Helpers::formatIARMResult(unknownVal);
    EXPECT_EQ(result, "999 [unknown IARM_Result_t]");
}



TEST(ReadPropertyValueTest, ReturnsFalseForEmptyString)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("", list);
    EXPECT_FALSE(result);
    EXPECT_TRUE(list.empty());
}

TEST(ReadPropertyValueTest, ReturnsFalseWhenNoEqualsSign)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("NOEQUALSSIGN", list);
    EXPECT_FALSE(result);
    EXPECT_TRUE(list.empty());
}

TEST(ReadPropertyValueTest, ReturnsTrueAndExtractsSingleValue)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("KEY=hello", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "hello");
}

TEST(ReadPropertyValueTest, ReturnsTrueAndExtractsMultipleSpaceSeparatedValues)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("KEY=a b c", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0], "a");
    EXPECT_EQ(list[1], "b");
    EXPECT_EQ(list[2], "c");
}

TEST(ReadPropertyValueTest, ReturnsTrueAndExtractsParenthesisFormat)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("KEY=(v1 v2)", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0], "v1");
    EXPECT_EQ(list[1], "v2");
}

TEST(ReadPropertyValueTest, StripsTrailingNewline)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("KEY=value\n", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "value");
}

TEST(ReadPropertyValueTest, StripsTrailingCarriageReturn)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("KEY=value\r", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "value");
}

TEST(ReadPropertyValueTest, ReturnsFalseWhenValueAfterEqualsIsEmpty)
{
    std::vector<std::string> list;
    bool result = Helpers::String::readPropertyValue("KEY=", list);
    EXPECT_FALSE(result);
    EXPECT_TRUE(list.empty());
}


static std::string writeTempFile(const std::string& content)
{
    char tmpPath[] = "/tmp/test_utils_XXXXXX";
    int fd = mkstemp(tmpPath);
    if (fd != -1)
    {
        write(fd, content.c_str(), content.size());
        close(fd);
    }
    return std::string(tmpPath);
}

TEST(ReadPropertyFromFileTest, ReturnsFalseForNonExistentFile)
{
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(
        "/tmp/does_not_exist_utils_test.txt", "KEY", list);
    EXPECT_FALSE(result);
    EXPECT_TRUE(list.empty());
}

TEST(ReadPropertyFromFileTest, ReturnsTrueAndExtractsValueForMatchingProperty)
{
    std::string path = writeTempFile("MYKEY=hello\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "hello");
    remove(path.c_str());
}

TEST(ReadPropertyFromFileTest, ReturnsFalseWhenPropertyNotFound)
{
    std::string path = writeTempFile("OTHER=value\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_FALSE(result);
    EXPECT_TRUE(list.empty());
    remove(path.c_str());
}

TEST(ReadPropertyFromFileTest, SkipsCommentLines)
{
    std::string path = writeTempFile("# MYKEY=commented\nMYKEY=real\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "real");
    remove(path.c_str());
}

TEST(ReadPropertyFromFileTest, SkipsEmptyLines)
{
    std::string path = writeTempFile("\n\nMYKEY=found\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "found");
    remove(path.c_str());
}

TEST(ReadPropertyFromFileTest, ExtractsMultipleValuesInParenthesisFormat)
{
    std::string path = writeTempFile("MYKEY=(val1 val2 val3)\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0], "val1");
    EXPECT_EQ(list[1], "val2");
    EXPECT_EQ(list[2], "val3");
    remove(path.c_str());
}

TEST(ReadPropertyFromFileTest, MatchesOnlyLineStartingWithProperty)
{
    std::string path = writeTempFile("NOTMYKEY=x\nMYKEY=correct\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "correct");
    remove(path.c_str());
}

TEST(ReadPropertyFromFileTest, StopsAtFirstMatchingLine)
{
    std::string path = writeTempFile("MYKEY=first\nMYKEY=second\n");
    std::vector<std::string> list;
    bool result = Helpers::readPropertyFromFile(path.c_str(), "MYKEY", list);
    EXPECT_TRUE(result);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0], "first");
    remove(path.c_str());
}


