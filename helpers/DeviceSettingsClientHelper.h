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
#include <interfaces/IDeviceSettingsHost.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsHDMIIn.h>
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
using DeviceSettingsHost        = WPEFramework::Exchange::IDeviceSettingsHost;
using DeviceSettingsVideoDevice = WPEFramework::Exchange::IDeviceSettingsVideoDevice;
using DeviceSettingsVideoPort   = WPEFramework::Exchange::IDeviceSettingsVideoPort;

using AudioPortType             = DeviceSettingsAudio::AudioPortType;
using AudioTypeConfigInfo       = DeviceSettingsAudio::AudioTypeConfigInfo;
using AudioPortConfigInfo       = DeviceSettingsAudio::AudioPortConfigInfo;
using IAudioTypeConfigIterator  = DeviceSettingsAudio::IAudioTypeConfigIterator;
using IAudioPortConfigIterator  = DeviceSettingsAudio::IAudioPortConfigIterator;
// Audio: operational enum types used in COM-RPC client code
using AudioDuckingType          = DeviceSettingsAudio::AudioDuckingType;
using AudioDuckingAction        = DeviceSettingsAudio::AudioDuckingAction;

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
using VideoPortSurroundMode     = DeviceSettingsVideoPort::VideoPortSurroundMode;

using VideoDeviceConfigInfo          = DeviceSettingsVideoDevice::VideoDeviceConfigInfo;
using IVideoDeviceConfigIterator     = DeviceSettingsVideoDevice::IVideoDeviceConfigIterator;
// VideoDevice: operational enum types used in COM-RPC client code
using VideoZoom                 = DeviceSettingsVideoDevice::VideoZoom;
using VideoCodec                = DeviceSettingsVideoDevice::VideoCodec;

// Display: operational enum types used in COM-RPC client code
using DisplayAVIContentType     = DeviceSettingsDisplay::DisplayAVIContentType;
using DisplayAVIScanInformation = DeviceSettingsDisplay::DisplayAVIScanInformation;

using FPDColorConfig                 = DeviceSettingsFPD::FPDColorConfig;
using FPDIndicatorConfig             = DeviceSettingsFPD::FPDIndicatorConfig;
using FPDTextDisplayConfig           = DeviceSettingsFPD::FPDTextDisplayConfig;
using FPDColorBinding                = DeviceSettingsFPD::FPDColorBinding;
using IFPDTextDisplayConfigIterator  = DeviceSettingsFPD::IFPDTextDisplayConfigIterator;
using IFPDIndicatorConfigIterator    = DeviceSettingsFPD::IFPDIndicatorConfigIterator;
using IFPDColorConfigIterator        = DeviceSettingsFPD::IFPDColorConfigIterator;
using IFPDColorBindingIterator       = DeviceSettingsFPD::IFPDColorBindingIterator;

#define INVALID_DS_HANDLE -1

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

/**
 * @brief Returns the DS_IARM-compatible name string for a StereoMode enum value.
 *
 * Mirrors AudioStereoMode::getName() from libds (audioStereoMode.cpp).
 * Used when converting AudioTypeConfigInfo::supportedStereoModeMask bits to
 * mode name strings — each port TYPE has its own mask, and each bit position
 * corresponds to the integer value of the StereoMode enum.
 *
 * @return Pointer to a static string literal, or nullptr for unknown modes.
 */
inline const char* StereoModeToName(Exchange::IDeviceSettingsAudio::StereoMode mode)
{
    switch (mode) {
    case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_MONO:        return "MONO";
    case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO:      return "STEREO";
    case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_SURROUND:    return "SURROUND";
    case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_PASSTHROUGH: return "PASSTHRU";
    case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_DD:          return "DOLBYDIGITAL";
    case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_DDPLUS:      return "DOLBYDIGITALPLUS";
    default:                                                                    return nullptr;
    }
}

/**
 * @brief Returns the DS_IARM-compatible mode name string for a StereoMode,
 *        falling back to "STEREO" for unknown/unmapped values.
 *
 * Mirrors AudioStereoMode::toString() from libds, used when building a
 * getSoundMode response string.  Unlike StereoModeToName(), this always
 * returns a valid non-null string.
 */
inline const char* StereoModeToString(Exchange::IDeviceSettingsAudio::StereoMode mode)
{
    const char* name = StereoModeToName(mode);
    return (name != nullptr) ? name : "STEREO";
}

