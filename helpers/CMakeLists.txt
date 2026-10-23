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

cmake_minimum_required(VERSION 3.3)

# All .h files (including subdirectories) are installed as public headers so
# downstream repos can use:
#   find_package(WPEFrameworkHelpers REQUIRED)
#   target_link_libraries(myPlugin PRIVATE WPEFramework::WPEFrameworkHelpers)

set(PLUGIN_NAME Helpers)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} INTERFACE)

# Public include: the helpers/ root (and subdirs) — consumers get all headers
target_include_directories(${MODULE_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/${STORAGE_DIRECTORY}/helpers>)

# ---- Install shared library --------------------------------------------------
install(TARGETS ${MODULE_NAME}
    EXPORT  ${MODULE_NAME}Targets
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

# ---- Install all public headers preserving directory structure ---------------
# GLOB_RECURSE picks up every .h/.hpp/.cpp in helpers/ and its subdirectories.
# WSEndpoint.cpp is intentionally included here: it is a template implementation
# that must be installed as a header (consumers #include it via WSEndpoint.h).
# The compiled .cpp sources ( Module.cpp, WebSockets/JsonRpc/*.cpp)
# are excluded from the glob because they are already listed in HELPERS_SOURCES above.
file(GLOB_RECURSE HELPERS_PUBLIC_HEADERS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.h"
    "*.hpp"
)

# Install each header into the mirror subdirectory under the install prefix so
# that the full directory tree is preserved (e.g. WebSockets/JsonRpc/Request.h
# installs to include/<ns>/helpers/WebSockets/JsonRpc/Request.h).
foreach(HEADER_FILE ${HELPERS_PUBLIC_HEADERS})
    get_filename_component(HEADER_DIR "${HEADER_FILE}" DIRECTORY)
    install(
        FILES       "${HEADER_FILE}"
        DESTINATION "include/${STORAGE_DIRECTORY}/helpers/${HEADER_DIR}")
endforeach()

# ---- Export CMake targets for find_package() --------------------------------
install(EXPORT ${MODULE_NAME}Targets
    FILE        ${MODULE_NAME}Targets.cmake
    NAMESPACE   ${NAMESPACE}::
    DESTINATION lib/cmake/${MODULE_NAME})

# ---- Generate WPEFrameworkHelpersConfig.cmake --------------------------------
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}Config.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/${MODULE_NAME}Targets.cmake\")\n")

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}Config.cmake"
    DESTINATION "lib/cmake/${MODULE_NAME}")
