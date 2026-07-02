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

/**
 * @file DeviceSettingsClientHelper.h
 *
 * @brief Base class for Thunder client plugins that connect to the
 *        DeviceSettings plugin via a single COM-RPC link on IDeviceSettings
 *        (ID_DEVICESETTINGS) and acquire sub-interfaces via QueryInterface.
 *
 * This header also provides the configuration store types and loader functions
 * (previously in DeviceSettingsConfig.h) so that a single include covers both
 * the COM-RPC connection management and the HAL config loading:
 *   - VideoPortConfigStore  / LoadVideoPortConfig()
 *   - AudioConfigStore      / LoadAudioConfig()
 *   - VideoDeviceConfigStore/ LoadVideoDeviceConfig()
 *   - FrontPanelConfigStore / LoadFrontPanelConfig()
 *
 * The DeviceSettingsClientHelper class additionally exposes single-argument
 * convenience wrappers (e.g. LoadVideoPortConfig(store)) that internally call
 * AcquireSubInterface<T>() so callers never need to manage raw interface pointers
 * for configuration loading.
 *
 * ## Architecture
 *
 * The COM-RPC link is always opened with the ROOT interface IDeviceSettings
 * (ID_DEVICESETTINGS).  Sub-interfaces (IDeviceSettingsVideoDevice,
 * IDeviceSettingsAudio, etc.) are obtained on demand by calling
 * AcquireSubInterface<T>(), which calls QueryInterface<T>() on the root.
 *
 * This means:
 *   - ONE connection to the DeviceSettings plugin regardless of how many
 *     sub-interfaces the client needs.
 *   - Sub-interfaces are AddRef'd on every AcquireSubInterface<T>() call and
 *     MUST be Release()'d by the caller when done.
 *
 * ## Activation lifecycle
 *
 *   1. client calls DeviceSettingsClientHelper::Open(service)  in its Configure(IShell*)
 *   2. Thunder connects asynchronously to "org.rdk.DeviceSettings" (default callsign)
 *   3. DeviceSettings activates → Operational(true) →
 *        OnDeviceSettingsActivated()  ← override to (re-)register events
 *   4. DeviceSettings deactivates (crash/restart) → Operational(false) →
 *        OnDeviceSettingsDeactivated() ← override to invalidate cached state
 *   5. DeviceSettings restarts → Operational(true) again →
 *        OnDeviceSettingsActivated() re-called automatically
 *
 * ## Config loading — convenience member wrappers
 *
 * Inside OnDeviceSettingsActivated() (or anywhere after Open()), call the
 * single-argument form to load config without managing raw interface pointers:
 *
 * @code
 *   VideoPortConfigStore vpStore;
 *   AudioConfigStore     audioStore;
 *
 *   void OnDeviceSettingsActivated() override {
 *       LoadVideoPortConfig(vpStore);   // acquires IDeviceSettingsVideoPort internally
 *       LoadAudioConfig(audioStore);    // acquires IDeviceSettingsAudio internally
 *   }
 * @endcode
 *
 * The two-argument free-function overloads are also available for callers that
 * already hold a raw sub-interface pointer:
 *
 * @code
 *   auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
 *   if (vp) {
 *       LoadVideoPortConfig(vp, vpStore);
 *       vp->Release();
 *   }
 * @endcode
 *
 * ## Usage Example
 *
 * @code
 * class FrameRateImplementation
 *     : public Exchange::IFrameRate
 *     , public Exchange::IConfiguration
 *     , public DeviceSettingsClientHelper   // root IDeviceSettings COM-RPC link
 *     // NOTE: do NOT inherit INotification directly — use an inner delegate class
 * {
 *     // Inner notification delegate — name conveys the DS sub-interface it handles
 *     class DSVideoDeviceNotification : public Exchange::IDeviceSettingsVideoDevice::INotification {
 *     public:
 *         explicit DSVideoDeviceNotification(FrameRateImplementation& p) : _parent(p) {}
 *         void OnDisplayFrameratePreChange(const string& fr) override {
 *             _parent.OnDisplayFrameratePreChange(fr);
 *         }
 *         void OnDisplayFrameratePostChange(const string& fr) override {
 *             _parent.OnDisplayFrameratePostChange(fr);
 *         }
 *         BEGIN_INTERFACE_MAP(DSVideoDeviceNotification)
 *             INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoDevice::INotification)
 *         END_INTERFACE_MAP
 *     private:
 *         FrameRateImplementation& _parent;
 *     };
 *
 *     int32_t                               _videoDeviceHandle { -1 };
 *     Core::Sink<DSVideoDeviceNotification> _notification;  // initialized with *this in constructor
 *     VideoDeviceConfigStore                _vdStore;
 *
 *     // Constructor — initialize _notification with *this in the member initializer list
 *     FrameRateImplementation()
 *         : ...
 *         , _notification(*this)   // mirrors USBMassStorage _USB_DeviceNotification(*this) pattern
 *     {}
 *
 *     uint32_t Configure(PluginHost::IShell* service) override {
 *         DeviceSettingsClientHelper::Open(service);
 *         return Core::ERROR_NONE;
 *     }
 *
 *     ~FrameRateImplementation() {
 *         auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
 *         if (vd) { vd->Unregister(&_notification); vd->Release(); }
 *         DeviceSettingsClientHelper::Close();
 *     }
 *
 *     // Called when DeviceSettings (re-)activates
 *     void OnDeviceSettingsActivated() override {
 *         LoadVideoDeviceConfig(_vdStore);   // convenience wrapper — no raw pointer needed
 *         auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
 *         if (vd) {
 *             vd->GetVideoDeviceHandle(0, _videoDeviceHandle);
 *             vd->Register(&_notification);  // delegate, not 'this'
 *             vd->Release();
 *         }
 *     }
 *
 *     void OnDeviceSettingsDeactivated() override {
 *         _videoDeviceHandle = -1;
 *         _vdStore.Clear();
 *     }
 *
 *     // IDeviceSettingsVideoDevice::INotification
 *     void OnDisplayFrameratePreChange(const string& fr) { ... }
 *     void OnDisplayFrameratePostChange(const string& fr) { ... }
 *
 *     Core::hresult GetDisplayFrameRate(string& framerate, bool& success) override {
 *         auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
 *         if (!vd) return Core::ERROR_UNAVAILABLE;
 *         Core::hresult rc = vd->GetCurrentDisplayFrameRate(_videoDeviceHandle, framerate);
 *         vd->Release();
 *         success = (rc == Core::ERROR_NONE);
 *         return rc;
 *     }
 * };
 * @endcode
 */

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

