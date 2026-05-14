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
#include <cstdio>
#include <cstring>
#include "UtilsfileExists.h"

// Helper: create a temporary file and return its path
static std::string createTempFile()
{
    char tmpl[] = "/tmp/utils_fileexists_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) {
        close(fd);
    }
    return std::string(tmpl);
}

// ---------------------------------------------------------------------------
// fileExists — returns true when file is present, false otherwise
// ---------------------------------------------------------------------------

TEST(UtilsFileExistsTest, ExistingFile_ReturnsTrue)
{
    std::string path = createTempFile();
    EXPECT_TRUE(Utils::fileExists(path.c_str()));
    std::remove(path.c_str());
}

TEST(UtilsFileExistsTest, NonExistingFile_ReturnsFalse)
{
    EXPECT_FALSE(Utils::fileExists("/tmp/utils_definitely_does_not_exist_XYZ_123456789.txt"));
}

TEST(UtilsFileExistsTest, ExistingDirectory_ReturnsTrue)
{
    // /tmp always exists
    EXPECT_TRUE(Utils::fileExists("/tmp"));
}

TEST(UtilsFileExistsTest, EmptyPath_ReturnsFalse)
{
    EXPECT_FALSE(Utils::fileExists(""));
}

TEST(UtilsFileExistsTest, FileAfterRemoval_ReturnsFalse)
{
    std::string path = createTempFile();
    EXPECT_TRUE(Utils::fileExists(path.c_str()));
    std::remove(path.c_str());
    EXPECT_FALSE(Utils::fileExists(path.c_str()));
}

TEST(UtilsFileExistsTest, FileWithContent_ReturnsTrue)
{
    std::string path = createTempFile();
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fputs("test content", f);
        fclose(f);
    }
    EXPECT_TRUE(Utils::fileExists(path.c_str()));
    std::remove(path.c_str());
}

TEST(UtilsFileExistsTest, ProcFilesystem_ReturnsTrue)
{
    // /proc/self should always exist on Linux
    EXPECT_TRUE(Utils::fileExists("/proc/self"));
}
