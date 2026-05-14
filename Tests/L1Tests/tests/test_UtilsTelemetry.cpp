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

// Telemetry tests are compiled WITHOUT ENABLE_TELEMETRY_LOGGING so all methods
// become empty stubs. The goal is to verify that:
//  1. The struct and its static methods are callable.
//  2. No crashes occur even when the logging back-end is absent.

#include <gtest/gtest.h>
#include "UtilsTelemetry.h"

// ===========================================================================
// Telemetry::init
// ===========================================================================

TEST(UtilsTelemetryTest, Init_DoesNotCrash)
{
    EXPECT_NO_THROW(Utils::Telemetry::init());
}

TEST(UtilsTelemetryTest, Init_CalledMultipleTimes_NoError)
{
    EXPECT_NO_THROW({
        Utils::Telemetry::init();
        Utils::Telemetry::init();
        Utils::Telemetry::init();
    });
}

// ===========================================================================
// Telemetry::sendMessage(char*)
// ===========================================================================

TEST(UtilsTelemetryTest, SendMessageSingleArg_DoesNotCrash)
{
    char msg[] = "test message";
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(msg));
}

TEST(UtilsTelemetryTest, SendMessageSingleArg_EmptyMessage_DoesNotCrash)
{
    char msg[] = "";
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(msg));
}

TEST(UtilsTelemetryTest, SendMessageSingleArg_LongMessage_DoesNotCrash)
{
    std::string longMsg(1024, 'X');
    // sendMessage takes char* — copy to writable buffer
    std::vector<char> buf(longMsg.begin(), longMsg.end());
    buf.push_back('\0');
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(buf.data()));
}

// ===========================================================================
// Telemetry::sendMessage(char* marker, char* message)
// ===========================================================================

TEST(UtilsTelemetryTest, SendMessageTwoArgs_DoesNotCrash)
{
    char marker[] = "THUNDER_TEST";
    char msg[]    = "payload";
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(marker, msg));
}

TEST(UtilsTelemetryTest, SendMessageTwoArgs_EmptyPayload_DoesNotCrash)
{
    char marker[] = "MARKER";
    char msg[]    = "";
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(marker, msg));
}

TEST(UtilsTelemetryTest, SendMessageTwoArgs_EmptyMarker_DoesNotCrash)
{
    char marker[] = "";
    char msg[]    = "some message";
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(marker, msg));
}

// ===========================================================================
// Telemetry::sendError
// ===========================================================================

TEST(UtilsTelemetryTest, SendError_SimpleFormat_DoesNotCrash)
{
    EXPECT_NO_THROW(Utils::Telemetry::sendError("error occurred: %s", "details"));
}

TEST(UtilsTelemetryTest, SendError_NoFormatArgs_DoesNotCrash)
{
    EXPECT_NO_THROW(Utils::Telemetry::sendError("simple error message"));
}

TEST(UtilsTelemetryTest, SendError_MultipleArgs_DoesNotCrash)
{
    EXPECT_NO_THROW(Utils::Telemetry::sendError("code=%d msg=%s", 42, "bad input"));
}

// ===========================================================================
// Telemetry — all methods callable without prior init
// ===========================================================================

TEST(UtilsTelemetryTest, SendMessageWithoutInit_DoesNotCrash)
{
    char msg[] = "no init";
    EXPECT_NO_THROW(Utils::Telemetry::sendMessage(msg));
}