#include <interfaces/IDeviceSettings.h>
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsDisplay.h>
#include <interfaces/IDeviceSettingsFPD.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>
#include <plugins/plugins.h>
#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

// ============================================================================
// Type aliases — convenience shorthands for DS sub-interface types
// ============================================================================

using DeviceSettingsAudio       = WPEFramework::Exchange::IDeviceSettingsAudio;
using DeviceSettingsDisplay     = WPEFramework::Exchange::IDeviceSettingsDisplay;
using DeviceSettingsFPD         = WPEFramework::Exchange::IDeviceSettingsFPD;
using DeviceSettingsVideoDevice = WPEFramework::Exchange::IDeviceSettingsVideoDevice;
using DeviceSettingsVideoPort   = WPEFramework::Exchange::IDeviceSettingsVideoPort;

using AudioPortType             = DeviceSettingsAudio::AudioPortType;
using AudioTypeConfigInfo       = DeviceSettingsAudio::AudioTypeConfigInfo;
using AudioPortConfigInfo       = DeviceSettingsAudio::AudioPortConfigInfo;
using IAudioTypeConfigIterator  = DeviceSettingsAudio::IAudioTypeConfigIterator;
using IAudioPortConfigIterator  = DeviceSettingsAudio::IAudioPortConfigIterator;
// Audio: operational enum types used in COM-RPC client code
using StereoMode                = DeviceSettingsAudio::StereoMode;
using AudioCapabilities         = DeviceSettingsAudio::AudioCapabilities;
using AudioFormat               = DeviceSettingsAudio::AudioFormat;
using DuckingType               = DeviceSettingsAudio::DuckingType;
using DuckingAction             = DeviceSettingsAudio::DuckingAction;

