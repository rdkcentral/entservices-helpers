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

// Main GTest entry point for entservices-helpers L1 tests.
// Individual test suites are compiled from their own translation units:
//   test_UtilsBIT.cpp              — bit manipulation macros (UtilsBIT.h)
//   test_UtilsCStr.cpp             — C_STR macro (UtilsCStr.h)
//   test_UtilsUnused.cpp           — UNUSED macro (UtilsUnused.h)
//   test_UtilsfileExists.cpp       — Utils::fileExists (UtilsfileExists.h)
//   test_UtilsisValidInt.cpp       — Utils::isValidInt/isValidUnsignedInt
//   test_UtilsString.cpp           — Utils::String namespace functions
//   test_UtilsFile.cpp             — Utils::getLastLine (UtilsFile.h)
//   test_UtilsgetFileContent.cpp   — readPropertyFromFile, readFileContent, …
//   test_UtilsLogging.cpp          — logging macro smoke tests
//   test_UtilsThreadRAII.cpp       — Utils::ThreadRAII class
//   test_UtilsTelemetry.cpp        — Utils::Telemetry struct
//   test_UtilsSearchRDKProfile.cpp — searchRdkProfile / profile_t

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
