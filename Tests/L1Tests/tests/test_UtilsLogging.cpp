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

// Tests for UtilsLogging.h — verifies that all logging macros expand correctly,
// do not produce compile errors, and write to stderr (format round-trip spot
// checks via pipe redirection are out of scope for unit tests; here we verify
// the macros produce valid calls at the call site level).

#include <gtest/gtest.h>
#include <cstdio>
#include "UtilsLogging.h"

// ===========================================================================
// Compile-time / smoke tests — each macro must expand without error
// ===========================================================================

TEST(UtilsLoggingTest, LOGDBG_CompilationAndInvocation)
{
    // Must compile and not crash
    EXPECT_NO_THROW(LOGDBG("test debug message %d", 1));
}

TEST(UtilsLoggingTest, LOGINFO_CompilationAndInvocation)
{
    EXPECT_NO_THROW(LOGINFO("test info message %s", "hello"));
}

TEST(UtilsLoggingTest, LOGWARN_CompilationAndInvocation)
{
    EXPECT_NO_THROW(LOGWARN("test warn message"));
}

TEST(UtilsLoggingTest, LOGERR_CompilationAndInvocation)
{
    EXPECT_NO_THROW(LOGERR("test error message code=%d", 42));
}

TEST(UtilsLoggingTest, LOGTRACE_CompilationAndInvocation)
{
    // LOGTRACE is a no-op unless __DEBUG__ is defined; either way must compile
    EXPECT_NO_THROW(LOGTRACE("trace %s", "data"));
}

// ===========================================================================
// Format-string variety — ensure variadic expansion works for common types
// ===========================================================================

TEST(UtilsLoggingTest, LOGINFO_IntegerArg)
{
    EXPECT_NO_THROW(LOGINFO("value = %d", 100));
}

TEST(UtilsLoggingTest, LOGINFO_StringArg)
{
    EXPECT_NO_THROW(LOGINFO("name = %s", "plugin"));
}

TEST(UtilsLoggingTest, LOGINFO_MultipleArgs)
{
    EXPECT_NO_THROW(LOGINFO("x=%d y=%d label=%s", 1, 2, "test"));
}

TEST(UtilsLoggingTest, LOGERR_NoFormatArgs)
{
    EXPECT_NO_THROW(LOGERR("standalone error message"));
}

TEST(UtilsLoggingTest, LOGWARN_FloatArg)
{
    EXPECT_NO_THROW(LOGWARN("ratio=%.2f", 3.14f));
}

// ===========================================================================
// Macro-usage inside conditional / loop constructs (regression: dangling else)
// ===========================================================================

TEST(UtilsLoggingTest, LOGINFO_InsideIfStatement_NoDanglingElse)
{
    bool cond = true;
    if (cond)
        LOGINFO("inside if");
    else
        LOGINFO("inside else");
    SUCCEED();
}

TEST(UtilsLoggingTest, LOGERR_InsideLoop)
{
    for (int i = 0; i < 3; ++i) {
        LOGERR("loop iteration %d", i);
    }
    SUCCEED();
}
