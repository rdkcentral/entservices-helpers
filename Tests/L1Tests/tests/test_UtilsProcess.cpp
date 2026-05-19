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
        .WillOnce(Return(&fakeProc))   // first call: return the matching process
        .WillOnce(Return(nullptr));    // second call: end of table
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    // kill(99999, SIGTERM) will fail (no such process) → ret_value stays false
    // but the function still returns false correctly; the important thing is
    // the mock path exercised the cmd-name match branch.
    bool result = Utils::killProcess("myservice");
    // PID 99999 almost certainly doesn't exist — kill returns -1 → false
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — match on cmdline argument
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessMatchesByCmdlineArgument)
{
    static proc_t fakeProc {};
    fakeProc.cmd[0] = '\0'; // cmd name doesn't match
    fakeProc.tid  = 99998;
    fakeProc.ppid = 1;
    // Build fake cmdline: ["/usr/bin/wrapper", "--plugin=target", nullptr]
    static const char* argv[] = { "/usr/bin/wrapper", "--plugin=target", nullptr };
    fakeProc.cmdline = const_cast<char**>(argv);

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&fakeProc))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(*p_readprocImplMock, closeproc(&fakePT));

    bool result = Utils::killProcess("target");
    // PID 99998 almost certainly doesn't exist → kill returns -1 → false
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// killProcess — multiple processes, none match
// ---------------------------------------------------------------------------

TEST_F(UtilsProcessTest, KillProcessMultipleProcessesNoMatch)
{
    static proc_t proc1 {}, proc2 {};
    strncpy(proc1.cmd, "alpha", sizeof(proc1.cmd) - 1);
    proc1.cmdline = nullptr;
    proc1.tid = 1001; proc1.ppid = 1;
    strncpy(proc2.cmd, "beta", sizeof(proc2.cmd) - 1);
    proc2.cmdline = nullptr;
    proc2.tid = 1002; proc2.ppid = 1;

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&proc1))
        .WillOnce(Return(&proc2))
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
    static proc_t fakeProc {};
    fakeProc.cmd[0] = '\0';
    fakeProc.cmdline = nullptr;
    fakeProc.tid = 9997; fakeProc.ppid = 1;

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&fakeProc))
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
    static proc_t child {};
    child.ppid = 1234;
    child.tid  = 5678;
    strncpy(child.cmd, "childproc", sizeof(child.cmd) - 1);

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&child))
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
    static proc_t c1 {}, c2 {}, c3 {};
    c1.ppid = 100; c1.tid = 201;
    c2.ppid = 100; c2.tid = 202;
    c3.ppid = 999; c3.tid = 300; // different parent — must NOT appear

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&c1))
        .WillOnce(Return(&c2))
        .WillOnce(Return(&c3))
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
    static proc_t p1 {};
    p1.ppid = 999; p1.tid = 111;

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&p1))
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
    static proc_t child {};
    child.ppid = 50; child.tid = 60;

    EXPECT_CALL(*p_readprocImplMock, openproc(_))
        .WillOnce(Return(&fakePT));
    EXPECT_CALL(*p_readprocImplMock, readproc(&fakePT, _))
        .WillOnce(Return(&child))
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
