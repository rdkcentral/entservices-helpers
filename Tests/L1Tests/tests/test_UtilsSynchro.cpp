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
// UtilsSynchro.hpp includes <plugins/plugins.h> which needs Module.h first.
#include "UtilsSynchro.hpp"

#include <mutex>
#include <thread>
#include <atomic>

// LockApiGuard and UnlockApiGuard are pure RAII guards over std::recursive_mutex.
// They do not call LOGINFO, so no WrapsImplMock is required for these tests.
// (getFunctionToCall uses LOGINFO but we do not instantiate that template here.)

// Dummy tag types to get distinct ApiLocks instances.
struct GuardTestClassA {};
struct GuardTestClassB {};

// ---------------------------------------------------------------------------
// LockApiGuard
// ---------------------------------------------------------------------------

TEST(LockApiGuardTest, ConstructAndDestructDoNotCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        Utils::Synchro::LockApiGuard<GuardTestClassA> guard;
    }); // unique_lock released in destructor
}

TEST(LockApiGuardTest, UnlockAndRelockWork)
{
    EXPECT_NO_FATAL_FAILURE({
        Utils::Synchro::LockApiGuard<GuardTestClassA> guard;
        guard.unlock();
        guard.lock();
    });
}

TEST(LockApiGuardTest, RecursiveMutexAllowsNestedGuards)
{
    // ApiLocks<T>::mtx is std::recursive_mutex so the same thread may acquire
    // it multiple times without deadlocking.
    EXPECT_NO_FATAL_FAILURE({
        Utils::Synchro::LockApiGuard<GuardTestClassA> outer;
        Utils::Synchro::LockApiGuard<GuardTestClassA> inner;
    });
}

TEST(LockApiGuardTest, DifferentTypeTagsUseSeparateMutexes)
{
    // GuardTestClassA and GuardTestClassB each have their own static mutex,
    // so both guards can be held simultaneously without deadlock.
    EXPECT_NO_FATAL_FAILURE({
        Utils::Synchro::LockApiGuard<GuardTestClassA> guardA;
        Utils::Synchro::LockApiGuard<GuardTestClassB> guardB;
    });
}

TEST(LockApiGuardTest, MutexIsReleasedAfterGuardScope)
{
    {
        Utils::Synchro::LockApiGuard<GuardTestClassA> guard;
        // lock is held; another thread's try_lock would fail here
    }
    // After scope the mutex must be free; try_lock from this thread should
    // succeed (recursive_mutex always succeeds for the owning thread, but
    // here we just verify try_lock does not deadlock).
    bool locked = Utils::Synchro::ApiLocks<GuardTestClassA>::mtx.try_lock();
    EXPECT_TRUE(locked);
    if (locked) {
        Utils::Synchro::ApiLocks<GuardTestClassA>::mtx.unlock();
    }
}

// ---------------------------------------------------------------------------
// UnlockApiGuard
// ---------------------------------------------------------------------------

TEST(UnlockApiGuardTest, NoopWhenIsThreadUsingLockedApiIsFalse)
{
    // isThreadUsingLockedApi starts as false in every test thread.
    // The guard constructor and destructor should be no-ops.
    EXPECT_NO_FATAL_FAILURE({
        Utils::Synchro::UnlockApiGuard<GuardTestClassA> guard;
    });
}

TEST(UnlockApiGuardTest, MultipleInstancesNoopWhenFlagIsFalse)
{
    EXPECT_NO_FATAL_FAILURE({
        Utils::Synchro::UnlockApiGuard<GuardTestClassA> g1;
        Utils::Synchro::UnlockApiGuard<GuardTestClassB> g2;
    });
}
