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
#ifdef USE_IARM
#include "UtilsIarm.h"
#endif

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

namespace Utils
{
    namespace String
    {
        // locale-wise comparison
        template<typename charT>
        struct loc_equal {
            explicit loc_equal( const std::locale& loc ) : loc_(loc) {}
            bool operator()(charT ch1, charT ch2) {
                return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
            }
        private:
            const std::locale& loc_;
        };

        // Case-insensitive substring lookup.
        // Returns the substring position or -1
        // Example: int pos = find_substr_ci(string, substring, std::locale());
        template<typename T>
        int find_substr_ci( const T& string, const T& substring, const std::locale& loc = std::locale() )
        {
            typename T::const_iterator it = std::search( string.begin(), string.end(),
                                                         substring.begin(), substring.end(), loc_equal<typename T::value_type>(loc) );
            if ( it != string.end() ) return it - string.begin();
            else return -1; // not found
        }

        // Case-insensitive substring inclusion lookup.
        // Example: if (Utils::String::contains(result, processName)) {..}
        template<typename T>
        bool contains( const T& string, const T& substring, const std::locale& loc = std::locale() )
        {
            int pos = find_substr_ci(string, substring, loc);
            return pos != -1;
        }

        // Case-insensitive substring inclusion lookup.
        // Example: if(Utils::String::contains(tmp, "grep -i")) {..}
        template<typename T>
        bool contains( const T& string, const char* c_substring, const std::locale& loc = std::locale() )
        {
            std::string substring(c_substring);
            int pos = find_substr_ci(string, substring, loc);
            return pos != -1;
        }

        // Looking for an element in a vector of elements
        template<typename T>
        bool contains(const std::vector<T>& vec, const T& label)
        {
            bool res = false;
            const typename std::vector<T>::iterator it;
            for(T lbl : vec)
            {
                if (label == lbl)
                    res = true;
            }
            return res;
        }

        // Case-insensitive string comparison
        // returns true if the strings are equal, otherwise returns false
        // Example: if (Utils::String::equal(line, provisionType)) {..}
        template<typename T>
        bool equal(const T& string, const T& string2, const std::locale& loc = std::locale() )
        {
            int pos = find_substr_ci(string, string2, loc);
            bool res = (pos == 0) && (string.length() == string2.length());
            return res;
        }

        // Case-insensitive string comparison
        // returns true if the strings are equal, otherwise returns false
        // Example: if(Utils::String::equal(line,"CRYPTANIUM")) {..}
        template<typename T>
        bool equal(const T& string, const char* c_string2, const std::locale& loc = std::locale() )
        {
            std::string string2(c_string2);
            int pos = find_substr_ci(string, string2, loc);
            bool res = (pos == 0) && (string.length() == string2.length());
            return res;
        }

        // Trim space characters (' ', '\n', '\v', '\f', \r') on the left side of string
        inline void ltrim(std::string &s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                return !std::isspace(ch);
            }));
        }

        // Trim space characters (' ', '\n', '\v', '\f', \r') on the right side of string
        inline void rtrim(std::string &s) {
            s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), s.end());
        }

        // Trim space characters (' ', '\n', '\v', '\f', \r') on both sides of string
        inline void trim(std::string &s) {
            ltrim(s);
            rtrim(s);
        }

        // Replace all occurrences of substring 'before' with the substring 'after', e.g. replace("foo-bar-foo", "foo", "bar") => "bar-bar-bar"
        // Replace is case-sensitive
        inline void replace_substr(std::string& str, const std::string& before, const std::string& after) {
            if(before.empty())
                return;
            size_t start_pos = 0;
            while((start_pos = str.find(before, start_pos)) != std::string::npos) {
                str.replace(start_pos, before.length(), after);
                start_pos += after.length();
            }
        }

        // Split string s into a vector of strings using the supplied delimiter
        inline void split(std::vector<std::string> &stringList, std::string &s, std::string delimiters)
        {
            size_t current = 0;
            size_t next;
            do
            {
                next = s.find_first_of( delimiters, current );
                stringList.push_back(s.substr( current, next - current ));
                if (next == string::npos) {
                    break;  // Exit before overflow
                }
                current = next + 1;
            }
            while (true);
        }

        //Implode a vector of strings into a string
        inline std::string& implode(const std::vector<std::string> v, std::string& s, const std::string delim = " ")
        {
            std::ostringstream imploded;
            const char* delim_c = delim.c_str();
            size_t dl = strlen(delim_c);

            std::copy(v.begin(), v.end(),
                      std::ostream_iterator<std::string>(imploded, delim_c));
            s = imploded.str();
            s.erase(s.end() - dl, s.end());
            return s;
        }

       /**
        * @brief           :Read the property value from the given string
        *                  If the property value have one or more values so it will copy to the vector list.
        * @param1[in]      : str - the given string.
        * @param2[out]     : stringList - property values will fill here.
        * @return          : bool - return true if the property is found and its value are successfully read.
        *                         - false if the property is not found or if there is any failure during reading.
        *
        */
        bool readPropertyValue(const std::string &str, std::vector<std::string> &stringList);
    }


    /***
     * @brief	: Checks that file exists
     * @param1[in]	: pFileName name of file
     * @return		: true if file exists.
     */
    inline bool fileExists(const char *pFileName)
    {
        struct stat fileStat;
        return stat(pFileName, &fileStat) == 0;
    }

    /**
     * @brief           :Read the property value from the given file if the given property is matches.
     *                  If the property value have one or more values so it will copy to the vector list.
     * @param1[in]      : pfilename - filename with path to parse the file.
     * @param2[in]      : property - property name need to be given.
     * @param3[out]     : stringList - property values will fill here.
     * @return          : bool - return true if the property is found and its value are successfully read.
     *                         - return false if the property is not found or if there is any failure during reading.
     *
     */
    bool readPropertyFromFile(const char* pfilename, const std::string& property, std::vector<std::string> &stringList);

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