using VideoPortType                  = DeviceSettingsVideoPort::VideoPort;
using VideoPortResolution            = DeviceSettingsVideoPort::VideoPortResolution;
using VideoPortTypeConfig            = DeviceSettingsVideoPort::VideoPortTypeConfig;
using VideoPortPortConfig            = DeviceSettingsVideoPort::VideoPortPortConfig;
using IVideoPortTypeConfigIterator   = DeviceSettingsVideoPort::IVideoPortTypeConfigIterator;
using IVideoPortPortConfigIterator   = DeviceSettingsVideoPort::IVideoPortPortConfigIterator;
using IVideoPortResolutionIterator   = DeviceSettingsVideoPort::IVideoPortResolutionIterator;
// VideoPort: operational enum types used in COM-RPC client code
using TVResolution              = DeviceSettingsVideoPort::TVResolution;
using HDRStandard               = DeviceSettingsVideoPort::HDRStandard;
using DisplayColorDepth         = DeviceSettingsVideoPort::DisplayColorDepth;

using VideoDeviceConfigInfo          = DeviceSettingsVideoDevice::VideoDeviceConfigInfo;
using IVideoDeviceConfigIterator     = DeviceSettingsVideoDevice::IVideoDeviceConfigIterator;
// VideoDevice: operational enum types used in COM-RPC client code
using VideoZoom                 = DeviceSettingsVideoDevice::VideoZoom;
using VideoCodingFormat         = DeviceSettingsVideoDevice::VideoCodingFormat;

// Display: operational enum types used in COM-RPC client code
using AVIContentType            = DeviceSettingsDisplay::AVIContentType;
using AVIScanInformation        = DeviceSettingsDisplay::AVIScanInformation;

