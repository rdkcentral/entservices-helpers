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
 * @file test_UtilsfileExists.cpp
 *
 * L1 unit tests for helpers/UtilsfileExists.h
 *
 * Function under test:
 *   Utils::fileExists(const char* pFileName)
 *     — calls ::stat() and returns true iff stat succeeds (return 0).
 *     — stat() is NOT a wrapped linker-intercepted call in the helpers
 *       test build, so real filesystem operations are used.
 *
 * Note: ::unlink() IS a wrapped call (linker -Wl,-wrap,unlink).
 *   The WrapsImplMock must be set up before any direct ::unlink()
 *   call in the test code, and torn down afterwards.
 */

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Module.h"
#include "UtilsfileExists.h"
#include "UtilsLogging.h"
#include "WrapsMock.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

/* The linker creates __real_unlink when -Wl,-wrap,unlink is active. */
extern "C" int __real_unlink(const char* path);

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

/* ===========================================================
 * Fixture
 * =========================================================== */
class FileExistsTest : public ::testing::Test {
protected:
    std::string tmpFile;
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };

    void SetUp() override
    {
        /* WrapsImplMock must be registered BEFORE any ::unlink() call
         * because the linker intercepts unlink and crashes on null impl. */
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(_))
            .WillByDefault(Invoke(__real_unlink));

        /* Create a real temp file so Exists tests have something to find. */
        mode_t old_umask = ::umask(0077); /* Set restrictive umask: owner-only access */
        char tmpl[] = "/tmp/utilsfileexists_XXXXXX";
        int fd = ::mkstemp(tmpl);
        ::umask(old_umask); /* Restore original umask */
        ASSERT_NE(-1, fd) << "mkstemp failed: " << strerror(errno);
        if (fd >= 0) {
            ::close(fd);
        }
        tmpFile = tmpl;
    }

    void TearDown() override
    {
        /* ::unlink() is intercepted → goes through WrapsImplMock → __real_unlink */
        (void)::unlink(tmpFile.c_str());
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }
};

/* ===========================================================
 * 1. Positive path — file exists
 * =========================================================== */

TEST_F(FileExistsTest, ExistingRegularFileReturnsTrue)
{
    LOGINFO("Test: fileExists returns true for an existing regular file");
    EXPECT_TRUE(Utils::fileExists(tmpFile.c_str()));
}

/* ===========================================================
 * 2. Negative path — file does not exist
 * =========================================================== */

TEST_F(FileExistsTest, NonExistentFileReturnsFalse)
{
    LOGINFO("Test: fileExists returns false after the file is deleted");
    (void)::unlink(tmpFile.c_str()); /* remove it first */
    EXPECT_FALSE(Utils::fileExists(tmpFile.c_str()));
}

TEST_F(FileExistsTest, NeverCreatedPathReturnsFalse)
{
    LOGINFO("Test: fileExists returns false for a path that was never created");
    std::string ghost = "/tmp/utilsfileexists_ghost_path_12345678";
    EXPECT_FALSE(Utils::fileExists(ghost.c_str()));
}

/* ===========================================================
 * 3. Special inputs
 * =========================================================== */

TEST_F(FileExistsTest, EmptyStringReturnsFalse)
{
    LOGINFO("Test: fileExists(\"\") returns false — stat(\"\") fails with ENOENT");
    EXPECT_FALSE(Utils::fileExists(""));
}

TEST_F(FileExistsTest, ExistingDirectoryReturnsTrue)
{
    LOGINFO("Test: fileExists returns true for a directory — stat works on dirs");
    /* /tmp is guaranteed to exist on any Linux CI runner */
    EXPECT_TRUE(Utils::fileExists("/tmp"));
}

/* ===========================================================
 * 4. File created mid-test (dynamic existence check)
 * =========================================================== */

TEST_F(FileExistsTest, FileNotExistsThenCreatedThenExists)
{
    LOGINFO("Test: fileExists correctly reflects file creation at runtime");
    std::string dynPath = "/tmp/utilsfileexists_dyncheck_XXXXXX";
    /* Ensure absent first */
    (void)::unlink(dynPath.c_str()); /* harmless if already absent */
    ASSERT_FALSE(Utils::fileExists(dynPath.c_str()));

    /* Create it */
    {
        std::ofstream f(dynPath);
        ASSERT_TRUE(f.is_open()) << "Could not create " << dynPath;
    }

    EXPECT_TRUE(Utils::fileExists(dynPath.c_str()));

    /* cleanup */
    (void)::unlink(dynPath.c_str());
}

/* ===========================================================
 * 5. Idempotency — repeated calls return the same result
 * =========================================================== */

TEST_F(FileExistsTest, RepeatedCallsOnExistingFileAreConsistent)
{
    LOGINFO("Test: calling fileExists multiple times on the same file is stable");
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(Utils::fileExists(tmpFile.c_str()))
            << "Iteration " << i << " should return true";
    }
}
