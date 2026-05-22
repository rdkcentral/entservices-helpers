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
#include "tr181api.h"
#include <plugins/plugins.h>
#include "UtilsString.h"
#include "UtilsfileExists.h"
#include "UtilsgetFileContent.h"

// std
#include <string>

#define SERVER_DETAILS  "127.0.0.1:9998"

#define UNUSED(expr)(void)(expr)
#define C_STR(x) (x).c_str()

enum LogLevel {FATAL_LEVEL = 0, ERROR_LEVEL, WARNING_LEVEL, INFO_LEVEL, DEBUG_LEVEL};

static int gDefaultLogLevel = DEBUG_LEVEL;

#define LOGDBG(fmt, ...) do { if(gDefaultLogLevel >= DEBUG_LEVEL) { fprintf(stderr, "[%d] DEBUG [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); }} while (0)
#define LOGINFO(fmt, ...) do { if(gDefaultLogLevel >= INFO_LEVEL) { fprintf(stderr, "[%d] INFO [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); }} while (0)
#define LOGWARN(fmt, ...) do { if(gDefaultLogLevel >= WARNING_LEVEL) { fprintf(stderr, "[%d] WARN [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); }} while (0)
#define LOGERR(fmt, ...) do { if(gDefaultLogLevel >= ERROR_LEVEL) { fprintf(stderr, "[%d] ERROR [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); }} while (0)

#define LOGDBGMETHOD() do { std::string json; parameters.ToString(json); LOGDBG( "params=%s", json.c_str() ); } while (0)
#define LOGINFOMETHOD() do { std::string json; parameters.ToString(json); LOGINFO( "params=%s", json.c_str() ); } while (0)
#define LOGTRACEMETHODFIN() do { std::string json; response.ToString(json); LOGDBG( "response=%s", json.c_str() ); } while (0)

#define LOG_DEVICE_EXCEPTION0() LOGWARN("Exception caught: code=%d message=%s", err.getCode(), err.what());
#define LOG_DEVICE_EXCEPTION1(param1) LOGWARN("Exception caught" #param1 "=%s code=%d message=%s", param1.c_str(), err.getCode(), err.what());
#define LOG_DEVICE_EXCEPTION2(param1, param2) LOGWARN("Exception caught " #param1 "=%s " #param2 "=%s code=%d message=%s", param1.c_str(), param2.c_str(), err.getCode(), err.what());

template<typename T> inline T min(T x, T y) { return (x < y ? x : y); }
template<typename T> inline T max(T x, T y) { return (x > y ? x : y); }
template<typename T> inline T clamp(T x, T lo, T hi) { return max(min(x, hi), lo); }

//this set of macros are used in the method handlers to make the code more consistent and easier to read
#define vectorSet(v,s) \
    if (find(begin(v), end(v), s) == end(v)) \
        v.emplace_back(s);

#define stringContains(s1,s2) \
    (search(s1.begin(), s1.end(), s2, s2+strlen(s2), \
        [](char c1, char c2){ \
            return toupper(c1) == toupper(c2); \
    }) != s1.end())

#define returnResponse(success) \
    { \
        response["success"] = success; \
        LOGTRACEMETHODFIN(); \
        return (Core::ERROR_NONE); \
    }

#define returnResponse_noLog(success) \
    { \
        bool successBoolean = success; \
        response["success"] = successBoolean; \
        return (successBoolean ? Core::ERROR_NONE : Core::ERROR_GENERAL); \
    }

#define returnIfWrongApiVersion(version)\
    if(m_apiVersionNumber < version)\
    {\
        LOGWARN("method %s not supported. version required=%u actual=%u", __FUNCTION__, version, m_apiVersionNumber);\
        returnResponse(false);\
    }

#define returnIfParamNotFound(param, name)\
    if (!param.HasLabel(name))\
    {\
        LOGERR("No argument '%s'", name);\
        returnResponse(false);\
    }

#define returnIfStringParamNotFound(param, name)\
    if (!param.HasLabel(name) || param[name].Content() != Core::JSON::Variant::type::STRING)\
    {\
        LOGERR("No argument '%s' or it has incorrect type", name);\
        returnResponse(false);\
    }

#define returnIfStringParamEmpty(param, name)\
    if (!param.HasLabel(name) || param[name].Content() != Core::JSON::Variant::type::STRING || param[name].String().empty())\
    {\
        LOGERR("No argument '%s' or it has incorrect type", name);\
        returnResponse(false);\
    }

#define returnIfBooleanParamNotFound(param, name)\
    if (!param.HasLabel(name) || param[name].Content() != Core::JSON::Variant::type::BOOLEAN)\
    {\
        LOGERR("No argument '%s' or it has incorrect type", name);\
        returnResponse(false);\
    }

#define returnIfArrayParamNotFound(param, name)\
    if (!param.HasLabel(name) || param[name].Content() != Core::JSON::Variant::type::ARRAY)\
    {\
        LOGERR("No argument '%s' or it has incorrect type", name);\
        returnResponse(false);\
    }

#define returnIfNumberParamNotFound(param, name)\
    if (!param.HasLabel(name) || param[name].Content() != Core::JSON::Variant::type::NUMBER)\
    {\
        LOGERR("No argument '%s' or it has incorrect type", name);\
        returnResponse(false);\
    }

#define returnIfObjectParamNotFound(param, name)\
    if (!param.HasLabel(name) || param[name].Content() != Core::JSON::Variant::type::OBJECT)\
    {\
        LOGERR("No argument '%s' or it has incorrect type", name);\
        returnResponse(false);\
    }

#define sendNotify(event,params)\
    std::string json;\
    params.ToString(json);\
    LOGINFO("Notify %s %s", event, json.c_str());\
    Notify(event,params);

#define sendNotifyMaskParameters(event,params)\
    std::string json; \
    params.ToString(json); \
    LOGINFO("Notify %s <***>", event); \
    Notify(event,params);

#define getNumberParameter(paramName, param) {\
    if (Core::JSON::Variant::type::NUMBER == parameters[paramName].Content()) \
        param = parameters[paramName].Number();\
    else\
        try { param = std::stoi( parameters[paramName].String()); }\
        catch (...) { param = 0; }\
}

#define getNumberParameterObject(parameters, paramName, param) {\
    if (Core::JSON::Variant::type::NUMBER == parameters[paramName].Content()) \
        param = parameters[paramName].Number();\
    else\
        try {param = std::stoi( parameters[paramName].String());}\
        catch (...) { param = 0; }\
}

#define getBoolParameter(paramName, param) {\
    if (Core::JSON::Variant::type::BOOLEAN == parameters[paramName].Content()) \
        param = parameters[paramName].Boolean();\
    else\
        param = parameters[paramName].String() == "true" || parameters[paramName].String() == "1";\
}

#define getStringParameter(paramName, param) {\
    if (Core::JSON::Variant::type::STRING == parameters[paramName].Content()) \
        param = parameters[paramName].String();\
}

namespace Helpers
{

    struct SecurityToken
    {
        static bool isThunderSecurityConfigured();
    };

#ifdef USE_THUNDER_COMMUNICATION
    // Thunder Plugin Communication
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> getThunderControllerClient(std::string callsign="");

    void activatePlugin(const char* callSign);

    bool isPluginActivated(const char* callSign);
#endif

#ifndef DONT_USE_TR181
    bool getTR181Config(const char* callerID, const char* paramName, TR181_ParamData_t& paramOutput);
#endif
}
