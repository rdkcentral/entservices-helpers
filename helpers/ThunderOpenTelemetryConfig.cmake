# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

string(TOLOWER ${NAMESPACE} NAMESPACE_LIB)

set(ENTSERVICES_OPENTELEMETRY_SAMPLER "ParentOn" CACHE STRING "Sampler for entservices open telemetry")
set(ENTSERVICES_OPENTELEMETRY_PROCESSOR "Batch" CACHE STRING "Processor for entservices open telemetry")
set(ENTSERVICES_OPENTELEMETRY_EXPORTER "Http" CACHE STRING "Exporter for entservices open telemetry")
set(ENTSERVICES_OPENTELEMETRY_TRACER_ENDPOINT "127.0.0.1:4318/v1/traces" CACHE STRING "Tracer endpoint for entservices open telemetry")
set(ENTSERVICES_OPENTELEMETRY_COMPONENT_NAME "Entservices plugins" CACHE STRING "Component name for entservices open telemetry")
set(ENTSERVICES_OPENTELEMETRY_COMPONENT_VERSION "1.0.0" CACHE STRING "Component version for entservices open telemetry")
set(ENTSERVICES_OPENTELEMETRY_LOGGER_ENDPOINT "127.0.0.1:4318/v1/logs" CACHE STRING "Tracer endpoint for entservices open telemetry")

find_package(ConfigGenerator REQUIRED)

write_config(
        SKIP_COMPARE
        SKIP_CLASSNAME
        SKIP_LOCATOR
        CUSTOM_PARAMS_WHITELIST "${CMAKE_CURRENT_LIST_DIR}/params.config"
        INSTALL_PATH "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${NAMESPACE}/"
        INSTALL_NAME "opentelemetry.json"
        PLUGINS "ThunderOpenTelemetry"
    )

