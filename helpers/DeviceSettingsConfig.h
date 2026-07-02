/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsFPD.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>

#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

using DeviceSettingsAudio = WPEFramework::Exchange::IDeviceSettingsAudio;
using DeviceSettingsFPD = WPEFramework::Exchange::IDeviceSettingsFPD;
using DeviceSettingsVideoDevice = WPEFramework::Exchange::IDeviceSettingsVideoDevice;
using DeviceSettingsVideoPort = WPEFramework::Exchange::IDeviceSettingsVideoPort;

using AudioPortType = DeviceSettingsAudio::AudioPortType;
using AudioTypeConfigInfo = DeviceSettingsAudio::AudioTypeConfigInfo;
using AudioPortConfigInfo = DeviceSettingsAudio::AudioPortConfigInfo;
using IAudioTypeConfigIterator = DeviceSettingsAudio::IAudioTypeConfigIterator;
using IAudioPortConfigIterator = DeviceSettingsAudio::IAudioPortConfigIterator;

using VideoPortType = DeviceSettingsVideoPort::VideoPort;
using VideoPortResolution = DeviceSettingsVideoPort::VideoPortResolution;
using VideoPortTypeConfig = DeviceSettingsVideoPort::VideoPortTypeConfig;
using VideoPortPortConfig = DeviceSettingsVideoPort::VideoPortPortConfig;
using IVideoPortTypeConfigIterator = DeviceSettingsVideoPort::IVideoPortTypeConfigIterator;
using IVideoPortPortConfigIterator = DeviceSettingsVideoPort::IVideoPortPortConfigIterator;
using IVideoPortResolutionIterator = DeviceSettingsVideoPort::IVideoPortResolutionIterator;

using VideoDeviceConfigInfo = DeviceSettingsVideoDevice::VideoDeviceConfigInfo;
using IVideoDeviceConfigIterator = DeviceSettingsVideoDevice::IVideoDeviceConfigIterator;

using FPDColorConfig = DeviceSettingsFPD::FPDColorConfig;
using FPDIndicatorConfig = DeviceSettingsFPD::FPDIndicatorConfig;
using FPDTextDisplayConfig = DeviceSettingsFPD::FPDTextDisplayConfig;
using FPDColorBinding = DeviceSettingsFPD::FPDColorBinding;
using IFPDTextDisplayConfigIterator = DeviceSettingsFPD::IFPDTextDisplayConfigIterator;
using IFPDIndicatorConfigIterator = DeviceSettingsFPD::IFPDIndicatorConfigIterator;
using IFPDColorConfigIterator = DeviceSettingsFPD::IFPDColorConfigIterator;
using IFPDColorBindingIterator = DeviceSettingsFPD::IFPDColorBindingIterator;

// ============================================================================
// Internal helpers (inline)
// ============================================================================

inline bool EqualsIgnoreCase(const std::string& lhs, const std::string& rhs)
{
    return (lhs.size() == rhs.size()) &&
           std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                      [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                      });
}

inline std::string BuildVideoPortName(const std::string& typeName, int32_t index)
{
    if (typeName.empty()) {
        return std::string("VIDEO") + std::to_string(index);
    }
    return typeName + std::to_string(index);
}

inline std::string BuildAudioPortName(AudioPortType portType, int32_t index)
{
    switch (portType) {
    case AudioPortType::AUDIO_PORT_TYPE_HDMI:      return std::string("HDMI")      + std::to_string(index);
    case AudioPortType::AUDIO_PORT_TYPE_SPDIF:     return std::string("SPDIF")     + std::to_string(index);
    case AudioPortType::AUDIO_PORT_TYPE_LR:        return std::string("LR")        + std::to_string(index);
    case AudioPortType::AUDIO_PORT_TYPE_SPEAKER:   return std::string("SPEAKER")   + std::to_string(index);
    case AudioPortType::AUDIO_PORT_TYPE_HDMIARC:   return std::string("HDMIARC")   + std::to_string(index);
    case AudioPortType::AUDIO_PORT_TYPE_HEADPHONE: return std::string("HEADPHONE") + std::to_string(index);
    default:                                        return std::string("AUDIO")     + std::to_string(index);
    }
}

