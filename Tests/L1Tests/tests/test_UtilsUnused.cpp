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
#include "UtilsUnused.h"

// ---------------------------------------------------------------------------
// UNUSED macro — must suppress "unused variable" warnings without error
// ---------------------------------------------------------------------------
TEST(UtilsUnusedTest, Unused_IntVariable_CompilesWithNoWarning)
{
    int x = 42;
    UNUSED(x);
    SUCCEED();
}

TEST(UtilsUnusedTest, Unused_PointerVariable_CompilesWithNoWarning)
{
    const char* p = "test";
    UNUSED(p);
    SUCCEED();
}

TEST(UtilsUnusedTest, Unused_FunctionParameter_CompilationTest)
{
    auto fn = [](int a, int b) -> int {
        UNUSED(b);
        return a;
    };
    EXPECT_EQ(fn(5, 99), 5);
}

TEST(UtilsUnusedTest, Unused_Expression_CompilationTest)
{
    int a = 1;
    int b = 2;
    UNUSED(a + b);
    SUCCEED();
}

TEST(UtilsUnusedTest, Unused_BoolVariable)
{
    bool flag = true;
    UNUSED(flag);
    SUCCEED();
}
