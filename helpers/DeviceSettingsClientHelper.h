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
 *   1. client calls DSHelper::Open(service)  in its Configure(IShell*)
 *   2. Thunder connects asynchronously to "DeviceSettings"
 *   3. DeviceSettings activates → Operational(true) →
 *        OnDeviceSettingsActivated()  ← override to (re-)register events
 *   4. DeviceSettings deactivates (crash/restart) → Operational(false) →
 *        OnDeviceSettingsDeactivated() ← override to invalidate cached state
 *   5. DeviceSettings restarts → Operational(true) again →
 *        OnDeviceSettingsActivated() re-called automatically
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

#include <interfaces/IDeviceSettings.h>
#include <plugins/plugins.h>
#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

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

    virtual ~DeviceSettingsClientHelper() = default;

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
        ASSERT(service != nullptr);
        ASSERT(_service == nullptr);

        _service  = service;
        _service->AddRef();
        _callsign = callsign;

        const uint32_t result = BaseClass::Open(_service, callsign);
        if (result != Core::ERROR_NONE) {
            LOGERR("DeviceSettingsClientHelper::Open(%s) failed: %u",
                   callsign.c_str(), result);
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
            LOGERR("DeviceSettingsClientHelper[%s]: QueryInterface<sub> returned nullptr",
                   _callsign.c_str());
        }
        return sub;
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
     *           vd->Register(this);
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
