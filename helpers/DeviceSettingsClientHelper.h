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
 *     , public DeviceSettingsClientHelper                         // ← always root
 *     , public Exchange::IDeviceSettingsVideoDevice::INotification
 * {
 *     int32_t _videoDeviceHandle { -1 };
 *
 *     uint32_t Configure(PluginHost::IShell* service) override {
 *         DeviceSettingsClientHelper::Open(service);
 *         return Core::ERROR_NONE;
 *     }
 *
 *     ~FrameRateImplementation() {
 *         auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
 *         if (vd) { vd->Unregister(this); vd->Release(); }
 *         DeviceSettingsClientHelper::Close();
 *     }
 *
 *     // Called when DeviceSettings (re-)activates
 *     void OnDeviceSettingsActivated() override {
 *         auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
 *         if (vd) {
 *             vd->GetVideoDeviceHandle(0, _videoDeviceHandle);
 *             vd->Register(this);    // subscribe to framerate events
 *             vd->Release();
 *         }
 *     }
 *
 *     void OnDeviceSettingsDeactivated() override {
 *         _videoDeviceHandle = -1;
 *     }
 *
 *     // IDeviceSettingsVideoDevice::INotification
 *     void OnDisplayFrameratePreChange(const string& fr) override { ... }
 *     void OnDisplayFrameratePostChange(const string& fr) override { ... }
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
    static constexpr const char* kDefaultCallsign = "DeviceSettings";

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
     * @param callsign  Plugin callsign (default: "DeviceSettings")
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


/**
 * @file DeviceSettingsClientHelper.h
 *
 * @brief Helper base class for client Thunder plugins that connect to the
 *        DeviceSettings plugin via COM-RPC (PluginSmartInterfaceType).
 *
 * ## Purpose
 * This helper replaces the pattern of:
 *   - Linking libds.so at compile time
 *   - Calling device::Host::getInstance() / device::VideoDevice etc.
 *   - Registering IARM bus event listeners
 *
 * Instead the client inherits DeviceSettingsClientHelper<INTERFACE>, calls
 * Open() during Configure(), and overrides OnDeviceSettingsActivated() /
 * OnDeviceSettingsDeactivated() to manage COM-RPC event registration.
 *
 * ## Activation lifecycle
 *
 *   1. Configure(IShell*) is called → client calls DSHelper::Open(service)
 *   2. Thunder connects to DeviceSettings plugin asynchronously
 *   3. When DeviceSettings activates → Operational(true) fires → calls
 *      OnDeviceSettingsActivated() → client acquires interface & registers events
 *   4. If DeviceSettings deactivates (crash / restart) → Operational(false) fires
 *      → OnDeviceSettingsDeactivated() called → client cleans up stale state
 *   5. When DeviceSettings comes back online → Operational(true) fires again
 *      → client re-registers events automatically
 *
 * ## Usage Example
 *
 * @code
 * // FrameRateImplementation.h
 * #include "DeviceSettingsClientHelper.h"
 * #include <interfaces/IDeviceSettingsVideoDevice.h>
 *
 * class FrameRateImplementation
 *     : public Exchange::IFrameRate
 *     , public Exchange::IConfiguration
 *     , public DeviceSettingsClientHelper<Exchange::IDeviceSettingsVideoDevice>
 *     , public Exchange::IDeviceSettingsVideoDevice::INotification
 * {
 *     using DSHelper = DeviceSettingsClientHelper<Exchange::IDeviceSettingsVideoDevice>;
 *
 *     uint32_t Configure(PluginHost::IShell* service) override {
 *         DSHelper::Open(service);
 *         return Core::ERROR_NONE;
 *     }
 *
 *     ~FrameRateImplementation() {
 *         DSHelper::Close();
 *     }
 *
 *     // Re-register COM-RPC events every time DeviceSettings (re-)activates
 *     void OnDeviceSettingsActivated() override {
 *         auto* vd = DSHelper::AcquireInterface();
 *         if (vd != nullptr) {
 *             int32_t handle = 0;
 *             vd->GetVideoDeviceHandle(0, handle);   // index 0 = primary video device
 *             _videoDeviceHandle = handle;
 *             vd->Register(this);                    // subscribe to framerate events
 *             vd->Release();
 *         }
 *     }
 *
 *     void OnDeviceSettingsDeactivated() override {
 *         _videoDeviceHandle = -1;                   // invalidate cached handle
 *     }
 *
 *     // IDeviceSettingsVideoDevice::INotification
 *     void OnDisplayFrameratePreChange(const string& frameRate) override { ... }
 *     void OnDisplayFrameratePostChange(const string& frameRate) override { ... }
 * };
 * @endcode
 */

#include <plugins/plugins.h>
#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

/**
 * @brief CRTP/inheritance base class for Thunder plugins that use DeviceSettings
 *        sub-interfaces via PluginSmartInterfaceType.
 *
 * @tparam INTERFACE  One of the Exchange::IDeviceSettingsXxx interfaces defined in
 *                    entservices-apis/apis/DeviceSettings/ (e.g.
 *                    Exchange::IDeviceSettingsVideoDevice).
 */
template <typename INTERFACE>
class DeviceSettingsClientHelper
    : public RPC::PluginSmartInterfaceType<INTERFACE>
{
    using BaseClass = RPC::PluginSmartInterfaceType<INTERFACE>;

public:
    // Default DeviceSettings Thunder plugin callsign
    static constexpr const char* kDefaultCallsign = "DeviceSettings";

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
     * @brief Connect to the DeviceSettings plugin.
     *
     * Call this from your Configure(PluginHost::IShell*) implementation.
     * Internally holds an AddRef on @p service — balanced by Close().
     *
     * When DeviceSettings is already active the Operational(true) callback
     * fires synchronously inside this call, so OnDeviceSettingsActivated()
     * may be invoked before Open() returns.
     *
     * @param service   IShell* provided by Configure()
     * @param callsign  DeviceSettings plugin callsign (default: "DeviceSettings")
     * @return Core::ERROR_NONE on success; other error codes indicate the
     *         Thunder framework could not set up the link (events will still
     *         fire when DeviceSettings becomes active later).
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
            LOGERR("DeviceSettingsClientHelper::Open(%s) failed with error %u",
                   callsign.c_str(), result);
        } else {
            LOGINFO("DeviceSettingsClientHelper::Open(%s) succeeded", callsign.c_str());
        }
        return result;
    }

    /**
     * @brief Disconnect from DeviceSettings and release the IShell reference.
     *
     * Call this from your destructor BEFORE releasing your own _service pointer.
     * It is safe to call Close() even if Open() was never called or already
     * called before.
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
     * @brief Acquire an AddRef'd pointer to the DeviceSettings sub-interface.
     *
     * The caller MUST call Release() on the returned pointer exactly once when
     * done.  Returns nullptr when DeviceSettings is not currently active.
     *
     * Typical usage:
     * @code
     *   auto* iface = AcquireInterface();
     *   if (iface != nullptr) {
     *       iface->SomeMethod(...);
     *       iface->Release();
     *   }
     * @endcode
     */
    INTERFACE* AcquireInterface()
    {
        return BaseClass::Interface();
    }

