/*
 * BLE Service Callbacks for DGT3000 Gateway
 *
 * This header defines the callback classes for handling events from the
 * BLE server and its characteristics.
 * 
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef BLE_SERVICE_CALLBACKS_H
#define BLE_SERVICE_CALLBACKS_H

#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLEDescriptor.h>

class DGT3000BLEService; // Forward declaration to avoid circular dependency

/**
 * @class DGT3000BaseCharacteristicCallbacks
 * @brief A base class for characteristic callbacks to hold a reference to the main service.
 */
class DGT3000BaseCharacteristicCallbacks : public BLECharacteristicCallbacks {
protected:
    DGT3000BLEService* m_service;

public:
    explicit DGT3000BaseCharacteristicCallbacks(DGT3000BLEService* service) : m_service(service) {}
};

/**
 * @class DGT3000ServerCallbacks
 * @brief Handles server-level BLE events like client connection and disconnection.
 */
class DGT3000ServerCallbacks : public BLEServerCallbacks {
private:
    DGT3000BLEService* m_service;
public:
    explicit DGT3000ServerCallbacks(DGT3000BLEService* service) : m_service(service) {}
    void onConnect(BLEServer* server) override;
    void onDisconnect(BLEServer* server) override;
};

/**
 * @class DGT3000CommandCallbacks
 * @brief Handles write events on the command characteristic.
 */
class DGT3000CommandCallbacks : public DGT3000BaseCharacteristicCallbacks {
public:
    using DGT3000BaseCharacteristicCallbacks::DGT3000BaseCharacteristicCallbacks;
    void onWrite(BLECharacteristic* characteristic) override;
};

/**
 * @class DGT3000EventCallbacks
 * @brief Handles read events on the event characteristic.
 * @note Reading this characteristic is not the primary way to get events (notifications are preferred).
 */
class DGT3000EventCallbacks : public DGT3000BaseCharacteristicCallbacks {
public:
    using DGT3000BaseCharacteristicCallbacks::DGT3000BaseCharacteristicCallbacks;
    void onRead(BLECharacteristic* characteristic) override;
};

/**
 * @class DGT3000StatusCallbacks
 * @brief Handles read events on the status characteristic.
 */
class DGT3000StatusCallbacks : public DGT3000BaseCharacteristicCallbacks {
public:
    using DGT3000BaseCharacteristicCallbacks::DGT3000BaseCharacteristicCallbacks;
    void onRead(BLECharacteristic* characteristic) override;
};

/**
 * @class DGT3000EventDescriptorCallbacks
 * @brief Handles writes to the event characteristic's CCCD (0x2902 descriptor).
 * This is triggered when a client subscribes to or unsubscribes from notifications.
 */
class DGT3000EventDescriptorCallbacks : public BLEDescriptorCallbacks {
private:
    DGT3000BLEService* m_service;

public:
    explicit DGT3000EventDescriptorCallbacks(DGT3000BLEService* service) : m_service(service) {}
    void onWrite(BLEDescriptor* pDescriptor) override;
};

#endif // BLE_SERVICE_CALLBACKS_H
