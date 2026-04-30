# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

# entservices-helpers: header-only utility library
# The helpers/ directory contains only .h files and must be built as an
# INTERFACE (header-only) CMake target — there are no .cpp sources to compile.

set(PLUGIN_NAME Helpers)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)

add_library(${MODULE_NAME} INTERFACE)

target_include_directories(${MODULE_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/${STORAGE_DIRECTORY}/helpers>)

install(
    FILES
        PluginInterfaceBuilder.h
        UtilsJsonRpc.h
        UtilsLogging.h
    DESTINATION
        include/${STORAGE_DIRECTORY}/helpers)

install(TARGETS ${MODULE_NAME}
    EXPORT ${MODULE_NAME}Targets)
