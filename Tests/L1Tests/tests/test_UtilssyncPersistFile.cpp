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
#include "WrapsMock.h"
#include "UtilssyncPersistFile.h"

#include <fstream>
#include <string>

using namespace WPEFramework;

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

// __real_unlink bypasses the --wrap redirect so TearDown can delete temp files.
extern "C" int __real_unlink(const char* path);

static const std::string SYNC_TEMP_FILE   = "/tmp/test_syncPersistFile_tmp.bin";
static const std::string PERSIST_JSON_FILE = "/tmp/test_persistJsonSettings.json";

class SyncPersistFileTest : public ::testing::Test {
protected:
    NiceMock<WrapsImplMock>* p_wrapsImplMock { nullptr };

    void SetUp() override
    {
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(*p_wrapsImplMock, unlink(_))
            .WillByDefault(Invoke(__real_unlink));
        // Start clean
        (void)::unlink(SYNC_TEMP_FILE.c_str());
        (void)::unlink(PERSIST_JSON_FILE.c_str());
    }

    void TearDown() override
    {
        (void)::unlink(SYNC_TEMP_FILE.c_str());
        (void)::unlink(PERSIST_JSON_FILE.c_str());
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;
    }

    // Helper: create a simple file with some content.
    void createFile(const std::string& path, const std::string& content = "data")
    {
        std::ofstream f(path);
        f << content;
    }
};

// ---------------------------------------------------------------------------
// syncPersistFile
// ---------------------------------------------------------------------------

TEST_F(SyncPersistFileTest, SyncNonExistentFileDoesNotCrash)
{
    // fopen returns NULL for missing file; function prints and returns early.
    EXPECT_NO_FATAL_FAILURE(Utils::syncPersistFile("/tmp/no_such_file_xyz.bin"));
}

TEST_F(SyncPersistFileTest, SyncExistingFileSucceeds)
{
    createFile(SYNC_TEMP_FILE);
    EXPECT_NO_FATAL_FAILURE(Utils::syncPersistFile(SYNC_TEMP_FILE));
}

TEST_F(SyncPersistFileTest, SyncExistingFileWithContentSucceeds)
{
    createFile(SYNC_TEMP_FILE, "some important data");
    EXPECT_NO_FATAL_FAILURE(Utils::syncPersistFile(SYNC_TEMP_FILE));
    // File must still exist after sync (fopen "r" does not modify the file)
    std::ifstream f(SYNC_TEMP_FILE);
    EXPECT_TRUE(f.good());
}

TEST_F(SyncPersistFileTest, SyncEmptyFileSucceeds)
{
    createFile(SYNC_TEMP_FILE, "");
    EXPECT_NO_FATAL_FAILURE(Utils::syncPersistFile(SYNC_TEMP_FILE));
}

// ---------------------------------------------------------------------------
// persistJsonSettings
// ---------------------------------------------------------------------------

TEST_F(SyncPersistFileTest, PersistJsonSettingsCreatesFile)
{
    JsonValue value("testValue");
    EXPECT_NO_FATAL_FAILURE(
        Utils::persistJsonSettings(PERSIST_JSON_FILE, "testKey", value));
    std::ifstream f(PERSIST_JSON_FILE);
    EXPECT_TRUE(f.good());
}

TEST_F(SyncPersistFileTest, PersistJsonSettingsWritesKeyValue)
{
    JsonValue value("hello");
    Utils::persistJsonSettings(PERSIST_JSON_FILE, "greeting", value);

    // Read back the raw file content and verify the key is present
    std::ifstream f(PERSIST_JSON_FILE);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, content.find("greeting"));
}

TEST_F(SyncPersistFileTest, PersistJsonSettingsOverwritesExistingKey)
{
    JsonValue val1("first");
    JsonValue val2("second");
    Utils::persistJsonSettings(PERSIST_JSON_FILE, "myKey", val1);
    Utils::persistJsonSettings(PERSIST_JSON_FILE, "myKey", val2);

    std::ifstream f(PERSIST_JSON_FILE);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, content.find("second"));
}

TEST_F(SyncPersistFileTest, PersistJsonSettingsIntegerValue)
{
    JsonValue value(static_cast<int32_t>(42));
    EXPECT_NO_FATAL_FAILURE(
        Utils::persistJsonSettings(PERSIST_JSON_FILE, "count", value));
    std::ifstream f(PERSIST_JSON_FILE);
    EXPECT_TRUE(f.good());
}
