/*
 * FreeRTOS Queue Management Utilities
 *
 * This header provides a thread-safe manager for inter-task communication
 * using FreeRTOS queues, specifically for the DGT3000 BLE Gateway.
 * It handles the lifecycle and operations for command, event, and response queues.
 * 
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "BLEGatewayTypes.h"
#include "00-GatewayConstants.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <logging.hpp>

/**
 * @class QueueManager
 * @brief Manages thread-safe FreeRTOS queues for inter-task communication.
 *
 * This class abstracts the creation, deletion, and operation of queues used
 * to pass data between the BLE task and the I2C task. It manages queues for:
 * - Raw Commands (BLE -> I2C)
 * - Events (I2C -> BLE)
 * - Command Responses (I2C -> BLE)
 */
class QueueManager : public esp32m::SimpleLoggable {
public:
    /**
     * @brief Constructs a new QueueManager object.
     */
    QueueManager();
    
    /**
     * @brief Destroys the QueueManager and cleans up all resources.
     */
    ~QueueManager();
    
    /**
     * @brief Initializes the queues and synchronization primitives.
     * @return true if all queues are created successfully, false otherwise.
     */
    bool initialize();
    
    /**
     * @brief Cleans up and destroys all queues and synchronization primitives.
     */
    void cleanup();
    
    /**
     * @brief Checks if the QueueManager has been successfully initialized.
     * @return true if initialized, false otherwise.
     */
    bool isInitialized() const;
    
    // --- Raw Command Queue (BLE -> I2C) ---

    /**
     * @brief Sends a raw command to the I2C task.
     * @param rawData A unique_ptr to the RawBLECommand to send.
     * @param timeoutMs Timeout in milliseconds to wait for space in the queue.
     * @return true if the command was sent successfully, false on timeout or error.
     */
    bool sendRawCommand(std::unique_ptr<RawBLECommand> rawData, uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    /**
     * @brief Receives a raw command from the queue.
     * @param timeoutMs Timeout in milliseconds to wait for a command.
     * @return A unique_ptr to the received RawBLECommand, or nullptr on timeout or error.
     */
    std::unique_ptr<RawBLECommand> receiveRawCommand(uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    uint16_t getRawCommandQueueDepth() const;
    uint16_t getRawCommandQueueFreeSpace() const;
    bool isRawCommandQueueFull() const;
    bool isRawCommandQueueEmpty() const;

    // --- Event Queue (I2C -> BLE) ---

    /**
     * @brief Sends a DGT event to the BLE task.
     * @param event A unique_ptr to the DGTEvent to send.
     * @param timeoutMs Timeout in milliseconds to wait for space in the queue.
     * @return true if the event was sent successfully, false on timeout or error.
     */
    bool sendEvent(std::unique_ptr<DGTEvent> event, uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    /**
     * @brief Receives a DGT event from the queue.
     * @param timeoutMs Timeout in milliseconds to wait for an event.
     * @return A unique_ptr to the received DGTEvent, or nullptr on timeout or error.
     */
    std::unique_ptr<DGTEvent> receiveEvent(uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    uint16_t getEventQueueDepth() const;
    uint16_t getEventQueueFreeSpace() const;
    bool isEventQueueFull() const;
    bool isEventQueueEmpty() const;
    
    // --- Response Queue (I2C -> BLE) ---

    /**
     * @brief Sends a command response to the BLE task.
     * @param response A unique_ptr to the CommandResponse to send.
     * @param timeoutMs Timeout in milliseconds to wait for space in the queue.
     * @return true if the response was sent successfully, false on timeout or error.
     */
    bool sendResponse(std::unique_ptr<CommandResponse> response, uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    /**
     * @brief Receives a command response from the queue.
     * @param timeoutMs Timeout in milliseconds to wait for a response.
     * @return A unique_ptr to the received CommandResponse, or nullptr on timeout or error.
     */
    std::unique_ptr<CommandResponse> receiveResponse(uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    uint16_t getResponseQueueDepth() const;
    uint16_t getResponseQueueFreeSpace() const;
    bool isResponseQueueFull() const;
    bool isResponseQueueEmpty() const;
    
    /**
     * @brief Sends a high-priority event (e.g., an error) to the front of the event queue.
     * @param event A unique_ptr to the DGTEvent to send.
     * @param timeoutMs Timeout in milliseconds to wait for space in the queue.
     * @return true if the event was sent successfully, false on timeout or error.
     */
    bool sendPriorityEvent(std::unique_ptr<DGTEvent> event, uint32_t timeoutMs = QUEUE_OPERATION_TIMEOUT_MS);
    
    // --- Statistics and Monitoring ---
    
    const QueueStats& getStatistics() const;
    void resetStatistics();
    void updateStatistics();
    
    /**
     * @brief Checks if all queues are operating within healthy utilization thresholds.
     * @return true if queues are healthy, false if any queue is nearing capacity.
     */
    bool isHealthy();
    
    float getRawCommandQueueUtilization() const;
    float getEventQueueUtilization() const;
    float getResponseQueueUtilization() const;
    
    // --- Emergency Operations ---
    
    /**
     * @brief Flushes all items from all queues, freeing allocated memory.
     */
    void flushAllQueues();
    void flushRawCommandQueue();
    void flushEventQueue();
    void flushResponseQueue();
    
    // --- Debug and Diagnostics ---
    
    void printQueueStatus();
    void printStatistics();
    
private:
    QueueHandles _queues; ///< Holds the handles for the FreeRTOS queues.
    SemaphoreHandle_t _queueMutex; ///< Mutex for thread-safe access to queues.
    QueueStats _stats; ///< Statistics for queue operations.
    
    // Health monitoring
    uint32_t _lastHealthCheck;
    bool _healthy;
    
    // Internal helper methods
    bool createQueue(QueueHandle_t& queue, size_t itemSize, size_t queueLength, const char* name);
    void destroyQueue(QueueHandle_t& queue);
    bool sendToQueueSafe(QueueHandle_t queue, const void* item, size_t itemSize, uint32_t timeoutMs);
    bool receiveFromQueueSafe(QueueHandle_t queue, void* item, size_t itemSize, uint32_t timeoutMs);
    void updateQueueStats(bool isSend, bool success, bool isCommand);
    
    // Constants for health monitoring
    static constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;
    static constexpr float QUEUE_HEALTH_THRESHOLD = 0.8f; // 80% utilization
};

// Global queue manager instance
extern std::unique_ptr<QueueManager> g_queueManager;

#endif // QUEUE_MANAGER_H
