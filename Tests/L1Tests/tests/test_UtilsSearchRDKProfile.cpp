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

// NOTE: UtilsSearchRDKProfile.h defines profile_t profileType and
// searchRdkProfile() at file scope (not inline/static).  This translation
// unit MUST be the only one that includes that header to avoid ODR violations.

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Helpers
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Module.h"

// Force-included mocks may pull in syslog wrapping; include WrapsMock so the
// impl pointer is set and the EXPECT_NE guard does not trip.
#include "WrapsMock.h"

#include "UtilsSearchRDKProfile.h"

#include <sys/stat.h>
#include <unistd.h>

using ::testing::NiceMock;

// searchRdkProfile() uses fopen/fgets/fclose on the hardcoded path
// /etc/device.properties.  Those libc functions are NOT wrapped in the
// helpers test build, so they go directly to real libc.  On a CI runner
// that file typically does not exist, making NOT_FOUND the expected result.

class SearchRdkProfileTest : public ::testing::Test {
protected:
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };

    void SetUp() override
    {
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
    }

    void TearDown() override
    {
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }
};

TEST_F(SearchRdkProfileTest, ReturnsValidProfileType)
{
    // The result must always be one of the defined enum values.
    profile_t result = searchRdkProfile();
    EXPECT_TRUE(result == NOT_FOUND || result == STB || result == TV);
}

TEST_F(SearchRdkProfileTest, ReturnsNotFoundWhenDevicePropertiesMissing)
{
    struct stat st;
    if (stat("/etc/device.properties", &st) == 0) {
        GTEST_SKIP() << "/etc/device.properties exists on this system; "
                        "skipping NOT_FOUND assertion";
    }
    EXPECT_EQ(NOT_FOUND, searchRdkProfile());
}

TEST_F(SearchRdkProfileTest, ReturnsExactlyOneValidEnumValueOnEachCall)
{
    // Repeated calls must return the same deterministic result for a given FS state.
    profile_t first  = searchRdkProfile();
    profile_t second = searchRdkProfile();
    EXPECT_EQ(first, second);
}

// ===========================================================================
// SearchDeviceName()
// Logic : opens /etc/device.properties (hardcoded path)
//         scans for a line containing "DEVICE_NAME="
//         strips trailing '\n', copies value into outDeviceName
//         Returns  0 on success
//         Returns -1 if file not found OR "DEVICE_NAME" line not found
// ===========================================================================

TEST(SearchDeviceNameTest, ReturnsNegativeOneWhenDevicePropertiesMissing)
{
    struct stat st;
    if (stat("/etc/device.properties", &st) == 0) {
        GTEST_SKIP() << "/etc/device.properties exists on this system; skipping";
    }
    char buf[256] = {};
    int result = SearchDeviceName(buf, sizeof(buf));
    EXPECT_EQ(-1, result);
}

TEST(SearchDeviceNameTest, SetsEmptyStringWhenDevicePropertiesMissing)
{
    struct stat st;
    if (stat("/etc/device.properties", &st) == 0) {
        GTEST_SKIP() << "/etc/device.properties exists on this system; skipping";
    }
    char buf[256];
    buf[0] = 'X'; // pre-fill to confirm it is explicitly nulled
    SearchDeviceName(buf, sizeof(buf));
    EXPECT_EQ('\0', buf[0]);
}

// ===========================================================================
// ReadJsonFileForKey()
// Logic : opens filePath, reads full file content into a string,
//         parses it with WPEFramework JsonObject::FromString(),
//         if key exists and its string value is non-empty -> sets value,
//         returns true; otherwise returns false
// Uses real temporary files — no mocks required.
// ===========================================================================

static std::string makeTempFileSRP(const std::string& content)
{
    char tmpPath[] = "/tmp/test_srp_XXXXXX";
    int fd = mkstemp(tmpPath);
    if (fd != -1)
    {
        (void)write(fd, content.c_str(), content.size());
        close(fd);
    }
    return std::string(tmpPath);
}

TEST(ReadJsonFileForKeyTest, ReturnsFalseForNonExistentFile)
{
    std::string value;
    bool result = ReadJsonFileForKey(
        "/tmp/no_such_srp_file_12345.json", "country", value);
    EXPECT_FALSE(result);
    EXPECT_TRUE(value.empty());
}

TEST(ReadJsonFileForKeyTest, ReturnsFalseForInvalidJson)
{
    std::string path = makeTempFileSRP("this is not valid json {{{{");
    std::string value;
    bool result = ReadJsonFileForKey(path.c_str(), "country", value);
    EXPECT_FALSE(result);
    remove(path.c_str());
}

TEST(ReadJsonFileForKeyTest, ReturnsFalseWhenKeyNotFound)
{
    std::string path = makeTempFileSRP("{\"other\":\"value\"}");
    std::string value;
    bool result = ReadJsonFileForKey(path.c_str(), "country", value);
    EXPECT_FALSE(result);
    EXPECT_TRUE(value.empty());
    remove(path.c_str());
}

TEST(ReadJsonFileForKeyTest, ReturnsFalseWhenKeyValueIsEmpty)
{
    std::string path = makeTempFileSRP("{\"country\":\"\"}");
    std::string value;
    bool result = ReadJsonFileForKey(path.c_str(), "country", value);
    EXPECT_FALSE(result);
    remove(path.c_str());
}

TEST(ReadJsonFileForKeyTest, ReturnsTrueAndSetsValueWhenKeyFound)
{
    std::string path = makeTempFileSRP("{\"country\":\"GB\"}");
    std::string value;
    bool result = ReadJsonFileForKey(path.c_str(), "country", value);
    EXPECT_TRUE(result);
    EXPECT_EQ(value, "GB");
    remove(path.c_str());
}

TEST(ReadJsonFileForKeyTest, ReturnsTrueForJsonWithMultipleKeys)
{
    std::string path = makeTempFileSRP(
        "{\"version\":\"1.0\",\"country\":\"US\",\"region\":\"NA\"}");
    std::string value;
    bool result = ReadJsonFileForKey(path.c_str(), "country", value);
    EXPECT_TRUE(result);
    EXPECT_EQ(value, "US");
    remove(path.c_str());
}

// ===========================================================================
// SearchPlatformCountryCode()
// Logic : calls ReadJsonFileForKey on two hardcoded paths:
//           /var/sky/build/vendorConfig.json  (key: "country")
//           /var/sky/build/buildConfig.json   (key: "country")
//         Returns true if either file has the key; false otherwise.
// On CI / developer machines neither file exists — always returns false.
// ===========================================================================

TEST(SearchPlatformCountryCodeTest, ReturnsFalseWhenNeitherConfigFileExists)
{
    struct stat st1, st2;
    if (stat("/var/sky/build/vendorConfig.json", &st1) == 0 ||
        stat("/var/sky/build/buildConfig.json",  &st2) == 0)
    {
        GTEST_SKIP() << "Sky build config found on this system; skipping";
    }
    std::string countryCode;
    bool result = SearchPlatformCountryCode(countryCode);
    EXPECT_FALSE(result);
    EXPECT_TRUE(countryCode.empty());
}

