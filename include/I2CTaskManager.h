/*
 * I2C Task Manager for DGT3000 Gateway
 *
 * This header defines the I2C core task (Core 0) that handles
 * DGT3000 communication and lifecycle management.
 * 
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef I2C_TASK_MANAGER_H
#define I2C_TASK_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "BLEGatewayTypes.h"
#include "QueueManager.h"
#include "DGT3000.h"
#include "00-GatewayConstants.h"
#include <logging.hpp>

/**
 * @class I2CTaskManager
 * @brief Manages the DGT3000 clock communication and lifecycle in a dedicated FreeRTOS task.
 *
 * This class is responsible for:
 * - Running a dedicated task on Core 0 for all I2C communication.
 * - Initializing, configuring, and cleaning up the DGT3000 connection.
 * - Processing commands received from the BLE service.
 * - Generating events (button presses, time updates) and sending them to the BLE service.
 * - Monitoring the connection status and handling automatic recovery.
 */
class I2CTaskManager : public esp32m::SimpleLoggable {
public:
    /**
     * @brief Constructs a new I2CTaskManager.
     * @param queueMgr Pointer to the QueueManager for inter-task communication.
     * @param status Pointer to the global SystemStatus object.
     */
    I2CTaskManager(QueueManager* queueMgr, SystemStatus* status);
    
    /**
     * @brief Destroys the I2CTaskManager object and cleans up resources.
     */
    ~I2CTaskManager();
    
    /**
     * @brief Initializes the manager and its dependencies.
     * @return true if initialization is successful, false otherwise.
     */
    bool initialize();
    
    /**
     * @brief Cleans up all resources, including stopping the task and shutting down the DGT3000.
     */
    void cleanup();
    
    /**
     * @brief Checks if the manager has been initialized.
     * @return true if initialized, false otherwise.
     */
    bool isInitialized() const;
    
    /**
     * @brief Starts the dedicated I2C task.
     * @return true if the task was started successfully, false otherwise.
     */
    bool startTask();
    
    /**
     * @brief Stops the dedicated I2C task.
     */
    void stopTask();
    
    /**
     * @brief Checks if the I2C task is currently running.
     * @return true if the task is running, false otherwise.
     */
    bool isTaskRunning() const;
    
    /**
     * @brief Initializes the DGT3000 connection and configuration.
     * @return true on success, false on failure.
     */
    bool initializeDGT3000();
    
    /**
     * @brief Cleans up the DGT3000 connection and powers it off.
     */
    void cleanupDGT3000();
    
    /**
     * @brief Checks if the DGT3000 is connected.
     * @return true if connected, false otherwise.
     */
    bool isDGT3000Connected() const;
    
    /**
     * @brief Checks if the DGT3000 is configured for central control.
     * @return true if configured, false otherwise.
     */
    bool isDGT3000Configured() const;
    
    /**
     * @brief Callback for when a BLE client connects. Triggers DGT3000 initialization.
     */
    void onBLEConnected();
    
    /**
     * @brief Callback for when a BLE client disconnects. Triggers DGT3000 cleanup.
     */
    void onBLEDisconnected();
    
    /**
     * @brief Attempts to recover a lost DGT3000 connection.
     * @return true if recovery was successful, false otherwise.
     */
    bool attemptRecovery();
    
    /**
     * @brief Resets the recovery attempt counter.
     */
    void resetRecoveryState();
    
    /**
     * @brief Gets the current task statistics.
     * @return A const reference to the I2CTaskStats object.
     */
    const I2CTaskStats& getStatistics() const;
    
    /**
     * @brief Resets all task statistics to zero.
     */
    void resetStatistics();
    
    /**
     * @brief Prints the current status of the I2C task and DGT connection to the log.
     */
    void printStatus();
    
    /**
     * @brief Prints the current task statistics to the log.
     */
    void printStatistics();
    
    /**
     * @brief Gets the string name for a given button code.
     * @param buttonCode The button code from a DGT event.
     * @return A const char* with the button name.
     */
    const char* getButtonName(uint8_t buttonCode) const;
    
private:
    TaskHandle_t _taskHandle; ///< Handle for the FreeRTOS task.
    
