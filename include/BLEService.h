/*
 * BLE Service for DGT3000 Gateway
 *
 * This header defines the BLE GATT service, characteristics, and logic
 * for interacting with the DGT3000 gateway.
 * 
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "BLEGatewayTypes.h"
#include "00-GatewayConstants.h"
#include "QueueManager.h"
#include <logging.hpp>
#include <memory>
#include "BLEServiceCallbacks.h"

/**
 * @class DGT3000BLEService
 * @brief Manages the BLE server, service, and characteristics for the gateway.
 *
 * This class is responsible for:
 * - Setting up and managing the BLE server and advertising.
 * - Creating the DGT3000 GATT service with its specific characteristics.
 * - Handling read/write requests from BLE clients.
 * - Sending notifications for events (e.g., button presses, time updates).
 * - Processing events and responses from the I2C task via queues.
 */
class DGT3000BLEService : public esp32m::SimpleLoggable {
public:
    // BLE Components
    BLEServer* bleServer;
    BLEService* dgt3000Service;
    BLECharacteristic* commandCharacteristic;
    BLECharacteristic* eventCharacteristic;
    BLECharacteristic* statusCharacteristic;
    BLECharacteristic* protocolVersionCharacteristic;
    BLEAdvertising* advertising;
    
    // Connection state
    bool deviceConnected;
    bool oldDeviceConnected;
    uint32_t connectionTime;
    
    // Dependencies
    QueueManager* queueManager;
    SystemStatus* systemStatus;
    
    // JSON documents for serialization
    JsonDocument commandBuffer;
    JsonDocument eventBuffer;
    JsonDocument _responseDoc;
    
    // Statistics for notifications
    struct {
        uint32_t notificationsSent;
        uint32_t notificationsFailed;
        uint32_t lastNotificationTime;
    } _notificationStats;

    // Callback pointers to manage their lifecycle
    std::unique_ptr<DGT3000ServerCallbacks> _serverCallbacks;
    std::unique_ptr<DGT3000CommandCallbacks> _commandCallbacks;
    std::unique_ptr<DGT3000EventCallbacks> _eventCallbacks;
    std::unique_ptr<DGT3000StatusCallbacks> _statusCallbacks;
    std::unique_ptr<DGT3000EventDescriptorCallbacks> _eventDescriptorCallbacks;
    
public:
    /**
     * @brief Constructs a new DGT3000BLEService.
     * @param queueMgr Pointer to the QueueManager for inter-task communication.
     * @param status Pointer to the global SystemStatus object.
     */
    DGT3000BLEService(QueueManager* queueMgr, SystemStatus* status);
    
    /**
     * @brief Destroys the DGT3000BLEService and cleans up resources.
     */
    ~DGT3000BLEService();
    
    /**
     * @brief Initializes the entire BLE stack, including server, service, and advertising.
     * @return true if initialization is successful, false otherwise.
     */
    bool initialize();
    
    /**
     * @brief Cleans up the BLE stack.
     */
    void cleanup();
    
    /**
     * @brief Checks if a BLE client is currently connected.
     * @return true if a client is connected, false otherwise.
     */
    bool isConnected() const { return deviceConnected; }
    
    /**
     * @brief Gets the time in milliseconds since the current connection was established.
     * @return Connection duration in milliseconds.
     */
    uint32_t getConnectionTime() const { return connectionTime; }
    
    /**
     * @brief Sends a DGT event as a BLE notification.
     * @param event The DGTEvent object to send.
     * @return true if the notification was sent successfully.
     */
    bool sendEvent(const DGTEvent& event);
    
    /**
     * @brief Sends a JSON string as a BLE notification on the event characteristic.
     * @param jsonData The JSON string to send.
     * @return true if the notification was sent successfully.
     */
    bool sendNotification(const char* jsonData);
    
    /**
     * @brief Updates the general system status information.
     */
    void updateStatus();
    
    /**
     * @brief Main processing loop for the BLE service, intended to be called periodically.
     * Handles event/response queues and connection state changes.
     */
    void processEvents();
    
    // --- Public handlers for callbacks ---
    void handleConnect();
    void handleDisconnect();
    void handleEventRead(BLECharacteristic* characteristic);
    void handleClientSubscription();
    const char* getCachedStatusJson() const { return m_cachedStatusJson.c_str(); }
    
    /**
     * @brief Proactively updates the cached status JSON string.
     */
    void updateStatusCache();

private:
    std::string m_cachedStatusJson; ///< Cached system status JSON string for quick reads.

    // Internal setup methods
    bool setupBLEServer();
    bool setupDGT3000Service();
    bool setupCharacteristics();
    bool startAdvertising();
    
    /**
     * @brief Processes the queue of events coming from the I2C task.
     */
    void processNotificationQueue();
    
    /**
     * @brief Processes the queue of command responses coming from the I2C task.
     */
    void processResponseQueue();

    /**
     * @brief Updates internal statistics for notifications.
     * @param success Whether the notification was sent successfully.
     */
    void updateNotificationStats(bool success);
};

// Global instance of the BLE service
extern std::unique_ptr<DGT3000BLEService> g_bleService;

/**
 * @brief Generates a JSON-formatted error response string.
 * @param commandId The ID of the command that failed.
 * @param errorCode The system error code.
 * @param message A descriptive error message.
 * @return A String object containing the JSON response.
 */
String generateErrorResponse(const char* commandId, SystemErrorCode errorCode, const char* message);

#endif // BLE_SERVICE_H