protected:
    /**
     * @brief Called when the DeviceSettings plugin becomes active (or re-activates).
     *
     * Override to (re-)register COM-RPC event notifications with the interface.
     * This will be called:
     *   - On initial DeviceSettings startup (possibly before Open() returns)
     *   - After a DeviceSettings restart / crash recovery
     *
     * Implementation pattern:
     * @code
     *   void OnDeviceSettingsActivated() override {
     *       auto* iface = AcquireInterface();
     *       if (iface != nullptr) {
     *           iface->Register(this);   // 'this' implements INTERFACE::INotification
     *           iface->Release();
     *       }
     *   }
     * @endcode
     */
    virtual void OnDeviceSettingsActivated() = 0;

    /**
     * @brief Called when the DeviceSettings plugin deactivates (shutdown or crash).
     *
     * At this point the COM-RPC connection is already severed — do NOT attempt
     * to call AcquireInterface() or any interface methods from here.
     *
     * Use this to:
     *   - Invalidate cached handles (e.g. _videoDeviceHandle = -1)
     *   - Reset state that depends on DeviceSettings being alive
     *   - Optionally signal error condition to callers
     */
    virtual void OnDeviceSettingsDeactivated() = 0;

private:
    /**
     * @brief Internal Thunder callback — sealed from further overriding.
     *
     * Dispatches to OnDeviceSettingsActivated() / OnDeviceSettingsDeactivated().
     * Derived classes must NOT override this; override the named methods above.
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
