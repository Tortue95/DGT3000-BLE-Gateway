/*
 * FreeRTOS Queue Management Utilities Implementation
 *
 * This file implements the thread-safe queue operations for inter-task
 * communication within the DGT3000 BLE Gateway.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "QueueManager.h"
#include <esp_heap_caps.h>

using namespace esp32m;

// =============================================================================
// QUEUE MANAGER IMPLEMENTATION
// =============================================================================

QueueManager::QueueManager()
    : SimpleLoggable("queue"), _queueMutex(nullptr), _lastHealthCheck(0), _healthy(false) {
    // Initialize queue handles to null.
    _queues.rawCommandQueue = nullptr;
    _queues.eventQueue = nullptr;
    _queues.responseQueue = nullptr;
}

QueueManager::~QueueManager() {
    cleanup();
}

bool QueueManager::initialize() {
    // Create a mutex for thread-safe access to queue operations.
    _queueMutex = xSemaphoreCreateMutex();
    if (_queueMutex == nullptr) {
        logE("Failed to create queue mutex");
        return false;
    }
    
    // Create the queue for raw commands (BLE -> I2C).
    // The queue stores pointers to dynamically allocated RawBLECommand objects.
    if (!createQueue(_queues.rawCommandQueue, sizeof(RawBLECommand*), QUEUE_COMMAND_SIZE, "RawCommandQueue")) {
        logE("Failed to create raw command queue");
        cleanup();
        return false;
    }
    
    // Create the queue for events (I2C -> BLE).
    // The queue stores pointers to dynamically allocated DGTEvent objects.
    if (!createQueue(_queues.eventQueue, sizeof(DGTEvent*), QUEUE_EVENT_SIZE, "EventQueue")) {
        logE("Failed to create event queue");
        cleanup();
        return false;
    }
    
    // Create the queue for command responses (I2C -> BLE).
    if (!createQueue(_queues.responseQueue, sizeof(CommandResponse*), QUEUE_COMMAND_SIZE, "ResponseQueue")) {
        logE("Failed to create response queue");
        cleanup();
        return false;
    }
    
    resetStatistics();
    _lastHealthCheck = millis();
    _healthy = true;
    
    logI("Queue Manager initialized successfully");
    return true;
}

void QueueManager::cleanup() {
    logI("Cleaning up Queue Manager...");
    
    // Destroying the queues also frees any items still within them.
    flushAllQueues(); // Ensure dynamic memory is freed before deleting queues.
    destroyQueue(_queues.rawCommandQueue);
    destroyQueue(_queues.eventQueue);
    destroyQueue(_queues.responseQueue);
    
    if (_queueMutex != nullptr) {
        vSemaphoreDelete(_queueMutex);
        _queueMutex = nullptr;
    }
    
    _healthy = false;
    logI("Queue Manager cleanup complete");
}

bool QueueManager::isInitialized() const {
    return _queues.isInitialized() && (_queueMutex != nullptr);
}

// =============================================================================
// RAW COMMAND QUEUE OPERATIONS
// =============================================================================

bool QueueManager::sendRawCommand(std::unique_ptr<RawBLECommand> rawData, uint32_t timeoutMs) {
    if (!isInitialized() || !rawData) return false;
    
    RawBLECommand* rawPtr = rawData.release(); // Release ownership to a raw pointer.
    if (sendToQueueSafe(_queues.rawCommandQueue, &rawPtr, sizeof(RawBLECommand*), timeoutMs)) {
        logD("Raw command sent to queue (len: %d)", rawPtr->length);
        return true;
    } else {
        logW("Failed to send raw command to queue (len: %d), deleting command.", rawPtr->length);
        delete rawPtr; // Prevent memory leak if sending fails.
        return false;
    }
}

std::unique_ptr<RawBLECommand> QueueManager::receiveRawCommand(uint32_t timeoutMs) {
    if (!isInitialized()) return nullptr;
    
    RawBLECommand* rawPtr = nullptr;
    if (receiveFromQueueSafe(_queues.rawCommandQueue, &rawPtr, sizeof(RawBLECommand*), timeoutMs)) {
        logD("Raw command received from queue (len: %d)", rawPtr->length);
        return std::unique_ptr<RawBLECommand>(rawPtr); // Re-wrap in unique_ptr to manage lifetime.
    }
    
    return nullptr;
}

uint16_t QueueManager::getRawCommandQueueDepth() const {
    if (!isInitialized()) return 0;
    return uxQueueMessagesWaiting(_queues.rawCommandQueue);
}

uint16_t QueueManager::getRawCommandQueueFreeSpace() const {
    if (!isInitialized()) return 0;
    return uxQueueSpacesAvailable(_queues.rawCommandQueue);
}

bool QueueManager::isRawCommandQueueFull() const {
    return getRawCommandQueueFreeSpace() == 0;
}

bool QueueManager::isRawCommandQueueEmpty() const {
    return getRawCommandQueueDepth() == 0;
}

// =============================================================================
// EVENT QUEUE OPERATIONS
// =============================================================================

bool QueueManager::sendEvent(std::unique_ptr<DGTEvent> event, uint32_t timeoutMs) {
    if (!isInitialized() || !event) return false;

    DGTEvent* rawPtr = event.release();
    bool success = sendToQueueSafe(_queues.eventQueue, &rawPtr, sizeof(DGTEvent*), timeoutMs);
    updateQueueStats(true, success, false);

    if (success) {
        logD("Event sent: %s", getEventTypeString(rawPtr->type));
    } else {
        logW("Failed to send event, deleting: %s", getEventTypeString(rawPtr->type));
        delete rawPtr;
    }
    return success;
}

std::unique_ptr<DGTEvent> QueueManager::receiveEvent(uint32_t timeoutMs) {
    if (!isInitialized()) return nullptr;

    DGTEvent* rawPtr = nullptr;
    bool success = receiveFromQueueSafe(_queues.eventQueue, &rawPtr, sizeof(DGTEvent*), timeoutMs);
    updateQueueStats(false, success, false);

    if (success) {
        logD("Event received: %s", getEventTypeString(rawPtr->type));
        return std::unique_ptr<DGTEvent>(rawPtr);
    }
    return nullptr;
}

uint16_t QueueManager::getEventQueueDepth() const {
    if (!isInitialized()) return 0;
    return uxQueueMessagesWaiting(_queues.eventQueue);
}

uint16_t QueueManager::getEventQueueFreeSpace() const {
    if (!isInitialized()) return 0;
    return uxQueueSpacesAvailable(_queues.eventQueue);
}

bool QueueManager::isEventQueueFull() const {
    return getEventQueueFreeSpace() == 0;
}

bool QueueManager::isEventQueueEmpty() const {
    return getEventQueueDepth() == 0;
}

// =============================================================================
// RESPONSE QUEUE OPERATIONS
// =============================================================================

bool QueueManager::sendResponse(std::unique_ptr<CommandResponse> response, uint32_t timeoutMs) {
    if (!isInitialized() || !response) return false;

    CommandResponse* rawPtr = response.release();
    bool success = sendToQueueSafe(_queues.responseQueue, &rawPtr, sizeof(CommandResponse*), timeoutMs);
    
    if (success) {
        logD("Response sent for ID: %s", rawPtr->id);
    } else {
        logW("Failed to send response, deleting for ID: %s", rawPtr->id);
        delete rawPtr;
    }
    return success;
}

std::unique_ptr<CommandResponse> QueueManager::receiveResponse(uint32_t timeoutMs) {
    if (!isInitialized()) return nullptr;

    CommandResponse* rawPtr = nullptr;
    bool success = receiveFromQueueSafe(_queues.responseQueue, &rawPtr, sizeof(CommandResponse*), timeoutMs);
    
    if (success) {
        logD("Response received for ID: %s", rawPtr->id);
        return std::unique_ptr<CommandResponse>(rawPtr);
    }
    return nullptr;
}

uint16_t QueueManager::getResponseQueueDepth() const {
    if (!isInitialized()) return 0;
    return uxQueueMessagesWaiting(_queues.responseQueue);
}

uint16_t QueueManager::getResponseQueueFreeSpace() const {
    if (!isInitialized()) return 0;
    return uxQueueSpacesAvailable(_queues.responseQueue);
}

bool QueueManager::isResponseQueueFull() const {
    return getResponseQueueFreeSpace() == 0;
}

bool QueueManager::isResponseQueueEmpty() const {
    return getResponseQueueDepth() == 0;
}

// =============================================================================
// PRIORITY OPERATIONS
// =============================================================================

bool QueueManager::sendPriorityEvent(std::unique_ptr<DGTEvent> event, uint32_t timeoutMs) {
    if (!isInitialized() || !event) return false;
    
    DGTEvent* rawPtr = event.release();
    if (xQueueSendToFront(_queues.eventQueue, &rawPtr, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        logI("Priority event queued: %s", getEventTypeString(rawPtr->type));
        updateQueueStats(true, true, false);
        return true;
    }
    
    logW("Failed to queue priority event, deleting: %s", getEventTypeString(rawPtr->type));
    delete rawPtr;
    updateQueueStats(true, false, false);
    return false;
}

// =============================================================================
// STATISTICS AND MONITORING
// =============================================================================

const QueueStats& QueueManager::getStatistics() const {
    return _stats;
}

void QueueManager::resetStatistics() {
    _stats = QueueStats();
    logD("Queue statistics reset");
}

void QueueManager::updateStatistics() {
    if (!isInitialized()) return;
    
    uint16_t evtDepth = getEventQueueDepth();
    if (evtDepth > _stats.maxEventQueueDepth) {
        _stats.maxEventQueueDepth = evtDepth;
    }
}

bool QueueManager::isHealthy() {
    if (!isInitialized()) return false;
    
    if (millis() - _lastHealthCheck < HEALTH_CHECK_INTERVAL_MS) {
        return _healthy; // Return cached health status.
    }
    
    _lastHealthCheck = millis();
    
    float rawCmdUtil = getRawCommandQueueUtilization();
    float evtUtil = getEventQueueUtilization();
    float respUtil = getResponseQueueUtilization();
    
    _healthy = (rawCmdUtil < QUEUE_HEALTH_THRESHOLD) &&
               (evtUtil < QUEUE_HEALTH_THRESHOLD) && 
               (respUtil < QUEUE_HEALTH_THRESHOLD);
    
    if (!_healthy) {
        logW("Queue health check failed - Utilization: RawCmd=%.1f%%, Evt=%.1f%%, Resp=%.1f%%", 
             rawCmdUtil * 100, evtUtil * 100, respUtil * 100);
    }
    
    return _healthy;
}

float QueueManager::getRawCommandQueueUtilization() const {
    if (!isInitialized()) return 0.0f;
    return (float)getRawCommandQueueDepth() / QUEUE_COMMAND_SIZE;
}

float QueueManager::getEventQueueUtilization() const {
    if (!isInitialized()) return 0.0f;
    return (float)getEventQueueDepth() / QUEUE_EVENT_SIZE;
}

float QueueManager::getResponseQueueUtilization() const {
    if (!isInitialized()) return 0.0f;
    return (float)getResponseQueueDepth() / QUEUE_COMMAND_SIZE;
}

// =============================================================================
// EMERGENCY OPERATIONS
// =============================================================================

void QueueManager::flushAllQueues() {
    logW("Flushing all queues...");
    flushRawCommandQueue();
    flushEventQueue();
    flushResponseQueue();
}

void QueueManager::flushRawCommandQueue() {
    if (!isInitialized()) return;
    
    std::unique_ptr<RawBLECommand> rawCmd;
    while ((rawCmd = receiveRawCommand(0)) != nullptr) {
        // unique_ptr automatically deletes the object.
    }
    logW("Raw command queue flushed.");
}

void QueueManager::flushEventQueue() {
    if (!isInitialized()) return;
    
    std::unique_ptr<DGTEvent> event;
    while ((event = receiveEvent(0)) != nullptr) {
        // unique_ptr automatically deletes the object.
    }
    logW("Event queue flushed.");
}

void QueueManager::flushResponseQueue() {
    if (!isInitialized()) return;
    
    std::unique_ptr<CommandResponse> response;
    while ((response = receiveResponse(0)) != nullptr) {
        // unique_ptr automatically deletes the object.
    }
    logW("Response queue flushed.");
}

// =============================================================================
// DEBUG AND DIAGNOSTICS
// =============================================================================

void QueueManager::printQueueStatus() {
    if (!isInitialized()) {
        logW("Queue Manager not initialized");
        return;
    }
    
    logI("--- Queue Status ---");
    logI("Raw Command: %d/%d (%.1f%%)", getRawCommandQueueDepth(), QUEUE_COMMAND_SIZE, getRawCommandQueueUtilization() * 100);
    logI("Event: %d/%d (%.1f%%)", getEventQueueDepth(), QUEUE_EVENT_SIZE, getEventQueueUtilization() * 100);
    logI("Response: %d/%d (%.1f%%)", getResponseQueueDepth(), QUEUE_COMMAND_SIZE, getResponseQueueUtilization() * 100);
    logI("Health: %s", _healthy ? "HEALTHY" : "UNHEALTHY");
}

void QueueManager::printStatistics() {
    logI("--- Queue Statistics ---");
    logI("Events: Queued=%lu, Processed=%lu", _stats.eventsQueued, _stats.eventsProcessed);
    logI("Errors: Overflows=%lu, Timeouts=%lu", _stats.queueOverflows, _stats.queueTimeouts);
    logI("Max Event Queue Depth: %d", _stats.maxEventQueueDepth);
}

// =============================================================================
// PRIVATE HELPER METHODS
// =============================================================================

bool QueueManager::createQueue(QueueHandle_t& queue, size_t itemSize, size_t queueLength, const char* name) {
    queue = xQueueCreate(queueLength, itemSize);
    if (queue == nullptr) {
        logE("Failed to create queue: %s", name);
        return false;
    }
    logD("Created queue: %s (%d items, %d bytes each)", name, queueLength, itemSize);
    return true;
}

void QueueManager::destroyQueue(QueueHandle_t& queue) {
    if (queue != nullptr) {
        vQueueDelete(queue);
        queue = nullptr;
    }
}

bool QueueManager::sendToQueueSafe(QueueHandle_t queue, const void* item, size_t itemSize, uint32_t timeoutMs) {
    if (queue == nullptr) return false;
    return (xQueueSend(queue, item, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}

bool QueueManager::receiveFromQueueSafe(QueueHandle_t queue, void* item, size_t itemSize, uint32_t timeoutMs) {
    if (queue == nullptr) return false;
    return (xQueueReceive(queue, item, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}

void QueueManager::updateQueueStats(bool isSend, bool success, bool isCommand) {
    // This logic is simplified as raw command stats are not tracked here.
    if (isSend) {
        if (!isCommand) _stats.eventsQueued++;
        if (!success) _stats.queueOverflows++;
    } else {
        if (!isCommand && success) _stats.eventsProcessed++;
        if (!success) _stats.queueTimeouts++;
    }
    updateStatistics();
}
