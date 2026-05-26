/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply :
*
* Copyright 2025 RDK Management
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

#include "PluginInterfaceBuilder.h"

//Base Retry Interval
#define PWRMGR_RETRY_INTERVAL_CPC                      (300)

//CPC component specific Retry Interval
#define FRONTPANEL_PWRMGR_RETRY_INTERVAL           PWRMGR_RETRY_INTERVAL_CPC
#define DEVICEBRIDGE_PWRMGR_RETRY_INTERVAL         PWRMGR_RETRY_INTERVAL_CPC
#define SECMANAGER_PWRMGR_RETRY_INTERVAL           PWRMGR_RETRY_INTERVAL_CPC
#define DEVICEPROVISIONING_PWRMGR_RETRY_INTERVAL   PWRMGR_RETRY_INTERVAL_CPC


//Base Retry Interval
#define PWRMGR_RETRY_COUNT_CPC                         (25)

//Component Specific  Retry Interval
#define FRONTPANEL_PWRMGR_RETRY_COUNT              PWRMGR_RETRY_COUNT_CPC 
#define DEVICEBRIDGE_PWRMGR_RETRY_COUNT            PWRMGR_RETRY_COUNT_CPC
#define SECMANAGER_PWRMGR_RETRY_COUNT              PWRMGR_RETRY_COUNT_CPC
#define DEVICEPROVISIONING_PWRMGR_RETRY_COUNT      PWRMGR_RETRY_COUNT_CPC

using PowerManagerInterfaceBuilder = WPEFramework::Plugin::PluginInterfaceBuilder<WPEFramework::Exchange::IPowerManager>;
using PowerManagerInterfaceRef = WPEFramework::Plugin::PluginInterfaceRef<WPEFramework::Exchange::IPowerManager>;
