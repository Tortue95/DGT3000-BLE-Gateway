/*
 * BLE Service Callbacks Implementation for DGT3000 Gateway
 *
 * This file implements the callback handlers for BLE events, such as
 * client connections, disconnections, and characteristic writes.
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
#include "BLEGatewayTypes.h"
#include "QueueManager.h"
#include <logging.hpp>

using namespace esp32m;

// =============================================================================
// DGT3000ServerCallbacks Implementation
// =============================================================================

void DGT3000ServerCallbacks::onConnect(BLEServer* server) {
    if (m_service) {
        m_service->handleConnect();
    }
}

void DGT3000ServerCallbacks::onDisconnect(BLEServer* server) {
    if (m_service) {
        m_service->handleDisconnect();
        // Restart advertising to allow a new client to connect.
        log_i("Client disconnected, restarting advertising...");
        m_service->bleServer->startAdvertising();
    }
}

// =============================================================================
// DGT3000CommandCallbacks Implementation
// =============================================================================

void DGT3000CommandCallbacks::onWrite(BLECharacteristic* characteristic) {
    std::string value = characteristic->getValue();
    if (value.length() == 0 || value.length() >= JSON_COMMAND_BUFFER_SIZE) {
        log_w("Received invalid command length: %d", value.length());
        return;
    }
    
    // Basic validation to check if the payload looks like a JSON object.
    if (value[0] != '{' || value[value.length()-1] != '}') {
        log_w("Received non-JSON command: %s", value.c_str());
        return;
    }
    
    // Allocate a command object on the heap and wrap it in a unique_ptr.
    auto rawCmd = std::unique_ptr<RawBLECommand>(new RawBLECommand());
    if (!rawCmd) {
        log_e("Failed to allocate memory for RawBLECommand");
        return;
    }
    
    rawCmd->timestamp = millis();
    rawCmd->length = value.length();
    strncpy(rawCmd->jsonData, value.c_str(), sizeof(rawCmd->jsonData) - 1);
    rawCmd->jsonData[sizeof(rawCmd->jsonData) - 1] = '\0';
    
    // Send the command to the processing queue. The QueueManager takes ownership.
    if (m_service && m_service->queueManager) {
        if (!m_service->queueManager->sendRawCommand(std::move(rawCmd), 10)) { // 10ms timeout
            log_e("Failed to send raw command to queue.");
        }
    } else {
        log_e("QueueManager not available; command dropped.");
    }
}

// =============================================================================
// DGT3000EventCallbacks Implementation
// =============================================================================

void DGT3000EventCallbacks::onRead(BLECharacteristic* characteristic) {
    if (m_service) {
        m_service->handleEventRead(characteristic);
    }
}

// =============================================================================
// DGT3000StatusCallbacks Implementation
// =============================================================================

void DGT3000StatusCallbacks::onRead(BLECharacteristic* characteristic) {
    if (m_service) {
        // Ensure the status cache is fresh before a client reads it.
        m_service->updateStatusCache(); 
        characteristic->setValue(m_service->getCachedStatusJson());
    }
}

// =============================================================================
// DGT3000EventDescriptorCallbacks Implementation
// =============================================================================

/**
 * @brief Called when a client writes to the event characteristic's CCCD.
 * This handles subscription to notifications.
 * @param pDescriptor Pointer to the descriptor that was written to.
 */
void DGT3000EventDescriptorCallbacks::onWrite(BLEDescriptor* pDescriptor) {
    uint8_t* value = pDescriptor->getValue();
    // Check if the client is enabling notifications (0x01, 0x00).
    if (pDescriptor->getLength() >= 2 && value[0] == 0x01 && value[1] == 0x00) {
        log_i("Client subscribed to event notifications.");
        if (m_service) {
            m_service->handleClientSubscription();
        }
    } else {
        log_i("Client unsubscribed from event notifications.");
    }
}
