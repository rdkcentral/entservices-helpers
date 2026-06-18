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
 * @file DeviceSettingsInterface.h
 *
 * @brief Convenience include for client plugins that use DeviceSettingsClientHelper.
 *
 * Including this single header provides:
 *   - DeviceSettingsClientHelper  (base class — inherits PluginSmartInterfaceType<IDeviceSettings>)
 *   - All DeviceSettings sub-interface headers (IDeviceSettingsVideoDevice, Audio, etc.)
 *
 * ## How to use (client plugin pattern)
 *
 *   1. Inherit DeviceSettingsClientHelper (always connects via root IDeviceSettings).
 *   2. Call Open(service) in your Configure(IShell*).
 *   3. Override OnDeviceSettingsActivated() to acquire sub-interfaces via
 *      AcquireSubInterface<T>() and register INotification callbacks.
 *   4. Override OnDeviceSettingsDeactivated() to invalidate cached handles.
 *   5. Use AcquireSubInterface<T>() in method bodies + Release() when done.
 *
 * ## Sub-interface selection quick reference
 *
 *   | Old libds API                                     | AcquireSubInterface<T> where T =                |
 *   |---------------------------------------------------|-------------------------------------------------|
 *   | device::VideoDevice::getCurrentDisframerate()     | Exchange::IDeviceSettingsVideoDevice            |
 *   | device::VideoDevice::setDisplayframerate()        | Exchange::IDeviceSettingsVideoDevice            |
 *   | device::VideoDevice::getFRFMode/setFRFMode()      | Exchange::IDeviceSettingsVideoDevice            |
 *   | device::VideoOutputPort::setResolution()          | Exchange::IDeviceSettingsVideoPort              |
 *   | device::VideoOutputPort::getHDCPStatus()          | Exchange::IDeviceSettingsVideoPort              |
 *   | device::AudioOutputPort::setStereoMode()          | Exchange::IDeviceSettingsAudio                  |
 *   | device::AudioOutputPort::setMuted()               | Exchange::IDeviceSettingsAudio                  |
 *   | device::Host::getHostEDID()                       | Exchange::IDeviceSettingsHost                   |
 *   | device::Host::getCPUTemperature()                 | Exchange::IDeviceSettingsHost                   |
 *   | device::FrontPanelIndicator::setBrightness()      | Exchange::IDeviceSettingsFPD                    |
 *   | device::HdmiInput / IARM_BUS_DSMGR_EVENT_HDMI_IN | Exchange::IDeviceSettingsHDMIIn                 |
 *   | device::CompositeInput events                     | Exchange::IDeviceSettingsCompositeIn            |
 *   | device::VideoOutputPort display events            | Exchange::IDeviceSettingsDisplay                |
 *
 * ## Example (FrameRate plugin)
 *
 * @code
 * #include "DeviceSettingsInterface.h"
 *
 * class FrameRateImplementation
 *     : public Exchange::IFrameRate
 *     , public Exchange::IConfiguration
 *     , public DeviceSettingsClientHelper   // root IDeviceSettings COM-RPC (single connection)
 *     // NOTE: do NOT inherit IDeviceSettingsXxx::INotification directly —
 *     //       use an inner Notification delegate class instead (see below).
 * {
 *     // -----------------------------------------------------------------------
 *     // Inner notification delegate — RECOMMENDED PATTERN for all client plugins
 *     // Register &_notification (not 'this') with the sub-interface.
 *     // -----------------------------------------------------------------------
 *     class Notification : public Exchange::IDeviceSettingsVideoDevice::INotification {
 *     public:
 *         explicit Notification(FrameRateImplementation& p) : _parent(p) {}
 *
 *         void OnDisplayFrameratePreChange(const string& fr) override {
 *             _parent.OnDisplayFrameratePreChange(fr);
 *         }
 *         void OnDisplayFrameratePostChange(const string& fr) override {
 *             _parent.OnDisplayFrameratePostChange(fr);
 *         }
 *
 *         BEGIN_INTERFACE_MAP(Notification)
 *             INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoDevice::INotification)
 *         END_INTERFACE_MAP
 *     private:
 *         FrameRateImplementation& _parent;
 *     };
 *
 *     int32_t    _videoDeviceHandle { -1 };
 *     Notification _notification    { *this };  // delegate instance
 *
 *     uint32_t Configure(PluginHost::IShell* service) override {
 *         DeviceSettingsClientHelper::Open(service);
 *         return Core::ERROR_NONE;
 *     }
 *
 *     void OnDeviceSettingsActivated() override {
 *         auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
 *         if (vd) {
 *             vd->GetVideoDeviceHandle(0, _videoDeviceHandle);
 *             vd->Register(&_notification);   // <-- pass delegate, not 'this'
 *             vd->Release();
 *         }
 *     }
 *
 *     void OnDeviceSettingsDeactivated() override {
 *         _videoDeviceHandle = -1;
 *     }
 *
 *     // Private helpers called by Notification
 *     void OnDisplayFrameratePreChange(const string& fr) { /* dispatch event */ }
 *     void OnDisplayFrameratePostChange(const string& fr) { /* dispatch event */ }
 *
 *     Core::hresult GetDisplayFrameRate(string& framerate, bool& success) override {
 *         if (_videoDeviceHandle < 0) return Core::ERROR_UNAVAILABLE;
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

#include "DeviceSettingsClientHelper.h"

// All DeviceSettings sub-interface headers — include the ones your plugin needs
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsCompositeIn.h>
#include <interfaces/IDeviceSettingsDisplay.h>
#include <interfaces/IDeviceSettingsFPD.h>
#include <interfaces/IDeviceSettingsHDMIIn.h>
#include <interfaces/IDeviceSettingsHost.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>

// DeviceSettingsClientHelper is the single class all clients inherit.
// There are no per-sub-interface typedefs — use DeviceSettingsClientHelper
// and call AcquireSubInterface<Exchange::IDeviceSettingsXxx>() as needed.
