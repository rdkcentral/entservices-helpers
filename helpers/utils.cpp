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

/**
 *  Utility functions used in plugins.
 *
 */

#include <string.h>
#include <sstream>
#include "utils.h"
#include "libIBus.h"
#include <curl/curl.h>
#include <mutex>

#define MAX_STRING_LENGTH 2048
using namespace WPEFramework;
using namespace std;

const char* Utils::IARM::NAME = "Thunder_Plugins";

bool Utils::IARM::isConnected() {
    IARM_Result_t res;
    int isRegistered = 0;
    res = IARM_Bus_IsConnected(NAME, &isRegistered);
    LOGINFO("IARM_Bus_IsConnected: %d (%d)", res, isRegistered);

    return (isRegistered == 1);
}

bool Utils::IARM::init() {
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

std::string Utils::formatIARMResult(IARM_Result_t result)
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

bool Utils::String::readPropertyValue(const std::string &str, std::vector<std::string> &stringList)
{
    std::size_t found_start_pos = 0;
    std::string propertyValue = "";
    bool ret_value = false;

    if (str.empty())
    {
        LOGERR("str - input line is empty");
    }
    // Extract the value after the '='
    else if ((found_start_pos = str.find("=")) != std::string::npos)
    {
       // Extract the value after the '(' if present
       if (str[found_start_pos+1] == '(')
       {
           propertyValue = str.substr(found_start_pos + 2);
       }
       else
       {
           propertyValue = str.substr(found_start_pos + 1);
       }
    }
    else
    {
        LOGERR("Property value not found in the input line");
    }

    if (!propertyValue.empty())
    {
        /* Remove new line character from end of the string if it have */
        if ((propertyValue.back() == '\r') || (propertyValue.back() == '\n'))
        {
            propertyValue.pop_back();
        }

        /* Remove ')' character from end of the string if it have */
        if (propertyValue.back() == ')')
        {
            propertyValue.pop_back();
        }

        split(stringList, propertyValue, " ");

        ret_value = true;
    }
    return ret_value;
}

bool Utils::readPropertyFromFile(const char* pfilename, const std::string& property, std::vector<std::string> &stringList)
{
    std::ifstream file(pfilename);
    std::string line = "";
    bool ret_value = false;

    if (file.is_open())
    {
       while (std::getline(file, line))
       {
           if (!line.empty())
           {
               // Skip lines that start with '#' (single-line comments) and property not found
               if ((line[0] != '#') && (line.find(property) == 0))
               {
                   if (false == (ret_value = Utils::String::readPropertyValue(line, stringList)))
                   {
                       LOGERR("Property value is not found");
                   }
                   break;
               }
           }
       }
       file.close();
    }
    else
    {
       LOGERR("Failed to open the file");
    }

    return ret_value;
}

#ifndef DONT_USE_TR181
bool Utils::getTR181Config(const char* callerID, const char* paramName, TR181_ParamData_t& paramOutput)
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

static size_t writeCurlResponse(void *ptr, size_t size, size_t nmemb, string stream)
{
    size_t realsize = size * nmemb;
    string temp(static_cast<const char*>(ptr), realsize);
    stream.append(temp);
    return realsize;
}

bool Utils::SecurityToken::isThunderSecurityConfigured()
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

#ifdef USE_THUNDER_COMMUNICATION
std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> > Utils::getThunderControllerClient(std::string callsign)
{
    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> > thunderClient = make_shared<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement> >(callsign.c_str(), "",false);

    return thunderClient;
}

void Utils::activatePlugin(const char* callSign)
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

bool Utils::isPluginActivated(const char* callSign)
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

#endif