// ============================================================================
// Common port-entry helpers
// ============================================================================

struct VideoPortEntry {
    std::string   name;
    std::string   typeName;
    VideoPortType type;
    int32_t       index;
};

struct AudioPortEntry {
    std::string   name;
    AudioPortType type;
    int32_t       index;
};

// ============================================================================
// VideoPortConfigStore
//   Populated by: LoadVideoPortConfig(Exchange::IDeviceSettingsVideoPort*, ...)
// ============================================================================

struct VideoPortConfigStore {
    std::vector<VideoPortTypeConfig> typeConfigs;
    std::vector<VideoPortPortConfig> portConfigs;
    std::vector<VideoPortResolution> resolutions;

    inline void Clear()
    {
        typeConfigs.clear();
        portConfigs.clear();
        resolutions.clear();
    }

    inline bool IsEmpty() const
    {
        return portConfigs.empty() && typeConfigs.empty();
    }

    inline bool BuildVideoPortEntries(std::vector<VideoPortEntry>& entries) const
    {
        entries.clear();
        for (size_t i = 0; i < portConfigs.size(); ++i) {
            const VideoPortPortConfig& pc = portConfigs[i];
            std::string typeName;
            for (size_t j = 0; j < typeConfigs.size(); ++j) {
                if (typeConfigs[j].typeId == pc.videoPortType) {
                    typeName = typeConfigs[j].name;
                    break;
                }
            }
            VideoPortEntry e;
            e.type     = pc.videoPortType;
            e.index    = pc.videoPortIndex;
            e.typeName = typeName;
            e.name     = BuildVideoPortName(typeName, pc.videoPortIndex);
            entries.push_back(e);
        }
        return !entries.empty();
    }

    inline std::string GetDefaultVideoPortName() const
    {
        std::vector<VideoPortEntry> entries;
        if (!BuildVideoPortEntries(entries)) {
            return std::string("HDMI0");
        }
        std::string defaultName = entries[0].name;
        bool found = false;
        for (size_t i = 0; i < entries.size() && !found; ++i) {
            if (entries[i].type == VideoPortType::DS_VIDEO_PORT_TYPE_HDMI && entries[i].index == 0) {
                defaultName = entries[i].name;
                found = true;
            }
        }
        for (size_t i = 0; i < entries.size() && !found; ++i) {
            if (entries[i].type == VideoPortType::DS_VIDEO_PORT_TYPE_INTERNAL && entries[i].index == 0) {
                defaultName = entries[i].name;
                found = true;
            }
        }
        return defaultName;
    }

    inline std::string GetDefaultResolution(const std::string& portName) const
    {
        for (size_t i = 0; i < portConfigs.size(); ++i) {
            const VideoPortPortConfig& pc = portConfigs[i];
            std::string typeName;
            for (size_t j = 0; j < typeConfigs.size(); ++j) {
                if (typeConfigs[j].typeId == pc.videoPortType) {
                    typeName = typeConfigs[j].name;
                    break;
                }
            }
            if (EqualsIgnoreCase(BuildVideoPortName(typeName, pc.videoPortIndex), portName)) {
                return pc.defaultResolution;
            }
        }
        return std::string();
    }

    inline bool GetConnectedAudioPort(const std::string& portName,
                                       int32_t& connectedAudioType,
                                       int32_t& connectedAudioIndex) const
    {
        for (size_t i = 0; i < portConfigs.size(); ++i) {
            const VideoPortPortConfig& pc = portConfigs[i];
            std::string typeName;
            for (size_t j = 0; j < typeConfigs.size(); ++j) {
                if (typeConfigs[j].typeId == pc.videoPortType) {
                    typeName = typeConfigs[j].name;
                    break;
                }
            }
            if (EqualsIgnoreCase(BuildVideoPortName(typeName, pc.videoPortIndex), portName)) {
                connectedAudioType  = pc.connectedAudioPortType;
                connectedAudioIndex = pc.connectedAudioPortIndex;
                return true;
            }
        }
        return false;
    }

    inline bool GetTypeConfig(VideoPortType typeId, VideoPortTypeConfig& cfg) const
    {
        for (size_t i = 0; i < typeConfigs.size(); ++i) {
            if (typeConfigs[i].typeId == typeId) {
                cfg = typeConfigs[i];
                return true;
            }
        }
        return false;
    }

