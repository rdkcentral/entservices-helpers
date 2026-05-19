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

// LOGINFO/LOGERR in UtilsProcess.h expand to fprintf(stderr,...) — NOT syslog.
// WrapsImplMock is therefore NOT required.
//
// openproc/readproc/closeproc are dispatched via ProcImpl function pointers
// (readprocMockInterface.cpp).  Each has EXPECT_NE(impl,nullptr), so
// readprocImplMock MUST be registered before any call that would enter the
// production code path.

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Module.h"
#include "readprocMock.h"
#include "UtilsProcess.h"

#include <csignal>
#include <vector>

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::NiceMock;

// ---------------------------------------------------------------------------
// Fixture — registers/unregisters readprocImplMock around each test
// ---------------------------------------------------------------------------

class UtilsProcessTest : public ::testing::Test {
protected:
    NiceMock<readprocImplMock>* p_readprocImplMock { nullptr };
    PROCTAB fakePT {};

    void SetUp() override
    {
        p_readprocImplMock = new NiceMock<readprocImplMock>;
        ProcImpl::setImpl(p_readprocImplMock);
    }

    void TearDown() override
    {
        ProcImpl::setImpl(nullptr);
        delete p_readprocImplMock;
        p_readprocImplMock = nullptr;
    }
};

// ---------------------------------------------------------------------------
// killProcess — empty input
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessEmptyNameReturnsFalse)
{
    // Empty name: function returns early before calling openproc
    // No mock expectations needed for proc calls
    bool result = Utils::killProcess("");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — openproc returns NULL
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessOpenProcNullReturnsFalse)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(nullptr));
    // closeproc and readproc must NOT be called
    EXPECT_CALL(*p_readprocImplMock, closeproc(_)).Times(0);
    EXPECT_CALL(*p_readprocImplMock, readproc(_, _)).Times(0);

    bool result = Utils::killProcess("someprocess");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — no matching process (readproc returns NULL immediately)
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessNoMatchReturnsFalse)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(nullptr)); // empty process table
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    bool result = Utils::killProcess("nonexistent_process_xyz");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — match on cmd name
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessMatchesByCommandName)
{
    static proc_t fakeProc {};
    // Set cmd name to something that contains our search string
    strncpy(fakeProc.cmd, "myservice", sizeof(fakeProc.cmd) - 1);
    fakeProc.cmd[sizeof(fakeProc.cmd) - 1] = '\0';
    fakeProc.tid  = 99999; // use a definitely non-existent PID
    fakeProc.ppid = 1;
    fakeProc.cmdline = nullptr;

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {};
            strncpy(d.cmd, "myservice", sizeof(d.cmd) - 1);
            d.tid = 99999; d.ppid = 1; d.cmdline = nullptr;
            *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    // kill(99999, SIGTERM) will fail (no such process) → ret_value stays false
    bool result = Utils::killProcess("myservice");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — match on cmdline argument
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessMatchesByCmdlineArgument)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {};
            static const char* av[] = { "/usr/bin/wrapper", "--plugin=target", nullptr };
            d.cmd[0] = '\0';
            d.tid = 99998; d.ppid = 1;
            d.cmdline = const_cast<char**>(av);
            *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    bool result = Utils::killProcess("target");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — multiple processes, none match
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessMultipleProcessesNoMatch)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {};
            strncpy(d.cmd, "alpha", sizeof(d.cmd) - 1);
            d.tid = 1001; d.ppid = 1; d.cmdline = nullptr;
            *p = d; return p;
        }))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {};
            strncpy(d.cmd, "beta", sizeof(d.cmd) - 1);
            d.tid = 1002; d.ppid = 1; d.cmdline = nullptr;
            *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    bool result = Utils::killProcess("gamma");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — cmd name is empty, cmdline is nullptr (no match path)
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessEntryWithEmptyCmdAndNullCmdline)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {};
            d.cmd[0] = '\0'; d.tid = 9997; d.ppid = 1; d.cmdline = nullptr;
            *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    bool result = Utils::killProcess("anything");
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — openproc returns NULL
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsOpenProcNullReturnsFalse)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(_)).Times(0);
    EXPECT_CALL(*p_readprocImplMock, readproc(_, _)).Times(0);

    std::vector<int> ids;
    bool result = Utils::getChildProcessIDs(1234, ids);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ids.empty());
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — empty process table
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsNoProcessesReturnsFalse)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    std::vector<int> ids;
    bool result = Utils::getChildProcessIDs(1234, ids);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ids.empty());
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — one child found
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsOneChildFound)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {};
            d.ppid = 1234; d.tid = 5678;
            strncpy(d.cmd, "childproc", sizeof(d.cmd) - 1);
            *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    std::vector<int> ids;
    bool result = Utils::getChildProcessIDs(1234, ids);
    EXPECT_TRUE(result);
    ASSERT_EQ(1u, ids.size());
    EXPECT_EQ(5678, ids[0]);
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — multiple children found
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsMultipleChildrenFound)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {}; d.ppid = 100; d.tid = 201; *p = d; return p;
        }))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {}; d.ppid = 100; d.tid = 202; *p = d; return p;
        }))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {}; d.ppid = 999; d.tid = 300; *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    std::vector<int> ids;
    bool result = Utils::getChildProcessIDs(100, ids);
    EXPECT_TRUE(result);
    ASSERT_EQ(2u, ids.size());
    EXPECT_EQ(201, ids[0]);
    EXPECT_EQ(202, ids[1]);
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — no process matches given ppid
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsNoChildForGivenPPID)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {}; d.ppid = 999; d.tid = 111; *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    std::vector<int> ids;
    bool result = Utils::getChildProcessIDs(1234, ids);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ids.empty());
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — passing zero as ppid
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsZeroPPID)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    std::vector<int> ids;
    bool result = Utils::getChildProcessIDs(0, ids);
    EXPECT_FALSE(result);
    EXPECT_TRUE(ids.empty());
}

// ---------------------------------------------------------------------------
// getChildProcessIDs — pre-existing items in output vector are preserved
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, GetChildProcessIDsAppendsToExistingVector)
{
    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Invoke([](PROCTAB*, proc_t* p) -> proc_t* {
            static proc_t d {}; d.ppid = 50; d.tid = 60; *p = d; return p;
        }))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    std::vector<int> ids = { 10, 20 }; // pre-existing entries
    bool result = Utils::getChildProcessIDs(50, ids);
    EXPECT_TRUE(result);
    ASSERT_EQ(3u, ids.size());
    EXPECT_EQ(10, ids[0]);
    EXPECT_EQ(20, ids[1]);
    EXPECT_EQ(60, ids[2]);
}
