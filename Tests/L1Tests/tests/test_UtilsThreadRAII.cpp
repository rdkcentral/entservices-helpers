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
#include <thread>
#include <atomic>
#include <chrono>
#include "UtilsThreadRAII.h"

// ===========================================================================
// ThreadRAII — default constructor
// ===========================================================================

TEST(UtilsThreadRAIITest, DefaultConstructor_NoCrash)
{
    Utils::ThreadRAII t;
    // destructor called at end of scope — must not crash with a non-joinable thread
    SUCCEED();
}

// ===========================================================================
// ThreadRAII — move constructor from std::thread
// ===========================================================================

TEST(UtilsThreadRAIITest, MoveConstructorFromThread_ThreadRuns)
{
    std::atomic<bool> executed{false};
    {
        Utils::ThreadRAII t(std::thread([&executed]() {
            executed = true;
        }));
    } // destructor joins the thread
    EXPECT_TRUE(executed.load());
}

// ===========================================================================
// ThreadRAII — get() returns the underlying thread
// ===========================================================================

TEST(UtilsThreadRAIITest, Get_ReturnsThread)
{
    std::atomic<bool> started{false};
    Utils::ThreadRAII t(std::thread([&started]() {
        started = true;
    }));
    // get() returns a reference to the internal thread
    EXPECT_TRUE(t.get().joinable());
    // destructor will join
}

// ===========================================================================
// ThreadRAII — move assignment operator
// ===========================================================================

TEST(UtilsThreadRAIITest, MoveAssignment_ThreadRuns)
{
    std::atomic<int> counter{0};
    Utils::ThreadRAII t;
    t = Utils::ThreadRAII(std::thread([&counter]() {
        ++counter;
    }));
    // destructor joins on scope exit
    // Need explicit destruction before checking
    { Utils::ThreadRAII tmp = std::move(t); }
    EXPECT_EQ(counter.load(), 1);
}

// ===========================================================================
// ThreadRAII — thread completes work before destructor
// ===========================================================================

TEST(UtilsThreadRAIITest, ThreadCompletesWork_ResultAvailable)
{
    std::atomic<int> result{0};
    {
        Utils::ThreadRAII t(std::thread([&result]() {
            result.store(42);
        }));
    }
    EXPECT_EQ(result.load(), 42);
}

// ===========================================================================
// ThreadRAII — multiple instances do not interfere
// ===========================================================================

TEST(UtilsThreadRAIITest, MultipleThreads_AllRun)
{
    const int N = 5;
    std::atomic<int> count{0};
    {
        std::vector<Utils::ThreadRAII> threads;
        for (int i = 0; i < N; ++i) {
            threads.emplace_back(std::thread([&count]() {
                ++count;
            }));
        }
    } // all destructors join here
    EXPECT_EQ(count.load(), N);
}

// ===========================================================================
// ThreadRAII — move semantics: moved-from object is safe to destroy
// ===========================================================================

TEST(UtilsThreadRAIITest, MoveFrom_MovedFromObjectSafeToDestroy)
{
    std::atomic<bool> ran{false};
    Utils::ThreadRAII t1(std::thread([&ran]() { ran = true; }));
    Utils::ThreadRAII t2(std::move(t1));
    // t1 is now empty — its destructor must not crash
    // t2 destructor joins the thread
    { Utils::ThreadRAII tmp = std::move(t2); }
    EXPECT_TRUE(ran.load());
}

// ===========================================================================
// ThreadRAII — thread executes after specified delay
// ===========================================================================

TEST(UtilsThreadRAIITest, ThreadWithSleepJoinsCleanly)
{
    std::atomic<bool> done{false};
    {
        Utils::ThreadRAII t(std::thread([&done]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            done = true;
        }));
    }
    EXPECT_TRUE(done.load());
}
