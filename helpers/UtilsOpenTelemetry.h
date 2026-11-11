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

#pragma once

#include <core/core.h>
#include <messaging/messaging.h>
#include <open_telemetry/open_telemetry.h>

#include <mutex>

namespace Utils
{
    class OpenTelemetry
    {

    private:
#if defined(THUNDER_VERSION) && (THUNDER_VERSION < 5)
        static constexpr TCHAR const* filename = "/etc/WPEFramework/opentelemetry.json";
#else
        static constexpr TCHAR const* filename = "/etc/Thunder/opentelemetry.json";
#endif

    private:
        static string getCollectorEndpoint()
        {
           string endpoint{_T("http://127.0.0.1:4318")};

            const char* env_endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
            if (env_endpoint != nullptr) {
                endpoint = env_endpoint;
            } 
        
            return endpoint;
        }

        static string getTracerEndpoint()
        {
            return  getCollectorEndpoint() + _T("/v1/traces");
        }
        static string getLoggerEndpoint()
        {
            return  getCollectorEndpoint() + _T("/v1/logs");
        }
        static string getMetricsEndpoint()
        {
            return  getCollectorEndpoint() + _T("/v1/metrics");
        }


    public:

        static void init()
        {
            static std::once_flag flag;
            std::call_once(flag, [](){
                Telemetry::OpenTelemetry::Config telemetryConfig;
                Core::OptionalType<Core::JSON::Error> error;
                Core::File file(filename);
                if (file.Open(true) == true) {
                    telemetryConfig.IElement::FromFile(file, error);
                    if(error.IsSet() == false) {
                        if(telemetryConfig.Exporter == Telemetry::OpenTelemetry::ExporterType::HTTP) {
                            if((telemetryConfig.TracerEndpoint.IsSet() == false) || (telemetryConfig.TracerEndpoint.Value().empty() == true)) {
                                telemetryConfig.TracerEndpoint = getTracerEndpoint();
                            }
                            if((telemetryConfig.Logger.Endpoint.IsSet() == false) || (telemetryConfig.Logger.Endpoint.Value().empty() == true)) {
                                telemetryConfig.Logger.Endpoint = getLoggerEndpoint();
                            }
                            if((telemetryConfig.Metrics.Endpoint.IsSet() == false) || (telemetryConfig.Metrics.Endpoint.Value().empty() == true)) {
                                telemetryConfig.Metrics.Endpoint = getMetricsEndpoint();
                            }
                        }
                        Telemetry::OpenTelemetry::Configure(telemetryConfig);
                    } else {
                        SYSLOG(Logging::Error, (_T("Error parsing Thunder open telemetry file: %s"), error.Value().Message().c_str()));
                    }
                } else {
                    SYSLOG(Logging::Error, (_T("Could not load Thunder open telemetry file")));
                }
            });

            Telemetry::OpenTelemetry::Instance().Initialize();
        }

        static void uninit()
        {
            Telemetry::OpenTelemetry::Instance().Deinitialize();        
        }

    };
}