    inline bool ResolveByName(const std::string& requestedPort,
                               VideoPortEntry& resolvedEntry) const
    {
        std::vector<VideoPortEntry> entries;
        if (!BuildVideoPortEntries(entries)) {
            return false;
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            const VideoPortEntry& e = entries[i];
            if (EqualsIgnoreCase(e.name, requestedPort) ||
                ((e.index == 0) && !e.typeName.empty() && EqualsIgnoreCase(e.typeName, requestedPort))) {
                resolvedEntry = e;
                return true;
            }
        }
        return false;
    }

    inline const std::vector<VideoPortResolution>& GetResolutions() const
    {
        return resolutions;
    }
};

// ============================================================================
// AudioConfigStore
//   Populated by: LoadAudioConfig(Exchange::IDeviceSettingsAudio*, ...)
// ============================================================================

struct AudioConfigStore {
    std::vector<AudioTypeConfigInfo> typeConfigs;
    std::vector<AudioPortConfigInfo> portConfigs;

    inline void Clear()
    {
        typeConfigs.clear();
        portConfigs.clear();
    }

    inline bool IsEmpty() const
    {
        return portConfigs.empty() && typeConfigs.empty();
    }

    inline bool BuildAudioPortEntries(std::vector<AudioPortEntry>& entries) const
    {
        entries.clear();
        for (size_t i = 0; i < portConfigs.size(); ++i) {
            const AudioPortConfigInfo& pc = portConfigs[i];
            AudioPortEntry e;
            e.type  = pc.audioPortType;
            e.index = pc.audioPortIndex;
            e.name  = BuildAudioPortName(pc.audioPortType, pc.audioPortIndex);
            entries.push_back(e);
        }
        return !entries.empty();
    }

    inline std::string GetDefaultAudioPortName() const
    {
        std::vector<AudioPortEntry> entries;
        if (!BuildAudioPortEntries(entries)) {
            return std::string("HDMI0");
        }
        std::string defaultName = entries[0].name;
        bool found = false;
        for (size_t i = 0; i < entries.size() && !found; ++i) {
            const std::string& n = entries[i].name;
            if (n.find("HDMI0") != std::string::npos || n.find("SPEAKER0") != std::string::npos) {
                defaultName = n;
                found = true;
            }
        }
        return defaultName;
    }

    inline bool GetTypeConfig(int32_t typeId, AudioTypeConfigInfo& cfg) const
    {
        for (size_t i = 0; i < typeConfigs.size(); ++i) {
            if (typeConfigs[i].typeId == typeId) {
                cfg = typeConfigs[i];
                return true;
            }
        }
        return false;
    }

