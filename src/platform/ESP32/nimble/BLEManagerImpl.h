/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Provides an implementation of the BLEManager singleton object
 *          for the ESP32 platform (NimBLE).
 */

#ifndef BLE_MANAGER_IMPL_H
#define BLE_MANAGER_IMPL_H

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

#include "sdkconfig.h"

#if CONFIG_BT_NIMBLE_ENABLED

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"

/* GATT context */
struct ble_gatt_char_context {
    uint16_t conn_handle;
    uint16_t attr_handle;
    struct ble_gatt_access_ctxt *ctxt;
    void *arg;
};

namespace chip {
namespace DeviceLayer {
namespace Internal {


/**
 * Concrete implementation of the NetworkProvisioningServer singleton object for the ESP32 platform.
 */
class BLEManagerImpl final : public BLEManager,
                             private ::chip::Ble::BleLayer,
                             //private BleLayer,
                             private BlePlatformDelegate,
                             private BleApplicationDelegate
{
    // Allow the BLEManager interface class to delegate method calls to
    // the implementation methods provided by this class.
    friend BLEManager;

    // ===== Members that implement the BLEManager internal interface.

    CHIP_ERROR _Init(void);
    CHIPoBLEServiceMode _GetCHIPoBLEServiceMode(void);
    CHIP_ERROR _SetCHIPoBLEServiceMode(CHIPoBLEServiceMode val);
    CHIP_ERROR _SetAdvertisingEnabled(bool val);
    CHIP_ERROR _GetDeviceName(char * buf, size_t bufSize);
    CHIP_ERROR _SetDeviceName(const char * deviceName);
    bool _IsAdvertisingEnabled(void);
    bool _IsFastAdvertisingEnabled(void);
    bool _IsAdvertising(void);
    CHIP_ERROR _SetFastAdvertisingEnabled(bool val);

    uint16_t _NumConnections(void);

    void _OnPlatformEvent(const ChipDeviceEvent * event);
    ::chip::Ble::BleLayer * _GetBleLayer(void) const;

    // ===== Members that implement virtual methods on BlePlatformDelegate.

    bool SubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId) override;
    bool UnsubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId) override;
    bool CloseConnection(BLE_CONNECTION_OBJECT conId) override;
    uint16_t GetMTU(BLE_CONNECTION_OBJECT conId) const override;
    bool SendIndication(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                        PacketBuffer * pBuf) override;
    bool SendWriteRequest(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                          PacketBuffer * pBuf) override;
    bool SendReadRequest(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                         PacketBuffer * pBuf) override;
    bool SendReadResponse(BLE_CONNECTION_OBJECT conId, BLE_READ_REQUEST_CONTEXT requestContext, const ChipBleUUID * svcId,
                          const ChipBleUUID * charId) override;

    // ===== Members that implement virtual methods on BleApplicationDelegate.

    void NotifyChipConnectionClosed(BLE_CONNECTION_OBJECT conId) override;

    // ===== Members for internal use by the following friends.

    friend BLEManager & BLEMgr(void);
    friend BLEManagerImpl & BLEMgrImpl(void);

    static BLEManagerImpl sInstance;

    // ===== Private members reserved for use by this class only.

    enum
    {
        kFlag_AsyncInitCompleted       = 0x0001, /**< One-time asynchronous initialization actions have been performed. */
        kFlag_ESPBLELayerInitialized   = 0x0002, /**< The ESP BLE layer has been initialized. */
        kFlag_AppRegistered            = 0x0004, /**< The CHIPoBLE application has been registered with the ESP BLE layer. */
        kFlag_AttrsRegistered          = 0x0008, /**< The CHIPoBLE GATT attributes have been registered with the ESP BLE layer. */
        kFlag_GATTServiceStarted       = 0x0010, /**< The CHIPoBLE GATT service has been started. */
        kFlag_AdvertisingConfigured    = 0x0020, /**< CHIPoBLE advertising has been configured in the ESP BLE layer. */
        kFlag_Advertising              = 0x0040, /**< The system is currently CHIPoBLE advertising. */
        kFlag_ControlOpInProgress      = 0x0080, /**< An async control operation has been issued to the ESP BLE layer. */
        kFlag_AdvertisingEnabled       = 0x0100, /**< The application has enabled CHIPoBLE advertising. */
        kFlag_FastAdvertisingEnabled   = 0x0200, /**< The application has enabled fast advertising. */
        kFlag_UseCustomDeviceName      = 0x0400, /**< The application has configured a custom BLE device name. */
        kFlag_AdvertisingRefreshNeeded = 0x0800, /**< The advertising configuration/state in ESP BLE layer needs to be updated. */
    };

