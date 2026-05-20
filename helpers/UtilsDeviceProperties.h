/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
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

#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <core/JSON.h>
#include "utils.h"

using namespace std;
#define RDK_PROFILE "RDK_PROFILE="
#define PROFILE_TV "TV"
#define PROFILE_STB "STB"

typedef enum profile {
    NOT_FOUND = -1,
    STB = 0,
    TV,
    MAX
} profile_t;

profile_t profileType = NOT_FOUND;

profile_t searchRdkProfile(void) {
    const char* devPropPath = "/etc/device.properties";
    char line[256], *rdkProfile = NULL;
    profile_t ret = NOT_FOUND;
    FILE* file;

    file = fopen(devPropPath, "r");
    if (file == NULL) {
        printf("Failed to open device properties file: %s\n", devPropPath);
        return NOT_FOUND;
    }

    while (fgets(line, sizeof(line), file)) {
        rdkProfile = strstr(line, RDK_PROFILE);
        if (rdkProfile != NULL) {
            rdkProfile += strlen(RDK_PROFILE); // Move past the 'RDK_PROFILE='
            printf("Found RDK_PROFILE: %s \n", rdkProfile);
            break;
        }
    }

    if (rdkProfile != NULL) {
        if (strncmp(rdkProfile, PROFILE_TV, strlen(PROFILE_TV)) == 0) {
            ret = TV;
            printf("Resulted RDK_PROFILE=TV \n");
        } else if (strncmp(rdkProfile, PROFILE_STB, strlen(PROFILE_STB)) == 0) {
            ret = STB;
            printf("Resulted RDK_PROFILE=STB \n");
        }
    } else {
        printf("Found RDK_PROFILE: NOT_FOUND \n");
        ret = NOT_FOUND;
    }
    fclose(file);
    return ret;
}

int SearchDeviceName(char* outDeviceName, size_t maxLen) {
    const char* devPropPath = "/etc/device.properties";
    char line[256];
    FILE* file;

    file = fopen(devPropPath, "r");
    if (file == NULL) {
        printf("Failed to open device properties file: %s\n", devPropPath);
        outDeviceName[0] = '\0';
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char* deviceName = strstr(line, "DEVICE_NAME=");
        if (deviceName != NULL) {
            deviceName += strlen("DEVICE_NAME=");
            size_t len = strlen(deviceName);
            if (len > 0 && deviceName[len - 1] == '\n') {
                deviceName[len - 1] = '\0';
            }
            strncpy(outDeviceName, deviceName, maxLen - 1);
            outDeviceName[maxLen - 1] = '\0';
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    outDeviceName[0] = '\0';
    return -1;
}

bool ReadJsonFileForKey(const char* filePath, const char* key, string &value) {
    std::string content;
    {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            printf("Failed to open JSON file: %s\n", filePath);
            return false;
        }
        // read the full content of the file
        content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // file is closed automatically here when going out of scope
    }
    JsonObject jsonObj;
    if (!jsonObj.FromString(content)) {
        printf("Failed to parse JSON content from file: %s\n", filePath);
        return false;
    }
    if (jsonObj.HasLabel(key) && jsonObj[key].String().empty() == false) {
        value = jsonObj[key].String();
        return true;
    }

    return false;
}

bool SearchPlatformCountryCode(string &countryCode) {
    const char* vendorFilePath = "/var/sky/build/vendorConfig.json";
    const char* buildFilePath = "/var/sky/build/buildConfig.json";

    if (ReadJsonFileForKey(vendorFilePath, "country", countryCode) ||
        ReadJsonFileForKey(buildFilePath, "country", countryCode)) {
        return true;
    }

    return false;
}