    inline bool IsHDMIOutPortPresent() const
    {
        std::vector<AudioPortEntry> entries;
        if (!BuildAudioPortEntries(entries)) {
            return false;
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].name.find("HDMI0") != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// VideoDeviceConfigStore
//   Populated by: LoadVideoDeviceConfig(Exchange::IDeviceSettingsVideoDevice*, ...)
// ============================================================================

struct VideoDeviceConfigStore {
    std::vector<VideoDeviceConfigInfo> deviceConfigs;

    inline void Clear()
    {
        deviceConfigs.clear();
    }

    inline bool IsEmpty() const
    {
        return deviceConfigs.empty();
    }

    inline const std::vector<VideoDeviceConfigInfo>& GetAllConfigs() const
    {
        return deviceConfigs;
    }

    inline bool GetConfig(int32_t index, VideoDeviceConfigInfo& cfg) const
    {
        if (index < 0 || static_cast<size_t>(index) >= deviceConfigs.size()) {
            return false;
        }
        cfg = deviceConfigs[static_cast<size_t>(index)];
        return true;
    }

    inline size_t GetCount() const
    {
        return deviceConfigs.size();
    }
};

// ============================================================================
// FrontPanelConfigStore
//   Populated by: LoadFrontPanelConfig(Exchange::IDeviceSettingsFPD*, ...)
// ============================================================================

struct FrontPanelConfigStore {
    std::vector<FPDColorConfig>       colors;
    std::vector<FPDIndicatorConfig>   indicators;
    std::vector<FPDTextDisplayConfig> textDisplays;
    std::vector<FPDColorBinding>      colorBindings;

    inline void Clear()
    {
        colors.clear();
        indicators.clear();
        textDisplays.clear();
        colorBindings.clear();
    }

    inline bool IsEmpty() const
    {
        return indicators.empty() && textDisplays.empty() && colors.empty() && colorBindings.empty();
    }

    inline const std::vector<FPDIndicatorConfig>& GetIndicators() const
    {
        return indicators;
    }

    inline const std::vector<FPDColorConfig>& GetColors() const
    {
        return colors;
    }

    inline const std::vector<FPDTextDisplayConfig>& GetTextDisplays() const
    {
        return textDisplays;
    }

    inline const std::vector<FPDColorBinding>& GetColorBindings() const
    {
        return colorBindings;
    }

    inline bool GetIndicatorById(int32_t id, FPDIndicatorConfig& cfg) const
    {
        for (size_t i = 0; i < indicators.size(); ++i) {
            if (indicators[i].id == id) {
                cfg = indicators[i];
                return true;
            }
        }
        return false;
    }

    inline bool GetTextDisplayByName(const std::string& name, FPDTextDisplayConfig& cfg) const
    {
        for (size_t i = 0; i < textDisplays.size(); ++i) {
            if (EqualsIgnoreCase(textDisplays[i].name, name)) {
                cfg = textDisplays[i];
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Standalone load functions — one per component interface (inline)
// ============================================================================

inline bool LoadVideoPortConfig(Exchange::IDeviceSettingsVideoPort* iface, VideoPortConfigStore& store)
{
    store.Clear();

    if (iface == nullptr) {
        LOGERR("LoadVideoPortConfig: iface is null");
        return false;
    }

    IVideoPortTypeConfigIterator* typeIt = nullptr;
    IVideoPortPortConfigIterator* portIt = nullptr;

    const uint32_t result = iface->GetVideoPortConfig(typeIt, portIt);
    if (result != Core::ERROR_NONE) {
        LOGERR("LoadVideoPortConfig: GetVideoPortConfig failed: %u", result);
        if (typeIt) typeIt->Release();
        if (portIt) portIt->Release();
        return false;
    }

    if (typeIt != nullptr) {
        VideoPortTypeConfig cfg;
        while (typeIt->Next(cfg)) {
            store.typeConfigs.push_back(cfg);
        }
        typeIt->Release();
    }

    if (portIt != nullptr) {
        VideoPortPortConfig cfg;
        while (portIt->Next(cfg)) {
            store.portConfigs.push_back(cfg);
        }
        portIt->Release();
    }

    // Resolution configuration is now retrieved per video port type.
    for (size_t i = 0; i < store.typeConfigs.size(); ++i) {
        IVideoPortResolutionIterator* resIt = nullptr;
        const uint32_t resResult = iface->GetVideoPortResolutionConfig(store.typeConfigs[i].typeId, resIt);
        if (resResult != Core::ERROR_NONE) {
            LOGWARN("LoadVideoPortConfig: GetVideoPortResolutionConfig failed for type=%d: %u",
                static_cast<int>(store.typeConfigs[i].typeId), resResult);
            if (resIt) {
                resIt->Release();
            }
            continue;
        }

        if (resIt != nullptr) {
            VideoPortResolution res;
            while (resIt->Next(res)) {
                const auto it = std::find_if(
                    store.resolutions.begin(),
                    store.resolutions.end(),
                    [&res](const VideoPortResolution& existing) {
                        return EqualsIgnoreCase(existing.name, res.name);
                    });

                if (it == store.resolutions.end()) {
                    store.resolutions.push_back(res);
                }
            }
            resIt->Release();
        }
    }

    LOGINFO("LoadVideoPortConfig: types=%zu ports=%zu resolutions=%zu",
            store.typeConfigs.size(), store.portConfigs.size(), store.resolutions.size());
    return true;
}

inline bool LoadAudioConfig(Exchange::IDeviceSettingsAudio* iface, AudioConfigStore& store)
{
    store.Clear();

    if (iface == nullptr) {
        LOGERR("LoadAudioConfig: iface is null");
        return false;
    }

    IAudioTypeConfigIterator* typeIt = nullptr;
    IAudioPortConfigIterator* portIt = nullptr;

    const uint32_t result = iface->GetAudioConfig(typeIt, portIt);
    if (result != Core::ERROR_NONE) {
        LOGERR("LoadAudioConfig: GetAudioConfig failed: %u", result);
        if (typeIt) typeIt->Release();
        if (portIt) portIt->Release();
        return false;
    }

    if (typeIt != nullptr) {
        AudioTypeConfigInfo cfg;
        while (typeIt->Next(cfg)) {
            store.typeConfigs.push_back(cfg);
        }
        typeIt->Release();
    }

    if (portIt != nullptr) {
        AudioPortConfigInfo cfg;
        while (portIt->Next(cfg)) {
            store.portConfigs.push_back(cfg);
        }
        portIt->Release();
    }

    LOGINFO("LoadAudioConfig: types=%zu ports=%zu",
            store.typeConfigs.size(), store.portConfigs.size());
    return true;
}

inline bool LoadVideoDeviceConfig(Exchange::IDeviceSettingsVideoDevice* iface, VideoDeviceConfigStore& store)
{
    store.Clear();

    if (iface == nullptr) {
        LOGERR("LoadVideoDeviceConfig: iface is null");
        return false;
    }

    IVideoDeviceConfigIterator* it = nullptr;

    const uint32_t result = iface->GetVideoDeviceConfig(it);
    if (result != Core::ERROR_NONE) {
        LOGERR("LoadVideoDeviceConfig: GetVideoDeviceConfig failed: %u", result);
        if (it) it->Release();
        return false;
    }

    if (it != nullptr) {
        VideoDeviceConfigInfo cfg;
        while (it->Next(cfg)) {
            store.deviceConfigs.push_back(cfg);
        }
        it->Release();
    }

    LOGINFO("LoadVideoDeviceConfig: devices=%zu", store.deviceConfigs.size());
    return true;
}

inline bool LoadFrontPanelConfig(Exchange::IDeviceSettingsFPD* iface, FrontPanelConfigStore& store)
{
    store.Clear();

    if (iface == nullptr) {
        LOGERR("LoadFrontPanelConfig: iface is null");
        return false;
    }

    IFPDTextDisplayConfigIterator* textIt    = nullptr;
    IFPDIndicatorConfigIterator*   indicIt   = nullptr;
    IFPDColorConfigIterator*       colorIt   = nullptr;
    IFPDColorBindingIterator*      bindingIt = nullptr;

    const uint32_t result = iface->GetFrontPanelConfig(textIt, indicIt, colorIt, bindingIt);
    if (result != Core::ERROR_NONE) {
        LOGERR("LoadFrontPanelConfig: GetFrontPanelConfig failed: %u", result);
        if (textIt)    textIt->Release();
        if (indicIt)   indicIt->Release();
        if (colorIt)   colorIt->Release();
        if (bindingIt) bindingIt->Release();
        return false;
    }

    if (textIt != nullptr) {
        FPDTextDisplayConfig cfg;
        while (textIt->Next(cfg)) {
            store.textDisplays.push_back(cfg);
        }
        textIt->Release();
    }

    if (indicIt != nullptr) {
        FPDIndicatorConfig cfg;
        while (indicIt->Next(cfg)) {
            store.indicators.push_back(cfg);
        }
        indicIt->Release();
    }

    if (colorIt != nullptr) {
        FPDColorConfig cfg;
        while (colorIt->Next(cfg)) {
            store.colors.push_back(cfg);
        }
        colorIt->Release();
    }

    if (bindingIt != nullptr) {
        FPDColorBinding cfg;
        while (bindingIt->Next(cfg)) {
            store.colorBindings.push_back(cfg);
        }
        bindingIt->Release();
    }

    LOGINFO("LoadFrontPanelConfig: textDisplays=%zu indicators=%zu colors=%zu bindings=%zu",
            store.textDisplays.size(), store.indicators.size(),
            store.colors.size(), store.colorBindings.size());
    return true;
}

} // namespace Plugin
} // namespace WPEFramework