using FPDColorConfig                 = DeviceSettingsFPD::FPDColorConfig;
using FPDIndicatorConfig             = DeviceSettingsFPD::FPDIndicatorConfig;
using FPDTextDisplayConfig           = DeviceSettingsFPD::FPDTextDisplayConfig;
using FPDColorBinding                = DeviceSettingsFPD::FPDColorBinding;
using IFPDTextDisplayConfigIterator  = DeviceSettingsFPD::IFPDTextDisplayConfigIterator;
using IFPDIndicatorConfigIterator    = DeviceSettingsFPD::IFPDIndicatorConfigIterator;
using IFPDColorConfigIterator        = DeviceSettingsFPD::IFPDColorConfigIterator;
using IFPDColorBindingIterator       = DeviceSettingsFPD::IFPDColorBindingIterator;

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
    std::vector<VideoPortResolution> resolutions;             ///< Deduplicated union across all port types
    std::map<VideoPortType, std::vector<VideoPortResolution>> resolutionsByType; ///< Per-type; keyed by VideoPort enum (from GetVideoPortResolutionConfig)

    inline void Clear()
    {
        typeConfigs.clear();
        portConfigs.clear();
        resolutions.clear();
        resolutionsByType.clear();
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

    inline std::vector<VideoPortResolution> GetResolutions() const
    {
        return resolutions;
    }

    /**
     * @brief Get the supported resolutions for a specific VideoPort type.
     *
     * Returns the resolution list loaded from GetVideoPortResolutionConfig()
     * for the requested @p typeId.  Returns false (and leaves @p out unchanged)
     * if no resolutions were loaded for that type.
     */
    inline bool GetResolutionsForType(VideoPortType typeId,
                                       std::vector<VideoPortResolution>& out) const
    {
        const auto it = resolutionsByType.find(typeId);
        if (it == resolutionsByType.end() || it->second.empty()) {
            return false;
        }
        out = it->second;
        return true;
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

    inline std::vector<VideoDeviceConfigInfo> GetAllConfigs() const
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

    inline std::vector<FPDIndicatorConfig> GetIndicators() const
    {
        return indicators;
    }

    inline std::vector<FPDColorConfig> GetColors() const
    {
        return colors;
    }

    inline std::vector<FPDTextDisplayConfig> GetTextDisplays() const
    {
        return textDisplays;
    }

    inline std::vector<FPDColorBinding> GetColorBindings() const
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
// These accept a raw sub-interface pointer for callers that already hold one.
// Prefer the DeviceSettingsClientHelper member wrappers below when possible.
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

    // Resolution configuration is retrieved per video port type via GetVideoPortResolutionConfig().
    // Results are stored in both resolutionsByType (per-type) and resolutions (deduplicated union).
    for (size_t i = 0; i < store.typeConfigs.size(); ++i) {
        const VideoPortType portType = store.typeConfigs[i].typeId;
        IVideoPortResolutionIterator* resIt = nullptr;
        const uint32_t resResult = iface->GetVideoPortResolutionConfig(portType, resIt);
        if (resResult != Core::ERROR_NONE) {
            LOGWARN("LoadVideoPortConfig: GetVideoPortResolutionConfig failed for type=%d: %u",
                static_cast<int>(portType), resResult);
            if (resIt) {
                resIt->Release();
            }
            continue;
        }

        if (resIt != nullptr) {
            std::vector<VideoPortResolution>& typeResolutions = store.resolutionsByType[portType];
            VideoPortResolution res;
            while (resIt->Next(res)) {
                typeResolutions.push_back(res);

                // Add to flat deduplicated union
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
            LOGINFO("LoadVideoPortConfig: type=%d resolutions=%zu",
                    static_cast<int>(portType), typeResolutions.size());
        }
    }

    LOGINFO("LoadVideoPortConfig: types=%zu ports=%zu resolutions=%zu",
            store.typeConfigs.size(), store.portConfigs.size(), store.resolutions.size());
    return true;
}

/**
 * @brief Load the supported resolutions for a single VideoPort type.
 *
 * Thin wrapper around GetVideoPortResolutionConfig() for callers that need
 * resolutions for one specific port type without loading the full config.
 *
 * @param iface     Raw IDeviceSettingsVideoPort pointer (caller owns, must not be null).
 * @param portType  The VideoPort enum value to query (e.g. DS_VIDEO_PORT_TYPE_HDMI).
 * @param out       Filled with the supported VideoPortResolution entries on success.
 * @return true on success, false on failure or empty iterator.
 */
inline bool LoadVideoPortResolutionConfig(Exchange::IDeviceSettingsVideoPort* iface,
                                           VideoPortType portType,
                                           std::vector<VideoPortResolution>& out)
{
    out.clear();

    if (iface == nullptr) {
        LOGERR("LoadVideoPortResolutionConfig: iface is null");
        return false;
    }

    IVideoPortResolutionIterator* resIt = nullptr;
    const uint32_t result = iface->GetVideoPortResolutionConfig(portType, resIt);
    if (result != Core::ERROR_NONE) {
        LOGERR("LoadVideoPortResolutionConfig: GetVideoPortResolutionConfig failed for type=%d: %u",
               static_cast<int>(portType), result);
        if (resIt) resIt->Release();
        return false;
    }

    if (resIt != nullptr) {
        VideoPortResolution res;
        while (resIt->Next(res)) {
            out.push_back(res);
        }
        resIt->Release();
    }

    LOGINFO("LoadVideoPortResolutionConfig: type=%d resolutions=%zu",
            static_cast<int>(portType), out.size());
    return !out.empty();
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

// ============================================================================
// DeviceSettingsClientHelper
// ============================================================================

/**
 * @brief Base class for Thunder plugins that consume DeviceSettings COM-RPC
 *        sub-interfaces.
 *
 * Always opens a COM-RPC link using the ROOT IDeviceSettings interface
 * (ID_DEVICESETTINGS).  Sub-interfaces are obtained via AcquireSubInterface<T>().
 */
class DeviceSettingsClientHelper
    : public RPC::PluginSmartInterfaceType<Exchange::IDeviceSettings>
{
    using BaseClass = RPC::PluginSmartInterfaceType<Exchange::IDeviceSettings>;

public:
    // Default DeviceSettings Thunder plugin callsign
    static constexpr const char* kDefaultCallsign = "org.rdk.DeviceSettings";

public:
    DeviceSettingsClientHelper()
        : _service(nullptr)
        , _callsign(kDefaultCallsign)
    {
    }

    virtual ~DeviceSettingsClientHelper() { Close(); }
    DeviceSettingsClientHelper(const DeviceSettingsClientHelper&)            = delete;
    DeviceSettingsClientHelper& operator=(const DeviceSettingsClientHelper&) = delete;
    DeviceSettingsClientHelper(DeviceSettingsClientHelper&&)                 = delete;
    DeviceSettingsClientHelper& operator=(DeviceSettingsClientHelper&&)      = delete;

public:
    /**
     * @brief Open a COM-RPC link to the DeviceSettings plugin.
     *
     * Call this from your Configure(PluginHost::IShell*) implementation.
     * Holds an AddRef on @p service — balanced by Close().
     *
     * If DeviceSettings is already running, Operational(true) fires
     * synchronously inside Open() so OnDeviceSettingsActivated() may be
     * called before Open() returns.
     *
     * @param service   IShell* provided to Configure()
     * @param callsign  Plugin callsign (default: "org.rdk.DeviceSettings")
     */
    uint32_t Open(PluginHost::IShell* service, const string& callsign = kDefaultCallsign)
    {
        if (service == nullptr) {
            LOGERR("DeviceSettingsClientHelper::Open() failed: service is nullptr");
            return Core::ERROR_BAD_REQUEST;
        }
        if (_service != nullptr) {
            LOGERR("DeviceSettingsClientHelper::Open(%s) called while already open", callsign.c_str());
            return Core::ERROR_GENERAL;
        }
        _service  = service;
        _service->AddRef();
        _callsign = callsign;

        const uint32_t result = BaseClass::Open(_service, callsign);
        if (result != Core::ERROR_NONE) {
            LOGERR("DeviceSettingsClientHelper::Open(%s) failed: %u",
                   callsign.c_str(), result);
            _service->Release();
            _service = nullptr;
            _callsign = kDefaultCallsign;
        } else {
            LOGINFO("DeviceSettingsClientHelper::Open(%s) succeeded", callsign.c_str());
        }
        return result;
    }

    /**
     * @brief Close the COM-RPC link and release the IShell reference.
     *
     * Call from your destructor BEFORE releasing any other held resources.
     * Safe to call even if Open() was never called.
     */
    void Close()
    {
        if (_service != nullptr) {
            LOGINFO("DeviceSettingsClientHelper::Close(%s)", _callsign.c_str());
            BaseClass::Close();
            _service->Release();
            _service = nullptr;
        }
    }

    /**
     * @brief Acquire an AddRef'd pointer to a DeviceSettings sub-interface.
     *
     * Calls QueryInterface<SUBINTERFACE>() on the root IDeviceSettings
     * COM-RPC proxy.  Returns nullptr if DeviceSettings is not currently
     * active or if the implementation does not expose that interface.
     *
     * The caller MUST call Release() on the returned pointer exactly once.
     *
     * @tparam SUBINTERFACE  One of Exchange::IDeviceSettingsXxx
     *                       (e.g. Exchange::IDeviceSettingsVideoDevice)
     *
     * @code
     *   auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
     *   if (vd != nullptr) {
     *       vd->SetDisplayFrameRate(handle, framerate);
     *       vd->Release();
     *   }
     * @endcode
     */
    template <typename SUBINTERFACE>
    SUBINTERFACE* AcquireSubInterface()
    {
        Exchange::IDeviceSettings* root = BaseClass::Interface();
        if (root == nullptr) {
            LOGERR("DeviceSettingsClientHelper[%s]: IDeviceSettings root not available",
                   _callsign.c_str());
            return nullptr;
        }
        SUBINTERFACE* sub = root->QueryInterface<SUBINTERFACE>();
        root->Release();   // root reference balanced — sub has its own AddRef from QI
        if (sub == nullptr) {
            LOGERR("DeviceSettingsClientHelper[%s]: QueryInterface<0x%08x> returned nullptr",
                   _callsign.c_str(), static_cast<uint32_t>(SUBINTERFACE::ID));
        }
        return sub;
    }

    /**
     * @brief Convenience: acquire IDeviceSettingsVideoPort, load config into
     *        @p store, then release the sub-interface.
     *
     * Equivalent to:
     * @code
     *   auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
     *   if (vp) { LoadVideoPortConfig(vp, store); vp->Release(); }
     * @endcode
     */
    bool LoadVideoPortConfig(VideoPortConfigStore& store)
    {
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadVideoPortConfig(iface, store);
        iface->Release();
        return ok;
    }

    /**
     * @brief Convenience: acquire IDeviceSettingsVideoPort and load the
     *        supported resolutions for a single @p portType into @p out.
     *
     * Useful for re-querying resolutions for one port type after the full
     * config has already been loaded, e.g. after a display hotplug event.
     */
    bool LoadVideoPortResolutionConfig(VideoPortType portType,
                                        std::vector<VideoPortResolution>& out)
    {
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadVideoPortResolutionConfig(iface, portType, out);
        iface->Release();
        return ok;
    }

    /**
     * @brief Convenience: acquire IDeviceSettingsAudio, load config into
     *        @p store, then release the sub-interface.
     */
    bool LoadAudioConfig(AudioConfigStore& store)
    {
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadAudioConfig(iface, store);
        iface->Release();
        return ok;
    }

    /**
     * @brief Convenience: acquire IDeviceSettingsVideoDevice, load config into
     *        @p store, then release the sub-interface.
     */
    bool LoadVideoDeviceConfig(VideoDeviceConfigStore& store)
    {
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadVideoDeviceConfig(iface, store);
        iface->Release();
        return ok;
    }

    /**
     * @brief Convenience: acquire IDeviceSettingsFPD, load config into
     *        @p store, then release the sub-interface.
     */
    bool LoadFrontPanelConfig(FrontPanelConfigStore& store)
    {
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsFPD>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadFrontPanelConfig(iface, store);
        iface->Release();
        return ok;
    }

protected:
    /**
     * @brief Called when the DeviceSettings plugin activates (or re-activates
     *        after a crash/restart).
     *
     * Override to re-register INotification callbacks on sub-interfaces.
     * This is the ONLY place where event subscription should happen so that
     * subscriptions are automatically restored after a DeviceSettings restart.
     *
     * Pattern:
     * @code
     *   void OnDeviceSettingsActivated() override {
     *       auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
     *       if (vd) {
     *           vd->GetVideoDeviceHandle(0, _handle);
     *           vd->Register(&_notification);
     *           vd->Release();
     *       }
     *   }
     * @endcode
     */
    virtual void OnDeviceSettingsActivated() = 0;

    /**
     * @brief Called when the DeviceSettings plugin deactivates.
     *
     * The COM-RPC connection is already severed — do NOT call
     * AcquireSubInterface() or any interface methods here.
     *
     * Use this to invalidate cached handles and reset state.
     */
    virtual void OnDeviceSettingsDeactivated() = 0;

private:
    /**
     * @brief Sealed Thunder callback.
     * Derived classes MUST NOT override Operational() — override the named
     * methods above instead.
     */
    void Operational(const bool upAndRunning) override final
    {
        if (upAndRunning) {
            LOGINFO("DeviceSettingsClientHelper[%s]: DeviceSettings activated — "
                    "re-registering event notifications", _callsign.c_str());
            OnDeviceSettingsActivated();
        } else {
            LOGINFO("DeviceSettingsClientHelper[%s]: DeviceSettings deactivated — "
                    "cleaning up stale state", _callsign.c_str());
            OnDeviceSettingsDeactivated();
        }
    }

private:
    PluginHost::IShell* _service;
    string              _callsign;
};

} // namespace Plugin
} // namespace WPEFramework
