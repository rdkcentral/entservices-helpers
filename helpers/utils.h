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
#include <curl/curl.h>
#include <mutex>

// std
#include <string>

#define SERVER_DETAILS  "127.0.0.1:9998"
#define MAX_STRING_LENGTH 2048

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

//using namespace WPEFramework;
using namespace std;

namespace Helpers
{

    using namespace WPEFramework;
    static inline size_t writeCurlResponse(void *ptr, size_t size, size_t nmemb, string stream)
    {
        size_t realsize = size * nmemb;
        string temp(static_cast<const char*>(ptr), realsize);
        stream.append(temp);
        return realsize;
    }

    struct SecurityToken
    {
        static inline bool isThunderSecurityConfigured()
        {
            bool configured = false;
            long http_code = 0;
            std::string jsonResp;
            CURL *curl_handle = NULL;
            CURLcode res = CURLE_OK;
            curl_handle = curl_easy_init();
            string serialNumber = "";
            string url = "http://127.0.0.1:9998/Service/Controller/Configuration/Controller";
            if (curl_handle &&
                !curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str()) &&
                !curl_easy_setopt(curl_handle, CURLOPT_HTTPGET,1) &&
                !curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1) && //when redirected, follow the redirections
                !curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeCurlResponse) &&
                !curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &jsonResp)) {

                res = curl_easy_perform(curl_handle);
                if(curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK)
                {
                    std::cout << "curl_easy_getinfo failed\n";
                }
                std::cout << "Thunder Controller Configuration ret: " << res << " http response code: " << http_code << std::endl;
            }
            else
            {
                std::cout << "Could not perform curl to read Thunder Controller Configuration\n";
            }

            if (curl_handle) {
                curl_easy_cleanup(curl_handle);
            }
            if ((res == CURLE_OK) && (http_code == 200))
            {
                //check for "Security" in response
                JsonObject responseJson = JsonObject(jsonResp);
                if (responseJson.HasLabel("subsystems"))
                {
                    const JsonArray subsystemList = responseJson["subsystems"].Array();
                    for (int i=0; i<subsystemList.Length(); i++)
                    {
                        string subsystem = subsystemList[i].String();
                        if (subsystem == "Security")
                        {
                            configured = true;
                            break;
                        }
                    }
                }
            }
            return configured;
        }
    };

#ifdef USE_THUNDER_COMMUNICATION
    // Thunder Plugin Communication
    inline std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> > getThunderControllerClient(std::string callsign="")
    {
        Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));
        std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> > thunderClient = make_shared<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> >(callsign.c_str(), "",false);

        return thunderClient;
    }

    inline bool isPluginActivated(const char* callSign)
    {
        string method = "status@" + string(callSign);
        Core::JSON::ArrayType<PluginHost::MetaData::Service> joResult;
        uint32_t status = getThunderControllerClient()->Get<Core::JSON::ArrayType<PluginHost::MetaData::Service> >(2000, method.c_str(),joResult);
        bool pluginActivated = false;
        if (status == Core::ERROR_NONE)
        {
            LOGINFO("Getting status for callSign %s, result: %s", callSign, joResult[0].JSONState.Data().c_str());
            pluginActivated = joResult[0].JSONState == PluginHost::IShell::ACTIVATED;
        }
        else
        {
            LOGWARN("Getting status for callSign %s, status: %d", callSign, status);
        }

        if(!pluginActivated){
            LOGWARN("Plugin %s is not active", callSign);
        } else {
            LOGINFO("Plugin %s is active ", callSign);
        }
        return pluginActivated;
    }

    inline void activatePlugin(const char* callSign)
    {
        JsonObject joParams;
        joParams.Set("callsign",callSign);
        JsonObject joResult;

        if(!isPluginActivated(callSign))
        {
            LOGINFO("Activating %s", callSign);
            uint32_t status = getThunderControllerClient()->Invoke<JsonObject, JsonObject>(2000, "activate", joParams, joResult);
            string strParams;
            string strResult;
            joParams.ToString(strParams);
            joResult.ToString(strResult);
            LOGINFO("Called method %s, with params %s, status: %d, result: %s"
                    , "activate"
                    , C_STR(strParams)
                    , status
                    , C_STR(strResult));
            if (status == Core::ERROR_NONE)
            {
                LOGINFO("%s Plugin activation status ret: %d ", callSign, status);
            }
        }
    }

#endif

#ifndef DONT_USE_TR181
    inline bool getTR181Config(const char* callerID, const char* paramName, TR181_ParamData_t& paramOutput)
    {
        tr181ErrorCode_t status;

        memset(&paramOutput, 0, sizeof(paramOutput));
        status = getParam((char*)callerID, (char*)paramName, &paramOutput);
        if (tr181Success == status )
        {
            std::cout << "getTR181Config for  " << paramName << " is " << paramOutput.value << std::endl;
        } else {
            std::cout << "Could not get param value for " << paramName  << std::endl;
        }
        return (tr181Success == status );
    }
#endif
}