    // Dependencies
    QueueManager* _queueManager; ///< Manages queues for commands and events.
    SystemStatus* _systemStatus; ///< Global system status object.
    std::unique_ptr<DGT3000> _dgt3000; ///< DGT3000 driver instance.
    
    // State Management
    I2CTaskState _taskState; ///< Current state of the I2C task.
    ConnectionState _dgtConnectionState; ///< Connection state of the DGT3000.
    bool _dgtConfigured; ///< Flag indicating if the DGT3000 is configured.
    bool _bleConnected; ///< Flag indicating if a BLE client is connected.
    
    // Timing and Recovery
    uint32_t _lastUpdateTime; ///< Timestamp of the last task loop.
    uint32_t _lastRecoveryAttempt; ///< Timestamp of the last recovery attempt.
    uint8_t _recoveryAttempts; ///< Counter for recovery attempts.
    uint32_t _connectionStartTime; ///< Timestamp when the DGT connection was initiated.
    
    // Statistics
    I2CTaskStats _stats; ///< Task statistics.
    
    // Time Monitoring
    struct {
        uint8_t lastTime[6];
        bool timeValid;
        uint32_t lastTimeUpdate;
        uint32_t timeUpdateCount;
    } _timeMonitoring;
    
    // Button Monitoring
    struct {
        uint8_t lastButtonState;
        uint32_t lastButtonTime;
        uint32_t buttonRepeatCount;
        bool buttonRepeatActive;
    } _buttonMonitoring;
    
    // Synchronization
    SemaphoreHandle_t _stateMutex; ///< Mutex for thread-safe state access.
    bool _initializingDGT; ///< Flag to prevent concurrent initialization attempts.
    int _lastDGTUpdateResult; ///< Stores the result of the last DGT3000 operation.

    // Reusable JSON documents to reduce stack allocation
    JsonDocument _commandParamsDoc;
    JsonDocument _responseResultDoc;
    
    // Task Implementation
    static void taskFunction(void* parameter);
    void runTask();
    
    // Core Task Operations
    void processCommand();
    void handleEvents();
    void monitorConnection();
    
    // Command Processing
    bool executeCommand(const char* id, const char* commandName, const JsonObjectConst& params);
    bool executeSetTime(const char* id, const JsonObjectConst& params);
    bool executeDisplayText(const char* id, const JsonObjectConst& params);
    bool executeEndDisplay(const char* id);
    bool executeStop(const char* id);
    bool executeRun(const char* id, const JsonObjectConst& params);
    bool executeGetTime(const char* id);
    bool executeGetStatus(const char* id);
    
    // Response Handling
    void sendCommandResponse(const char* id, bool success, const JsonObjectConst& result);
    void sendCommandError(const char* id, SystemErrorCode errorCode, const char* message = nullptr);
    
    // Event Generation
    void generateButtonEvent();
    void handleButtonRepeat();
    void generateTimeEvent(const uint8_t time[6]);
    void generateConnectionStatusEvent(bool connected, bool configured);
    void generateErrorEvent(SystemErrorCode errorCode, const char* message);
    
    // DGT3000 Management
    bool configureDGT3000();
    void handleDGT3000Error(int error);
    
    // State Management Helpers
    void setState(I2CTaskState newState);
    void updateConnectionState();
    void updateStatistics();
    
    // Recovery Helpers
    bool shouldAttemptRecovery() const;
    bool performRecovery();
    
    // Timing Helpers
    bool isTimeout(uint32_t startTime, uint32_t timeoutMs) const;
    void delayWithYield(uint32_t ms);
};

// Global instance (consider dependency injection instead of a global)
extern std::unique_ptr<I2CTaskManager> g_i2cTaskManager;

// Utility Functions
const char* getI2CTaskStateString(I2CTaskState state);
SystemErrorCode mapDGTErrorToSystemError(int dgtError);

#endif // I2C_TASK_MANAGER_H
