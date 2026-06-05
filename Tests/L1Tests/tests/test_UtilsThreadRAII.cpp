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

// ThreadRAII uses LOGINFO/LOGERR which expand to fprintf(stderr,...) — NOT syslog.
// Therefore WrapsImplMock is NOT required for these tests.

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Module.h"
#include "UtilsThreadRAII.h"

#include <atomic>
#include <chrono>
#include <thread>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Simple task that sets a flag and exits.
static void setFlagTask(std::atomic<bool>& flag)
{
    flag.store(true);
}

// Task that spins briefly before setting a flag, to ensure the thread is
// truly running when the guard is constructed.
static void slowFlagTask(std::atomic<bool>& flag)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    flag.store(true);
}

// ---------------------------------------------------------------------------
// Default constructor
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, DefaultConstructorCreatesNoThread)
{
    Utils::ThreadRAII t;
    // get() must return a default-constructed, non-joinable thread
    EXPECT_FALSE(t.get().joinable());
}

// ---------------------------------------------------------------------------
// Constructor from rvalue thread
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, ConstructorTakesOwnershipOfThread)
{
    std::atomic<bool> done{false};
    {
        Utils::ThreadRAII t(std::thread(setFlagTask, std::ref(done)));
        EXPECT_TRUE(t.get().joinable());
    } // destructor joins here
    EXPECT_TRUE(done.load());
}

TEST(ThreadRAIITest, DestructorJoinsRunningThread)
{
    std::atomic<bool> done{false};
    {
        Utils::ThreadRAII t(std::thread(slowFlagTask, std::ref(done)));
        // thread may still be sleeping here
    } // destructor must wait for the thread
    EXPECT_TRUE(done.load());
}

// ---------------------------------------------------------------------------
// get()
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, GetReturnsJoinableReferenceForRunningThread)
{
    std::atomic<bool> done{false};
    Utils::ThreadRAII t(std::thread(setFlagTask, std::ref(done)));
    std::thread& ref = t.get();
    EXPECT_TRUE(ref.joinable());
    ref.join(); // join via the returned reference
    EXPECT_FALSE(t.get().joinable()); // now non-joinable
    EXPECT_TRUE(done.load());
}

TEST(ThreadRAIITest, GetOnDefaultConstructedReturnsNonJoinable)
{
    Utils::ThreadRAII t;
    EXPECT_FALSE(t.get().joinable());
}

// ---------------------------------------------------------------------------
// Destructor when thread already joined manually
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, DestructorIsNoopWhenThreadAlreadyJoined)
{
    std::atomic<bool> done{false};
    {
        Utils::ThreadRAII t(std::thread(setFlagTask, std::ref(done)));
        t.get().join(); // explicit join — destructor should detect non-joinable
        EXPECT_FALSE(t.get().joinable());
    } // destructor runs: joinable()==false -> no join called -> no crash
    EXPECT_TRUE(done.load());
}

// ---------------------------------------------------------------------------
// Destructor when default-constructed (no thread held)
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, DestructorOnDefaultConstructedDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        Utils::ThreadRAII t;
    });
}

// ---------------------------------------------------------------------------
// Move constructor
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, MoveConstructorTransfersOwnership)
{
    std::atomic<bool> done{false};
    Utils::ThreadRAII src(std::thread(slowFlagTask, std::ref(done)));
    EXPECT_TRUE(src.get().joinable());

    Utils::ThreadRAII dst(std::move(src));
    EXPECT_TRUE(dst.get().joinable());
    // dst destructor joins
}

TEST(ThreadRAIITest, MoveConstructorFromDefaultLeavesDestinationEmpty)
{
    Utils::ThreadRAII src;
    Utils::ThreadRAII dst(std::move(src));
    EXPECT_FALSE(dst.get().joinable());
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, MoveAssignmentTransfersOwnership)
{
    std::atomic<bool> done{false};
    Utils::ThreadRAII src(std::thread(slowFlagTask, std::ref(done)));
    Utils::ThreadRAII dst;

    dst = std::move(src);
    EXPECT_TRUE(dst.get().joinable());
    // dst destructor joins
}

TEST(ThreadRAIITest, MoveAssignmentFromDefaultMakesDestinationEmpty)
{
    Utils::ThreadRAII src;
    Utils::ThreadRAII dst;
    dst = std::move(src);
    EXPECT_FALSE(dst.get().joinable());
}

// ---------------------------------------------------------------------------
// Thread completes before guard scope ends — no double-join
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, ThreadCompletesBeforeScopeEndsNoDoubleJoin)
{
    std::atomic<bool> done{false};
    {
        Utils::ThreadRAII t(std::thread(setFlagTask, std::ref(done)));
        // Wait for thread to finish naturally
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_TRUE(done.load());
        // Thread has finished but is still joinable until joined
        // Destructor of ThreadRAII will join it safely
    }
    EXPECT_TRUE(done.load());
}

// ---------------------------------------------------------------------------
// Multiple sequential ThreadRAII objects — no resource leaks
// ---------------------------------------------------------------------------

TEST(ThreadRAIITest, MultipleSequentialThreadsJoinCleanly)
{
    for (int i = 0; i < 5; ++i) {
        std::atomic<bool> done{false};
        {
            Utils::ThreadRAII t(std::thread(setFlagTask, std::ref(done)));
        }
        EXPECT_TRUE(done.load());
    }
}
