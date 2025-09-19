/*
 * BLE Service Implementation for DGT3000 Gateway
 *
 * This file implements the BLE GATT service and characteristics
 * for communication with the DGT3000 chess clock.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "BLEService.h"
#include "BLEServiceCallbacks.h"
#include "driver/temp_sensor.h" // Required for ESP32 temperature sensor

using namespace esp32m;

// =============================================================================
// DGT3000BLEService Implementation
// =============================================================================

DGT3000BLEService::DGT3000BLEService(QueueManager* queueMgr, SystemStatus* status)
    : SimpleLoggable("ble"),
      bleServer(nullptr),
      dgt3000Service(nullptr),
      commandCharacteristic(nullptr),
      eventCharacteristic(nullptr),
      statusCharacteristic(nullptr),
      protocolVersionCharacteristic(nullptr),
      advertising(nullptr),
      deviceConnected(false),
      oldDeviceConnected(false),
      connectionTime(0),
      queueManager(queueMgr),
      systemStatus(status),
      m_cachedStatusJson("")
{
    // Initialize notification statistics.
    _notificationStats.notificationsSent = 0;
    _notificationStats.notificationsFailed = 0;
    _notificationStats.lastNotificationTime = 0;
}

DGT3000BLEService::~DGT3000BLEService() {
    cleanup();
}

bool DGT3000BLEService::initialize() {
    logI("Initializing DGT3000 BLE Service...");
    
    BLEDevice::init(BLE_DEVICE_NAME);
    logD("BLE Device initialized: %s", BLE_DEVICE_NAME);
    
    if (!setupBLEServer() || !setupDGT3000Service() || !setupCharacteristics() || !startAdvertising()) {
        logE("Failed to initialize BLE stack");
        cleanup();
        return false;
    }
    
    if (systemStatus) {
        systemStatus->systemState = SystemState::IDLE;
        systemStatus->bleConnectionState = ConnectionState::DISCONNECTED;
        systemStatus->updateActivity();
    }
    
    logI("DGT3000 BLE Service initialized successfully");
    return true;
}

void DGT3000BLEService::cleanup() {
    logI("Cleaning up BLE Service...");
    
    if (advertising) {
        BLEDevice::stopAdvertising();
    }
    
    // unique_ptr automatically handles deletion of callback objects.
    _serverCallbacks.reset();
    _commandCallbacks.reset();
    _eventCallbacks.reset();
    _statusCallbacks.reset();
    _eventDescriptorCallbacks.reset();
    
    // De-initialize the BLE device.
    BLEDevice::deinit(false);
    
    deviceConnected = false;
    oldDeviceConnected = false;
    
    logI("BLE Service cleanup complete");
}

bool DGT3000BLEService::setupBLEServer() {
    bleServer = BLEDevice::createServer();
    if (!bleServer) {
        logE("Failed to create BLE Server");
        return false;
    }
    
    _serverCallbacks = std::unique_ptr<DGT3000ServerCallbacks>(new DGT3000ServerCallbacks(this));
    bleServer->setCallbacks(_serverCallbacks.get());
    logD("BLE Server created and callbacks set");
    return true;
}

bool DGT3000BLEService::setupDGT3000Service() {
    dgt3000Service = bleServer->createService(BLE_DGT3000_SERVICE_UUID);
    if (!dgt3000Service) {
        logE("Failed to create DGT3000 Service");
        return false;
    }
    logD("DGT3000 Service created (UUID: %s)", BLE_DGT3000_SERVICE_UUID);
    return true;
}

bool DGT3000BLEService::setupCharacteristics() {
    // Protocol Version Characteristic (Read-only)
    protocolVersionCharacteristic = dgt3000Service->createCharacteristic(
        BLE_PROTOCOL_VERSION_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    protocolVersionCharacteristic->setValue(BLE_PROTOCOL_VERSION);
    logD("Protocol Version characteristic created");

    // Command Characteristic (Write-only)
    commandCharacteristic = dgt3000Service->createCharacteristic(
        BLE_COMMAND_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    _commandCallbacks = std::unique_ptr<DGT3000CommandCallbacks>(new DGT3000CommandCallbacks(this));
    commandCharacteristic->setCallbacks(_commandCallbacks.get());
    logD("Command characteristic created");
    
    // Event Characteristic (Notify-only)
    eventCharacteristic = dgt3000Service->createCharacteristic(
        BLE_EVENT_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    // Add BLE2902 descriptor to allow clients to subscribe to notifications.
    auto p2902 = new BLE2902();
    _eventDescriptorCallbacks = std::unique_ptr<DGT3000EventDescriptorCallbacks>(new DGT3000EventDescriptorCallbacks(this));
    p2902->setCallbacks(_eventDescriptorCallbacks.get());
    eventCharacteristic->addDescriptor(p2902);
    logD("Event characteristic created with 2902 descriptor");
    
    // Status Characteristic (Read-only)
    statusCharacteristic = dgt3000Service->createCharacteristic(
        BLE_STATUS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    _statusCallbacks = std::unique_ptr<DGT3000StatusCallbacks>(new DGT3000StatusCallbacks(this));
    statusCharacteristic->setCallbacks(_statusCallbacks.get());
    logD("Status characteristic created");
    
    dgt3000Service->start();
    logD("All characteristics created and service started");
    return true;
}

bool DGT3000BLEService::startAdvertising() {
    advertising = BLEDevice::getAdvertising();
    if (!advertising) {
        logE("Failed to get BLE Advertising instance");
        return false;
    }
    
    advertising->addServiceUUID(BLE_DGT3000_SERVICE_UUID);
    advertising->setScanResponse(false);
    advertising->setMinPreferred(0x0); // General discoverable mode
    
    BLEDevice::startAdvertising();
    logI("BLE Advertising started. Device discoverable as '%s'", BLE_DEVICE_NAME);
    return true;
}

void DGT3000BLEService::processEvents() {
    // Periodically update the system status.
    updateStatus();
    
    // Process event and response queues if a client is connected.
    if (queueManager && deviceConnected) {
        processNotificationQueue();
        processResponseQueue();
    }
    
    // Proactively update the status JSON cache every 2 seconds.
    static uint32_t lastStatusCacheUpdate = 0;
    if (millis() - lastStatusCacheUpdate > 2000) {
        lastStatusCacheUpdate = millis();
        updateStatusCache();
    }
}

void DGT3000BLEService::processNotificationQueue() {
    if (!queueManager || !deviceConnected) return;
    
    const uint32_t maxProcessingTime = 20; // Max ms to spend in this loop.
    const uint32_t maxEventsPerCycle = 10;
    uint32_t startTime = millis();
    uint32_t eventsProcessed = 0;
    
    std::unique_ptr<DGTEvent> event;
    while (eventsProcessed < maxEventsPerCycle && 
           (millis() - startTime) < maxProcessingTime &&
           (event = queueManager->receiveEvent(0)) != nullptr) {
        
        sendEvent(*event);
        eventsProcessed++;
    }
}

void DGT3000BLEService::processResponseQueue() {
    if (!queueManager || !deviceConnected) return;

    std::unique_ptr<CommandResponse> response = queueManager->receiveResponse(0);
    if (response) {
        logD("Processing response for command ID: %s", response->id);
        
        _responseDoc.clear();
        _responseDoc["type"] = "command_response";
        _responseDoc["id"] = response->id;
        _responseDoc["status"] = response->success ? "success" : "error";

        if (response->success) {
            _responseDoc["result"] = response->result;
        } else {
            JsonObject data = _responseDoc["data"].to<JsonObject>();
            data["errorCode"] = static_cast<uint16_t>(response->errorCode);
            data["errorMessage"] = response->errorMessage;
        }

        String jsonString;
        serializeJson(_responseDoc, jsonString);

        if (sendNotification(jsonString.c_str())) {
            logI("Sent response for command ID: %s", response->id);
        } else {
            logW("Failed to send response for command ID: %s", response->id);
        }
    }
}

bool DGT3000BLEService::sendEvent(const DGTEvent& event) {
    if (!deviceConnected || !eventCharacteristic) return false;
    
    eventBuffer.clear();
    eventBuffer["type"] = getEventTypeString(event.type);
    eventBuffer["timestamp"] = event.timestamp;
    eventBuffer["data"] = event.data;
    
    String jsonString;
    serializeJson(eventBuffer, jsonString);
    
    return sendNotification(jsonString.c_str());
}

bool DGT3000BLEService::sendNotification(const char* jsonData) {
    if (!deviceConnected || !eventCharacteristic) return false;
    
    logD("Sending Notification: %s", jsonData);
    eventCharacteristic->setValue(jsonData);
    eventCharacteristic->notify();
    
    updateNotificationStats(true);
    if (systemStatus) {
        systemStatus->eventsGenerated++;
        systemStatus->updateActivity();
    }
    return true;
}

void DGT3000BLEService::updateStatus() {
    if (!systemStatus) return;
    
    systemStatus->updateUptime();
    systemStatus->freeHeap = ESP.getFreeHeap() / 1024; // In KB
    
    // Read the internal ESP32 temperature sensor.
    float temp_celsius = 0;
    if (temp_sensor_read_celsius(&temp_celsius) == ESP_OK) {
        systemStatus->temperature = (int16_t)temp_celsius;
    } else {
        systemStatus->temperature = -999; // Error indicator
    }
    
    systemStatus->bleConnectionState = deviceConnected ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
}

void DGT3000BLEService::updateStatusCache() {
    if (!systemStatus) return;
    
    updateStatus(); // Ensure status data is fresh.
    
    JsonDocument statusDoc;
    statusDoc["systemState"] = getSystemStateString(systemStatus->systemState);
    statusDoc["bleConnected"] = deviceConnected;
    statusDoc["dgtConnected"] = (systemStatus->dgtConnectionState == ConnectionState::CONNECTED);
    statusDoc["dgtConfigured"] = systemStatus->dgtConfigured;
    statusDoc["uptime"] = systemStatus->uptime;
    statusDoc["freeHeap"] = systemStatus->freeHeap;
    statusDoc["temperature"] = systemStatus->temperature;
    statusDoc["commandsProcessed"] = systemStatus->commandsProcessed;
    statusDoc["eventsGenerated"] = systemStatus->eventsGenerated;
    statusDoc["notificationsSent"] = _notificationStats.notificationsSent;
    statusDoc["notificationsFailed"] = _notificationStats.notificationsFailed;
    
    if (queueManager) {
        statusDoc["rawCmdQueueDepth"] = queueManager->getRawCommandQueueDepth();
        statusDoc["evtQueueDepth"] = queueManager->getEventQueueDepth();
        statusDoc["respQueueDepth"] = queueManager->getResponseQueueDepth();
        statusDoc["queuesHealthy"] = queueManager->isHealthy();
    }
    
    String statusJson;
    serializeJson(statusDoc, statusJson);
    m_cachedStatusJson = statusJson.c_str();
    
    logD("Status cache updated (%d bytes)", statusJson.length());
}

void DGT3000BLEService::updateNotificationStats(bool success) {
    if (success) {
        _notificationStats.notificationsSent++;
        _notificationStats.lastNotificationTime = millis();
    } else {
        _notificationStats.notificationsFailed++;
    }
}

// --- Callback Handlers ---

void DGT3000BLEService::handleConnect() {
    deviceConnected = true;
    logI("BLE Client connected");
    // Forward the connection event to the I2C task manager.
    extern void onBLEConnected();
    onBLEConnected();
}

void DGT3000BLEService::handleDisconnect() {
    deviceConnected = false;
    logI("BLE Client disconnected");
    // Forward the disconnection event to the I2C task manager.
    extern void onBLEDisconnected();
    onBLEDisconnected();
}

void DGT3000BLEService::handleClientSubscription() {
    if (!queueManager || !systemStatus) return;

    // When a client subscribes, immediately send the current DGT connection status.
    auto event = std::unique_ptr<DGTEvent>(new DGTEvent(DGTEvent::CONNECTION_STATUS));
    event->data["connected"] = (systemStatus->dgtConnectionState == ConnectionState::CONNECTED);
    event->data["configured"] = systemStatus->dgtConfigured;
    event->priority = 1;

    logI("Client subscribed to events. Queueing initial connection status.");
    queueManager->sendEvent(std::move(event), 100);
}

void DGT3000BLEService::handleEventRead(BLECharacteristic* characteristic) {
    logI("Event characteristic was read by client (not typical).");
}

String generateErrorResponse(const char* commandId, SystemErrorCode errorCode, const char* message) {
    JsonDocument response;
    response["id"] = commandId;
    response["status"] = "error";
    response["errorCode"] = static_cast<uint16_t>(errorCode);
    response["error"] = message ? message : getErrorCodeString(errorCode);
    
    String jsonString;
    serializeJson(response, jsonString);
    return jsonString;
}