    enum
    {
        kMaxConnections      = BLE_LAYER_NUM_BLE_ENDPOINTS,
        kMaxDeviceNameLength = 16
    };

    uint16_t mSubscribedConIds[kMaxConnections];

    struct CHIPoBLEConState
    {
        PacketBuffer * PendingIndBuf;
        uint16_t ConId;
        uint16_t MTU : 10;
        uint16_t Allocated : 1;
        uint16_t Subscribed : 1;
        uint16_t Unused : 4;
    };


    CHIPoBLEConState mCons[kMaxConnections];
    CHIPoBLEServiceMode mServiceMode;
    uint16_t mServiceAttrHandle;
    uint16_t mRXCharAttrHandle;
    uint16_t mTXCharAttrHandle;
    uint16_t mTXCharCCCDAttrHandle;
    uint16_t mFlags;
    uint16_t mNumGAPCons;
    char mDeviceName[kMaxDeviceNameLength + 1];

    void DriveBLEState(void);
    CHIP_ERROR InitESPBleLayer(void);
    CHIP_ERROR ConfigureAdvertisingData(void);
    CHIP_ERROR StartAdvertising(void);
    void HandleRXCharRead(struct ble_gatt_char_context * param);
    void HandleRXCharWrite(struct ble_gatt_char_context * param);
    void HandleTXCharWrite(struct ble_gatt_char_context * param);
    void HandleTXCharRead(struct ble_gatt_char_context * param);
    void HandleTXCharCCCDRead(void * param);
    void HandleTXCharCCCDWrite(struct ble_gap_event * gapEvent);
    CHIP_ERROR HandleTXComplete(struct ble_gap_event * gapEvent);
    CHIP_ERROR HandleGAPConnect(struct ble_gap_event *gapEvent);
    CHIP_ERROR HandleGAPDisconnect(struct ble_gap_event *gapEvent);
    CHIP_ERROR SetSubscribed(uint16_t conId);
    bool UnsetSubscribed(uint16_t conId);
    bool IsSubscribed(uint16_t conId);
    static int ble_svr_gap_event(struct ble_gap_event *event, void *arg);

    static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt,
                            void *arg);

    static void DriveBLEState(intptr_t arg);
};

/**
 * Returns a reference to the public interface of the BLEManager singleton object.
 *
 * Internal components should use this to access features of the BLEManager object
 * that are common to all platforms.
 */
inline BLEManager & BLEMgr(void)
{
    return BLEManagerImpl::sInstance;
}

/**
 * Returns the platform-specific implementation of the BLEManager singleton object.
 *
 * Internal components can use this to gain access to features of the BLEManager
 * that are specific to the ESP32 platform.
 */
inline BLEManagerImpl & BLEMgrImpl(void)
{
    return BLEManagerImpl::sInstance;
}

inline ::chip::Ble::BleLayer * BLEManagerImpl::_GetBleLayer() const
{
    return (BleLayer *) (this);
}

inline BLEManager::CHIPoBLEServiceMode BLEManagerImpl::_GetCHIPoBLEServiceMode(void)
{
    return mServiceMode;
}

inline bool BLEManagerImpl::_IsAdvertisingEnabled(void)
{
    return GetFlag(mFlags, kFlag_AdvertisingEnabled);
}

inline bool BLEManagerImpl::_IsFastAdvertisingEnabled(void)
{
    return GetFlag(mFlags, kFlag_FastAdvertisingEnabled);
}

inline bool BLEManagerImpl::_IsAdvertising(void)
{
    return GetFlag(mFlags, kFlag_Advertising);
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

#endif // CONFIG_BT_NIMBLE_ENABLED

#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

#endif // BLE_MANAGER_IMPL_H