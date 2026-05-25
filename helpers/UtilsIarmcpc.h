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
#include <sstream>
#include <iterator>
#include <vector>
#include <algorithm>
#include "tracing/Logging.h"
#include <syscall.h>
#include <plugins/plugins.h>
#include "UtilsString.h"
#include "UtilsfileExists.h"
#include "UtilsgetFileContent.h"
#include <mutex>

#ifndef DONT_USE_TR181
#include "tr181api.h"
#endif

#ifdef HAS_API_POWERSTATE
#include "libIBus.h"
#endif

#ifdef USE_THUNDER_COMMUNICATION
#include <curl/curl.h>
#endif

// std
#include <string>

#define SERVER_DETAILS  "127.0.0.1:9998"
#define MAX_STRING_LENGTH 2048

using namespace std;

namespace Helpers
{
    using namespace WPEFramework;
#ifdef HAS_API_POWERSTATE
struct IARM {
   static constexpr const char* NAME = "Thunder_Plugins";

   static bool isConnected() {
        IARM_Result_t res;
        int isRegistered = 0;
        res = IARM_Bus_IsConnected(NAME, &isRegistered);
        LOGINFO("IARM_Bus_IsConnected: %d (%d)", res, isRegistered);

        return (isRegistered == 1);
    }

    static bool init() {
        IARM_Result_t res;
        bool result = false;

        if (isConnected()) {
            LOGINFO("IARM already connected");
            result = true;
        } else {
            res = IARM_Bus_Init(NAME);
            LOGINFO("IARM_Bus_Init: %d", res);
            if (res == IARM_RESULT_SUCCESS ||
                res == IARM_RESULT_INVALID_STATE /* already inited or connected */) {

                res = IARM_Bus_Connect();
                LOGINFO("IARM_Bus_Connect: %d", res);
                if (res == IARM_RESULT_SUCCESS ||
                    res == IARM_RESULT_INVALID_STATE /* already connected or not inited */) {

                    result = isConnected();
                } else {
                    LOGERR("IARM_Bus_Connect failure: %d", res);
                }
            } else {
                LOGERR("IARM_Bus_Init failure: %d", res);
            }
        }

        return result;
    }

    static std::string formatIARMResult(IARM_Result_t result)
    {
        switch (result) {
            case IARM_RESULT_SUCCESS:       return std::string("IARM_RESULT_SUCCESS [success]");
            case IARM_RESULT_INVALID_PARAM: return std::string("IARM_RESULT_INVALID_PARAM [invalid input parameter]");
            case IARM_RESULT_INVALID_STATE: return std::string("IARM_RESULT_INVALID_STATE [invalid state encountered]");
            case IARM_RESULT_IPCCORE_FAIL:  return std::string("IARM_RESULT_IPCORE_FAIL [underlying IPC failure]");
            case IARM_RESULT_OOM:           return std::string("IARM_RESULT_OOM [out of memory]");
        default:
            std::ostringstream tmp;
            tmp << result << " [unknown IARM_Result_t]";
            return tmp.str();
         }
    }

};
#endif // HAS_API_POWERSTATE
}