/**
 * @brief Returns the ordered list of all StereoMode values recognised by the
 *        DeviceSettings COM-RPC stack.
 *
 * Each value in AudioTypeConfigInfo::supportedStereoModeMask is a bitmask where
 * bit N set means the StereoMode whose integer value is N is supported by that
 * port type.  Iterate this list and check each bit rather than iterating all 32
 * bits of the mask.
 *
 * Combined with StereoModeToName() this replaces the libds
 * AudioOutputPortType::getSupportedStereoModes() / AudioStereoMode::getName()
 * pattern used in DS_IARM.
 */
inline const std::vector<Exchange::IDeviceSettingsAudio::StereoMode>& KnownStereoModes()
{
    static const std::vector<Exchange::IDeviceSettingsAudio::StereoMode> kModes = {
        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_MONO,
        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO,
        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_SURROUND,
        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_PASSTHROUGH,
        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_DD,
        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_DDPLUS,
    };
    return kModes;
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
            LOGERR("No video port entries available to resolve '%s'", requestedPort.c_str());
            return false;
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            const VideoPortEntry& e = entries[i];
            if (EqualsIgnoreCase(e.name, requestedPort) ||
                ((e.index == 0) && !e.typeName.empty() && EqualsIgnoreCase(e.typeName, requestedPort))) {
                resolvedEntry = e;
                LOGINFO("Resolved video port '%s' to type=%d index=%d", requestedPort.c_str(), e.type, e.index);
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

    inline bool getAudioPortEntries(std::vector<AudioPortEntry>& entries) const
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
        if (!getAudioPortEntries(entries)) {
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
        if (!getAudioPortEntries(entries)) {
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
        _videoPortHandles.clear();
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadVideoPortConfig(iface, store);
        if (ok) {
            std::vector<VideoPortEntry> entries;
            if (store.BuildVideoPortEntries(entries)) {
                for (const VideoPortEntry& e : entries) {
                    int32_t handle = INVALID_DS_HANDLE;
                    Core::hresult rc = iface->GetVideoPort(e.type, e.index, handle);
                    if (rc == Core::ERROR_NONE) {
                        _videoPortHandles[e.name] = handle;
                        LOGINFO("LoadVideoPortConfig: VideoPort '%s' -> handle=%d", e.name.c_str(), handle);
                    } else {
                        LOGERR("LoadVideoPortConfig: failed to acquire VideoPort '%s' handle, Error=%d",
                               e.name.c_str(), rc);
                    }
                }
            }
            else {
                LOGWARN("LoadVideoPortConfig: no video port entries found");
            }
        }
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
        _audioPortHandles.clear();
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadAudioConfig(iface, store);
        if (ok) {
            std::vector<AudioPortEntry> entries;
            if (store.getAudioPortEntries(entries)) {
                for (const AudioPortEntry& e : entries) {
                    int32_t handle = INVALID_DS_HANDLE;
                    Core::hresult rc = iface->GetAudioPort(e.type, e.index, handle);
                    if (rc == Core::ERROR_NONE) {
                        _audioPortHandles[e.name] = handle;
                        LOGINFO("LoadAudioConfig: AudioPort '%s' -> handle=%d", e.name.c_str(), handle);
                    } else {
                        LOGERR("LoadAudioConfig: failed to acquire AudioPort '%s' handle, Error=%d",
                               e.name.c_str(), rc);
                    }
                }
            }
            else {
                LOGWARN("LoadAudioConfig: no audio port entries found");
            }
        }
        iface->Release();
        return ok;
    }

    /**
     * @brief Convenience: acquire IDeviceSettingsVideoDevice, load config into
     *        @p store, then release the sub-interface.
     */
    bool LoadVideoDeviceConfig(VideoDeviceConfigStore& store)
    {
        _videoDeviceHandles.clear();
        auto* iface = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
        if (!iface) return false;
        const bool ok = ::WPEFramework::Plugin::LoadVideoDeviceConfig(iface, store);
        if (ok) {
            const size_t count = store.GetCount();
            _videoDeviceHandles.resize(count, INVALID_DS_HANDLE);
            for (size_t i = 0; i < count; ++i) {
                int32_t handle = INVALID_DS_HANDLE;
                Core::hresult rc = iface->GetVideoDeviceHandle(static_cast<int32_t>(i), handle);
                if (rc == Core::ERROR_NONE) {
                    _videoDeviceHandles[i] = handle;
                    LOGINFO("LoadVideoDeviceConfig: device[%zu] handle=%d", i, handle);
                } else {
                    LOGERR("LoadVideoDeviceConfig: GetVideoDeviceHandle(%zu) failed, Error=%d",
                           i, static_cast<int>(rc));
                }
            }
        }
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

    /**
     * @brief Look up an audio port handle by name.
     *
     * Use when you only need the port handle without a connectivity check.
     * When you also need to check connectivity, use isAudioOutputPortConnected()
     * instead — it fills the handle AND performs the check in one call.
     *
     * @param audioPortName  Port name (e.g. "HDMI0", "SPDIF0", "HDMI_ARC0")
     * @return The cached port handle on success, or -1 if not found.
     */
    int32_t getCachedAudioPortHandle(const std::string& audioPortName) const
    {
        const auto it = _audioPortHandles.find(audioPortName);
        if (it != _audioPortHandles.end()) {
            return it->second;
        }
        LOGERR("getCachedAudioPortHandle: audioPort '%s' not found in handles map",
               audioPortName.c_str());
        return INVALID_DS_HANDLE;
    }

    /**
     * @brief Look up a video port handle by name.
     *
     * @param videoPortName  Port name (e.g. "HDMI0", "COMPONENT0")
     * @return The cached port handle on success, or -1 if not found.
     */
    int32_t getCachedVideoPortHandle(const std::string& videoPortName) const
    {
        const auto it = _videoPortHandles.find(videoPortName);
        if (it != _videoPortHandles.end()) {
            return it->second;
        }
        LOGERR("getCachedVideoPortHandle: videoPort '%s' not found in handles map",
               videoPortName.c_str());
        return INVALID_DS_HANDLE;
    }

    /**
     * @brief Look up a display handle by port name.
     *
     * @param portName  Port name (e.g. "HDMI0")
     * @return The cached display handle on success, or -1 if not found.
     */
    int32_t getCachedDisplayHandle(const std::string& portName) const
    {
        const auto it = _displayHandles.find(portName);
        if (it != _displayHandles.end()) {
            return it->second;
        }
        LOGERR("getCachedDisplayHandle: displayPort '%s' not found in handles map",
               portName.c_str());
        return INVALID_DS_HANDLE;
    }

    /**
     * @brief Returns the cached video device handle for the given device index.
     *
     * Populated by LoadVideoDeviceConfig(). Use index 0 for the primary device.
     *
     * @param index  Zero-based video device index (default: 0).
     * @return The cached handle on success, or INVALID_DS_HANDLE if out of range
     *         or not yet acquired.
     */
    int32_t getCachedVideoDeviceHandle(int32_t index = 0) const
    {
        if (index < 0 || static_cast<size_t>(index) >= _videoDeviceHandles.size()) {
            LOGERR("getCachedVideoDeviceHandle: index %d out of range (count=%zu)",
                   index, _videoDeviceHandles.size());
            return INVALID_DS_HANDLE;
        }
        const int32_t handle = _videoDeviceHandles[static_cast<size_t>(index)];
        if (INVALID_DS_HANDLE == handle) {
            LOGERR("getCachedVideoDeviceHandle: handle for index %d not yet acquired", index);
        }
        return handle;
    }

    /**
     * @brief Returns all cached audio port handles as (portName, handle) pairs.
     *
     * Use this instead of iterating _audioPortHandles directly in client plugin code.
     * The order is unspecified (map iteration order).
     */
    std::vector<std::pair<std::string, int32_t>> getAudioPortHandleEntries() const
    {
        std::vector<std::pair<std::string, int32_t>> entries;
        for (const auto& kv : _audioPortHandles) {
            entries.emplace_back(kv.first, kv.second);
        }
        return entries;
    }

    /**
     * @brief Returns true if a handle is cached for the given audio port name.
     *
     * Use this instead of _audioPortHandles.count()/find() checks in client plugin code.
     *
     * @param portName  Port name (e.g. "HDMI0", "HDMI_ARC0")
     */
    bool hasAudioPortHandle(const std::string& portName) const
    {
        return _audioPortHandles.count(portName) > 0;
    }

    /**
     * @brief Returns all cached video port handles as (portName, handle) pairs.
     *
     * Use this instead of iterating _videoPortHandles directly in client plugin code.
     * The order is unspecified (map iteration order).
     */
    std::vector<std::pair<std::string, int32_t>> getVideoPortHandleEntries() const
    {
        std::vector<std::pair<std::string, int32_t>> entries;
        for (const auto& kv : _videoPortHandles) {
            entries.emplace_back(kv.first, kv.second);
        }
        return entries;
    }

    /**
     * @brief Replicates AudioOutputPort::isConnected() per-type logic using COM-RPC.
     *
     * Fills @p portHandle from the internal _audioPortHandles cache via
     * getCachedAudioPortHandle(), then checks per-type connectivity.
     * Callers do NOT need to call getCachedAudioPortHandle() separately.
     *
     * Returns false immediately (and logs the error) if the port is not in the
     * cache (i.e. not yet registered in OnDeviceSettingsActivated).
     *
     * DS_IARM reference (audioOutputPort.cpp / hdmiIn.cpp):
     *   HDMI type      -> VideoOutputPortConfig::getPort("HDMI0").isDisplayConnected()
     *                     COM-RPC: IDeviceSettingsVideoPort::IsVideoPortDisplayConnected
     *   HDMI_ARC type  -> dsGetHDMIARCPortId + HdmiInput::isPortConnected
     *                     COM-RPC: GetAudioHDMIARCPortId + IDeviceSettingsHDMIIn::GetHDMIInStatus
     *   HEADPHONE type -> dsAudioOutIsConnected(handle)
     *                     COM-RPC: IDeviceSettingsAudio::IsAudioOutputConnected
     *   Others (SPDIF/SPEAKER/LR) -> always true
     *
     * @param audio      Audio sub-interface already acquired by the caller (not released here)
     * @param portName   Audio port name (e.g. "HDMI0", "HDMI_ARC0", "SPDIF0")
     * @param portHandle [OUT] Set to the cached audio port handle on success, -1 if not found.
     * @return true if the port was found in the cache AND is connected; false otherwise.
     *
     * Usage:
     * @code
     *   int32_t audioHandle = -1;
     *   if (isAudioOutputPortConnected(audio, audioPort, audioHandle)) {
     *       audio->SomeApiCall(audioHandle, ...);
     *   } else {
     *       LOGERR("aport is not connected!");
     *       success = false;
     *   }
     * @endcode
     */
    bool isAudioOutputPortConnected(
        Exchange::IDeviceSettingsAudio* audio,
        const std::string& portName,
        int32_t& portHandle)
    {
        Core::hresult comResult = Core::ERROR_NONE;
        portHandle = getCachedAudioPortHandle(portName);
        if (INVALID_DS_HANDLE == portHandle) {
            return false;  // getCachedAudioPortHandle already logged the error
        }
        if (portName.find("HDMI") != std::string::npos && portName.find("ARC") == std::string::npos) {
            // HDMI audio port -- connected iff the video display is connected
            // mirrors: VideoOutputPortConfig::getPort("HDMI0").isDisplayConnected()
            const auto vpIt = _videoPortHandles.find(portName);
            if (vpIt != _videoPortHandles.end()) {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    bool connected = false;
                    comResult = vp->IsVideoPortDisplayConnected(vpIt->second, connected);
                    if (comResult != Core::ERROR_NONE) {
                        LOGERR("IsVideoPortDisplayConnected failed for '%s': %u",portName.c_str(), comResult);
                    }
                    else {
                        LOGINFO("IsVideoPortDisplayConnected('%s') returned %d", portName.c_str(), connected);
                    }
                    vp->Release();
                    return connected;
                }
            }
            return false;
        } else if (portName.find("HDMI_ARC") != std::string::npos) {
            // DS_IARM: dsGetHDMIARCPortId(&portId) then HdmiInput::isPortConnected(portId)
            //          -> dsHdmiInGetStatus() -> Status.isPortConnected[portId]
            // COM-RPC: GetAudioHDMIARCPortId gives the HDMI-In port index;
            //          GetHDMIInStatus returns a per-port connection status iterator;
            //          index into it at arcPortId to read isPortConnected.
            int32_t arcPortId = -1;
            if (audio != nullptr)
            {
                comResult = audio->GetAudioHDMIARCPortId(portHandle, arcPortId);
                if (comResult != Core::ERROR_NONE && arcPortId < 0 ) {
                    LOGERR("GetAudioHDMIARCPortId failed for '%s' ArcPortId: %d, Error: %u", portName.c_str(), arcPortId, comResult);
                    return false;
                }
                auto* hdmiIn = AcquireSubInterface<Exchange::IDeviceSettingsHDMIIn>();
                if (hdmiIn != nullptr) {
                    Exchange::IDeviceSettingsHDMIIn::HDMIInStatus hdmiStatus{};
                    Exchange::IDeviceSettingsHDMIIn::IHDMIInPortConnectionStatusIterator* portIt = nullptr;
                    bool connected = false;
                    comResult = hdmiIn->GetHDMIInStatus(hdmiStatus, portIt);
                    if (comResult == Core::ERROR_NONE && portIt != nullptr) {
                        Exchange::IDeviceSettingsHDMIIn::HDMIPortConnectionStatus portStatus{};
                        int32_t idx = 0;
                        while (portIt->Next(portStatus)) {
                            if (idx == arcPortId) {
                                connected = portStatus.isPortConnected;
                                LOGINFO("GetHDMIInStatus('%s') portId=%d isPortConnected=%d", portName.c_str(), arcPortId, connected);
                                break;
                            }
                            ++idx;
                        }
                        portIt->Release();
                    }
                    else {
                        LOGERR("GetHDMIInStatus failed for '%s': %u", portName.c_str(), comResult);
                    }
                    hdmiIn->Release();
                    return connected;
                }
                else {
                    LOGERR("IDeviceSettingsHDMIIn interface is null for '%s'", portName.c_str());
                }
            }
            else {
                LOGERR("Audio interface is null for '%s'", portName.c_str());
            }
            return false;
        } else if (portName.find("HEADPHONE") != std::string::npos) {
            // DS_IARM: dsAudioOutIsConnected(handle)
            // COM-RPC: IsAudioOutputConnected
            bool connected = false;
            if (audio == nullptr) {
                LOGERR("Audio interface is null for '%s'", portName.c_str());
            }
            else {
                comResult = audio->IsAudioOutputConnected(portHandle, connected);
                if (comResult != Core::ERROR_NONE) {
                    LOGERR("IsAudioOutputConnected failed for '%s': %u", portName.c_str(), comResult);
                }
                else {
                    LOGINFO("IsAudioOutputConnected('%s') returned %d", portName.c_str(), connected);
                }
            }
            return connected;
        } else {
            LOGINFO("'%s' is not HDMI, HDMI_ARC, or HEADPHONE — assuming connected", portName.c_str());
            // SPDIF, SPEAKER, LR/IDLR -- always connected (DS_IARM else branch returns true)
            return true;
        }
    }

protected:
    // -------------------------------------------------------------------------
    // Cached port handles and config stores
    // Populated in OnDeviceSettingsActivated(), cleared in OnDeviceSettingsDeactivated().
    // Derived classes may read/write these directly in their overrides.
    // -------------------------------------------------------------------------
    std::map<std::string, int32_t> _videoPortHandles;   ///< key = port name e.g. "HDMI0"
    std::map<std::string, int32_t> _audioPortHandles;   ///< key = port name e.g. "HDMI0"
    std::map<std::string, int32_t> _displayHandles;     ///< key = port name
    std::vector<int32_t>           _videoDeviceHandles; ///< index = device index (0-based)
    VideoPortConfigStore           _vpConfigStore;      ///< port types, names, resolutions
    AudioConfigStore               _audioConfigStore;   ///< audio port types and names
    VideoDeviceConfigStore         _vdConfigStore;      ///< video device capabilities
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
            LOGINFO("DeviceSettingsClientHelper[%s]: DeviceSettings activated — re-registering event notifications", _callsign.c_str());
            _videoPortHandles.clear();
            _audioPortHandles.clear();
            _displayHandles.clear();
            _videoDeviceHandles.clear();
            OnDeviceSettingsActivated();
        } else {
            LOGINFO("DeviceSettingsClientHelper[%s]: DeviceSettings deactivated — cleaning up stale state", _callsign.c_str());
            OnDeviceSettingsDeactivated();
            _videoPortHandles.clear();
            _audioPortHandles.clear();
            _displayHandles.clear();
            _videoDeviceHandles.clear();
        }
    }

private:
    PluginHost::IShell* _service;
    string              _callsign;
};

} // namespace Plugin
} // namespace WPEFramework