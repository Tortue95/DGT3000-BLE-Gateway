/*
 * I2C Task Manager Implementation for DGT3000 Gateway
 *
 * Implementation of the I2C core task (Core 0) that handles
 * DGT3000 communication and lifecycle management.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "I2CTaskManager.h"
#include <esp_task_wdt.h>

using namespace esp32m;

// =============================================================================
// I2C TASK MANAGER IMPLEMENTATION
// =============================================================================

I2CTaskManager::I2CTaskManager(QueueManager* queueMgr, SystemStatus* status)
    : SimpleLoggable("i2c"),
      _taskHandle(nullptr),
      _queueManager(queueMgr),
      _systemStatus(status),
      _taskState(I2CTaskState::IDLE),
      _dgtConnectionState(ConnectionState::DISCONNECTED),
      _dgtConfigured(false),
      _bleConnected(false),
      _lastUpdateTime(0),
      _lastRecoveryAttempt(0),
      _recoveryAttempts(0),
      _connectionStartTime(0),
      _stateMutex(nullptr),
      _initializingDGT(false),
      _lastDGTUpdateResult(DGT_SUCCESS)
{
    // Initialize all state and monitoring structures.
    _stats = I2CTaskStats();
    
    _timeMonitoring.timeValid = false;
    _timeMonitoring.lastTimeUpdate = 0;
    _timeMonitoring.timeUpdateCount = 0;
    memset(_timeMonitoring.lastTime, 0, sizeof(_timeMonitoring.lastTime));
    
    _buttonMonitoring.lastButtonState = 0;
    _buttonMonitoring.lastButtonTime = 0;
    _buttonMonitoring.buttonRepeatCount = 0;
    _buttonMonitoring.buttonRepeatActive = false;
}

I2CTaskManager::~I2CTaskManager() {
    stopTask();
    if (_stateMutex != nullptr) {
        vSemaphoreDelete(_stateMutex);
    }
    _dgt3000.reset();
}

bool I2CTaskManager::initialize() {
    // Create a mutex for thread-safe access to shared state variables.
    _stateMutex = xSemaphoreCreateMutex();
    if (_stateMutex == nullptr) {
        logE("Failed to create state mutex");
        return false;
    }
    
    // Create the DGT3000 driver instance, but defer hardware initialization.
    _dgt3000 = std::unique_ptr<DGT3000>(new DGT3000());
    if (!_dgt3000) {
        logE("Failed to create DGT3000 instance");
        return false;
    }
    
    resetStatistics();
    setState(I2CTaskState::INITIALIZED);
    logI("I2C Task Manager initialized");
    return true;
}

void I2CTaskManager::cleanup() {
    logI("Cleaning up I2C Task Manager...");
    stopTask();
    cleanupDGT3000();
    _dgt3000.reset();

    if (_stateMutex != nullptr) {
        vSemaphoreDelete(_stateMutex);
        _stateMutex = nullptr;
    }

    setState(I2CTaskState::IDLE);
    logI("I2C Task Manager cleanup complete");
}

bool I2CTaskManager::isInitialized() const {
    return (_dgt3000 != nullptr) && (_stateMutex != nullptr) && (_taskState != I2CTaskState::IDLE);
}

// =============================================================================
// TASK CONTROL
// =============================================================================

bool I2CTaskManager::startTask() {
    if (!isInitialized()) {
        logW("Cannot start task: manager not initialized");
        return false;
    }
    if (_taskHandle != nullptr) {
        logW("Task already running");
        return true;
    }
    
    // Create the dedicated FreeRTOS task pinned to Core 0.
    BaseType_t result = xTaskCreatePinnedToCore(
        taskFunction,           // Task function entry point
        "I2CTask",              // Task name
        I2C_TASK_STACK_SIZE,    // Stack size
        this,                   // Pass this object as the task parameter
        I2C_TASK_PRIORITY,      // Task priority
        &_taskHandle,           // Task handle output
        I2C_TASK_CORE           // Pin to Core 0
    );
    
    if (result != pdPASS) {
        logE("Failed to create I2C Task");
        return false;
    }
    
    setState(I2CTaskState::RUNNING);
    logI("I2C Task started");
    return true;
}

void I2CTaskManager::stopTask() {
    if (_taskHandle == nullptr) {
        return;
    }
    
    logI("Stopping I2C Task...");
    setState(I2CTaskState::STOPPING);
    
    // This will trigger the task loop to exit, and the task will delete itself.
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
    
    setState(I2CTaskState::INITIALIZED);
    logI("I2C Task stopped");
}

bool I2CTaskManager::isTaskRunning() const {
    return (_taskHandle != nullptr) && (_taskState == I2CTaskState::RUNNING);
}

// =============================================================================
// DGT3000 LIFECYCLE MANAGEMENT
// =============================================================================

bool I2CTaskManager::initializeDGT3000() {
    _initializingDGT = true;
    if (!_dgt3000) {
        logE("DGT3000 instance not available");
        _initializingDGT = false;
        return false;
    }
    
    logI("Initializing DGT3000 connection...");
    _connectionStartTime = millis();
    
    // Step 1: Initialize the DGT3000 hardware (I2C buses).
    if (!_dgt3000->begin(DGT3000_MASTER_SDA_PIN, DGT3000_MASTER_SCL_PIN, DGT3000_SLAVE_SDA_PIN, DGT3000_SLAVE_SCL_PIN)) {
        logE("Failed to initialize DGT3000 hardware");
        _initializingDGT = false;
        return false;
    }
    delay(100); // Small delay for stability after hardware init.

    // Step 2: Perform the configuration handshake.
    if (!configureDGT3000()) {
        logE("Failed to configure DGT3000");
        _initializingDGT = false;
        return false;
    }
    
    _dgtConnectionState = ConnectionState::CONNECTED;
    _dgtConfigured = true;
    
    if (_systemStatus) {
        _systemStatus->dgtConnectionState = _dgtConnectionState;
        _systemStatus->dgtConfigured = _dgtConfigured;
        _systemStatus->updateActivity();
    }
    
    generateConnectionStatusEvent(true, true);
    logI("DGT3000 initialized successfully");
    _initializingDGT = false;
    return true;
}

void I2CTaskManager::cleanupDGT3000() {
    if (!_dgt3000 || !isDGT3000Connected()) {
        return;
    }
    
    logI("Cleaning up DGT3000 connection...");
    
    _dgt3000->end(); // This sends the power-off command.
    
    _dgtConnectionState = ConnectionState::DISCONNECTED;
    _dgtConfigured = false;
    
    if (_systemStatus) {
        _systemStatus->dgtConnectionState = _dgtConnectionState;
        _systemStatus->dgtConfigured = _dgtConfigured;
        _systemStatus->updateActivity();
    }
    
    generateConnectionStatusEvent(false, false);
    logI("DGT3000 cleanup complete");
}

bool I2CTaskManager::isDGT3000Connected() const {
    return _dgtConnectionState == ConnectionState::CONNECTED;
}

bool I2CTaskManager::isDGT3000Configured() const {
    return _dgtConfigured;
}

// =============================================================================
// CONNECTION STATE MANAGEMENT
// =============================================================================

void I2CTaskManager::onBLEConnected() {
    logI("BLE connected, initializing DGT3000...");
    _bleConnected = true;
    
    if (!initializeDGT3000()) {
        logE("Failed to initialize DGT3000 on BLE connection");
        generateErrorEvent(SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to initialize DGT3000");
    }
}

void I2CTaskManager::onBLEDisconnected() {
    logI("BLE disconnected, cleaning up DGT3000...");
    _bleConnected = false;
    cleanupDGT3000();
}

// =============================================================================
// TASK IMPLEMENTATION
// =============================================================================

void I2CTaskManager::taskFunction(void* parameter) {
    I2CTaskManager* manager = static_cast<I2CTaskManager*>(parameter);
    if (manager) {
        manager->runTask();
    }
    vTaskDelete(nullptr);
}

void I2CTaskManager::runTask() {
    logI("I2C Task started on Core %d", xPortGetCoreID());
    esp_task_wdt_add(nullptr);
    _lastUpdateTime = millis();
    
    while (_taskState == I2CTaskState::RUNNING) {
        uint32_t loopStart = millis();
        esp_task_wdt_reset();
        
        // Main task loop operations
        processCommand();
        if (isDGT3000Connected()) {
            handleEvents();
        }
        monitorConnection();
        updateStatistics();
        
        // Maintain a consistent update frequency.
        uint32_t elapsed = millis() - loopStart;
        if (elapsed < I2C_TASK_UPDATE_INTERVAL_MS) {
            delayWithYield(I2C_TASK_UPDATE_INTERVAL_MS - elapsed);
        }
    }
    
    esp_task_wdt_delete(nullptr);
    logI("I2C Task finished");
}

// =============================================================================
// CORE TASK OPERATIONS
// =============================================================================

void I2CTaskManager::processCommand() {
    if (!_queueManager) return;

    std::unique_ptr<RawBLECommand> rawCmd;

    // Process a single command from the queue.
    if ((rawCmd = _queueManager->receiveRawCommand(0)) != nullptr) {
        _stats.commandsReceived++;

        _commandParamsDoc.clear(); 
        DeserializationError error = deserializeJson(_commandParamsDoc, rawCmd->jsonData);
        
        const char* id = _commandParamsDoc["id"];
        if (error) {
            logE("JSON parse error: %s", error.c_str());
            if (id) sendCommandError(id, SystemErrorCode::JSON_PARSE_ERROR, error.c_str());
            return; // Process only one command, so return after handling.
        }

        const char* commandName = _commandParamsDoc["command"];
        if (!id || !commandName) {
            logW("Missing 'id' or 'command' field in JSON command");
            if (id) sendCommandError(id, SystemErrorCode::JSON_INVALID_COMMAND, "Missing 'id' or 'command' field");
            return; // Process only one command, so return after handling.
        }

        logI("Processing command: %s (ID: %s)", commandName, id);

        // Check if the command requires a DGT connection.
        bool needsDGT = (strcmp(commandName, "getStatus") != 0);
        if (needsDGT && !isDGT3000Connected()) {
            sendCommandError(id, SystemErrorCode::DGT_NOT_CONFIGURED, "DGT3000 not connected");
            return; // Process only one command, so return after handling.
        }

        bool success = executeCommand(id, commandName, _commandParamsDoc["params"]);
        if (success) _stats.commandsExecuted++;
        else _stats.commandsFailed++;
    }
}

void I2CTaskManager::handleEvents() {
    if (!_dgt3000 || !isDGT3000Connected()) return;
    
    // Check for discrete button presses/releases.
    generateButtonEvent();

    // Check for button-hold-repeat events.
    handleButtonRepeat();

    // Check for time updates from the clock.
    if (_dgt3000->isNewTimeAvailable()) {
        uint8_t time[6];
        if (_dgt3000->getTime(time)) {
            generateTimeEvent(time);
        }
    }
}

void I2CTaskManager::monitorConnection() {
    if (shouldAttemptRecovery()) {
        attemptRecovery();
    }
    updateConnectionState();
}

// =============================================================================
// COMMAND PROCESSING
// =============================================================================

bool I2CTaskManager::executeCommand(const char* id, const char* commandName, const JsonObjectConst& params) {
    if (strcmp(commandName, "setTime") == 0) return executeSetTime(id, params);
    if (strcmp(commandName, "displayText") == 0) return executeDisplayText(id, params);
    if (strcmp(commandName, "endDisplay") == 0) return executeEndDisplay(id);
    if (strcmp(commandName, "stop") == 0) return executeStop(id);
    if (strcmp(commandName, "run") == 0) return executeRun(id, params);
    if (strcmp(commandName, "getTime") == 0) return executeGetTime(id);
    if (strcmp(commandName, "getStatus") == 0) return executeGetStatus(id);
    
    sendCommandError(id, SystemErrorCode::JSON_INVALID_COMMAND, "Unknown command");
    return false;
}

bool I2CTaskManager::executeSetTime(const char* id, const JsonObjectConst& params) {
    uint8_t leftMode = params["leftMode"];
    uint8_t leftHours = params["leftHours"];
    uint8_t leftMinutes = params["leftMinutes"];
    uint8_t leftSeconds = params["leftSeconds"];
    uint8_t rightMode = params["rightMode"];
    uint8_t rightHours = params["rightHours"];
    uint8_t rightMinutes = params["rightMinutes"];
    uint8_t rightSeconds = params["rightSeconds"];
    
    if (!validateTimeParameters(leftMode, leftHours, leftMinutes, leftSeconds, rightMode, rightHours, rightMinutes, rightSeconds)) {
        sendCommandError(id, SystemErrorCode::JSON_INVALID_PARAMETERS, "Invalid time parameters");
        return false;
    }
    
    bool success = _dgt3000->setAndRun(leftMode, leftHours, leftMinutes, leftSeconds, rightMode, rightHours, rightMinutes, rightSeconds);
    
    if (success) {
        _responseResultDoc.clear();
        _responseResultDoc["status"] = "Time set successfully";
        sendCommandResponse(id, true, _responseResultDoc.as<JsonObjectConst>());
    } else {
        handleDGT3000Error(_dgt3000->getLastError());
        sendCommandError(id, SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to set time on DGT3000");
    }
    return success;
}

bool I2CTaskManager::executeDisplayText(const char* id, const JsonObjectConst& params) {
    const char* text = params["text"];
    uint8_t beep = params["beep"] | 0;
    uint8_t leftDots = params["leftDots"] | 0;
    uint8_t rightDots = params["rightDots"] | 0;
    
    if (!validateDisplayTextParameters(text, beep, leftDots, rightDots)) {
        sendCommandError(id, SystemErrorCode::JSON_INVALID_PARAMETERS, "Invalid display text parameters");
        return false;
    }
    
    bool success = _dgt3000->displayText(text, beep, leftDots, rightDots);
    
    if (success) {
        _responseResultDoc.clear();
        _responseResultDoc["status"] = "Text displayed successfully";
        sendCommandResponse(id, true, _responseResultDoc.as<JsonObjectConst>());
    } else {
        handleDGT3000Error(_dgt3000->getLastError());
        sendCommandError(id, SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to display text on DGT3000");
    }
    return success;
}

bool I2CTaskManager::executeEndDisplay(const char* id) {
    bool success = _dgt3000->endDisplay();
    if (success) {
        _responseResultDoc.clear();
        _responseResultDoc["status"] = "Display ended successfully";
        sendCommandResponse(id, true, _responseResultDoc.as<JsonObjectConst>());
    } else {
        handleDGT3000Error(_dgt3000->getLastError());
        sendCommandError(id, SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to end display");
    }
    return success;
}

bool I2CTaskManager::executeStop(const char* id) {
    bool success = _dgt3000->stop();
    if (success) {
        _responseResultDoc.clear();
        _responseResultDoc["status"] = "Timers stopped successfully";
        sendCommandResponse(id, true, _responseResultDoc.as<JsonObjectConst>());
    } else {
        handleDGT3000Error(_dgt3000->getLastError());
        sendCommandError(id, SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to stop timers");
    }
    return success;
}

bool I2CTaskManager::executeRun(const char* id, const JsonObjectConst& params) {
    uint8_t leftMode = params["leftMode"];
    uint8_t rightMode = params["rightMode"];
    
    if (!validateRunParameters(leftMode, rightMode)) {
        sendCommandError(id, SystemErrorCode::JSON_INVALID_PARAMETERS, "Invalid run parameters");
        return false;
    }
    
    bool success = _dgt3000->run(leftMode, rightMode);
    if (success) {
        _responseResultDoc.clear();
        _responseResultDoc["status"] = "Timers started successfully";
        sendCommandResponse(id, true, _responseResultDoc.as<JsonObjectConst>());
    } else {
        handleDGT3000Error(_dgt3000->getLastError());
        sendCommandError(id, SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to start timers");
    }
    return success;
}

bool I2CTaskManager::executeGetTime(const char* id) {
    uint8_t time[6];
    if (_dgt3000->getTime(time)) {
        _responseResultDoc.clear();
        auto& result = _responseResultDoc;
        result["leftHours"] = time[0];
        result["leftMinutes"] = time[1];
        result["leftSeconds"] = time[2];
        result["rightHours"] = time[3];
        result["rightMinutes"] = time[4];
        result["rightSeconds"] = time[5];
        sendCommandResponse(id, true, result.as<JsonObjectConst>());
        return true;
    } else {
        handleDGT3000Error(_dgt3000->getLastError());
        sendCommandError(id, SystemErrorCode::I2C_COMMUNICATION_ERROR, "Failed to get time");
        return false;
    }
}

bool I2CTaskManager::executeGetStatus(const char* id) {
    _responseResultDoc.clear();
    auto& result = _responseResultDoc;
    result["dgtConnected"] = isDGT3000Connected();
    result["dgtConfigured"] = isDGT3000Configured();
    result["bleConnected"] = _bleConnected;
    result["lastUpdateTime"] = _lastUpdateTime;
    result["recoveryAttempts"] = _recoveryAttempts;
    
    if (_dgt3000) {
        result["lastDgtError"] = _dgt3000->getLastError();
        result["lastDgtErrorString"] = _dgt3000->getErrorString(_dgt3000->getLastError());
    }
    
    sendCommandResponse(id, true, result.as<JsonObjectConst>());
    return true;
}

// =============================================================================
// RESPONSE HANDLING
// =============================================================================

void I2CTaskManager::sendCommandResponse(const char* id, bool success, const JsonObjectConst& result) {
    if (!_queueManager) return;
    
    auto response = std::unique_ptr<CommandResponse>(new CommandResponse());
    strncpy(response->id, id, APP_MAX_COMMAND_ID_LENGTH - 1);
    response->id[APP_MAX_COMMAND_ID_LENGTH - 1] = '\0';
    response->success = success;
    response->timestamp = millis();

    if (success) {
        response->result = result;
    } else {
        response->errorCode = static_cast<SystemErrorCode>(result["errorCode"].as<int>());
        const char* msg = result["errorMessage"];
        strncpy(response->errorMessage, msg, APP_MAX_ERROR_MESSAGE_LENGTH - 1);
        response->errorMessage[APP_MAX_ERROR_MESSAGE_LENGTH - 1] = '\0';
    }
    
    if (!_queueManager->sendResponse(std::move(response), 100)) {
        logW("Failed to send command response to queue");
    }
    
    if (_systemStatus) {
        _systemStatus->commandsProcessed++;
        _systemStatus->updateActivity();
    }
}

void I2CTaskManager::sendCommandError(const char* id, SystemErrorCode errorCode, const char* message) {
    _responseResultDoc.clear();
    _responseResultDoc["errorCode"] = static_cast<uint16_t>(errorCode);
    _responseResultDoc["errorMessage"] = message ? message : getErrorCodeString(errorCode);
    sendCommandResponse(id, false, _responseResultDoc.as<JsonObjectConst>());
}

// =============================================================================
// EVENT GENERATION
// =============================================================================
void I2CTaskManager::generateButtonEvent() {
    if (!_queueManager || !_dgt3000) return;

    uint8_t button;
    // Process all discrete button events from the DGT3000's internal buffer.
    while (_dgt3000->getButtonEvent(&button)) {
        auto event = std::unique_ptr<DGTEvent>(new DGTEvent(DGTEvent::BUTTON_EVENT));
        event->priority = 0; // High priority

        JsonDocument& buttonData = event->data;
        const char* buttonName = getButtonName(button);
        buttonData["button"] = buttonName;
        buttonData["buttonCode"] = button;
        buttonData["isRepeat"] = false;

        if (_queueManager->sendPriorityEvent(std::move(event), 2)) {
            _stats.eventsGenerated++;
            // Reset repeat tracking on any new discrete event.
            _buttonMonitoring.buttonRepeatActive = false;
            _buttonMonitoring.buttonRepeatCount = 0;
            logI("Button event: %s (code: 0x%02X)", buttonName, button);
        }
    }
}

void I2CTaskManager::handleButtonRepeat() {
    if (!_queueManager || !_dgt3000) return;

    uint8_t currentButtonState = _dgt3000->getButtonState();
    uint32_t now = millis();
    uint8_t mainButtonsState = currentButtonState & 0x1F; // Only main 5 buttons repeat

    if (mainButtonsState != 0) {
        // A main button is being held down.
        if (!_buttonMonitoring.buttonRepeatActive) {
            // First time detecting a hold, initialize repeat state.
            _buttonMonitoring.buttonRepeatActive = true;
            _buttonMonitoring.lastButtonState = mainButtonsState;
            _buttonMonitoring.lastButtonTime = now;
            _buttonMonitoring.buttonRepeatCount = 0;
        }

        if (_buttonMonitoring.lastButtonState == mainButtonsState) {
            uint32_t holdDuration = now - _buttonMonitoring.lastButtonTime;
            uint32_t repeatThreshold = (_buttonMonitoring.buttonRepeatCount == 0) ? 800 : 400;

            if (holdDuration > repeatThreshold) {
                _buttonMonitoring.buttonRepeatCount++;
                _buttonMonitoring.lastButtonTime = now;

                auto event = std::unique_ptr<DGTEvent>(new DGTEvent(DGTEvent::BUTTON_EVENT));
                event->priority = 0;

                JsonDocument& buttonData = event->data;
                const char* buttonName = getButtonName(mainButtonsState);
                buttonData["button"] = buttonName;
                buttonData["buttonCode"] = mainButtonsState;
                buttonData["isRepeat"] = true;
                buttonData["repeatCount"] = _buttonMonitoring.buttonRepeatCount;
                
                if (_queueManager->sendPriorityEvent(std::move(event), 2)) {
                    _stats.eventsGenerated++;
                    logI("Button repeat: %s (count: %d)", buttonName, _buttonMonitoring.buttonRepeatCount);
                }
            }
        } else {
            // A different button is being held, reset state.
            _buttonMonitoring.buttonRepeatActive = false;
            _buttonMonitoring.buttonRepeatCount = 0;
        }

    } else {
        // No main buttons are pressed, reset repeat logic.
        _buttonMonitoring.buttonRepeatActive = false;
        _buttonMonitoring.buttonRepeatCount = 0;
    }
}

void I2CTaskManager::generateTimeEvent(const uint8_t time[6]) {
    if (!_queueManager) return;

    auto event = std::unique_ptr<DGTEvent>(new DGTEvent(DGTEvent::TIME_UPDATE));
    event->priority = 1; // Lower priority

    JsonDocument& timeData = event->data;
    timeData["leftHours"] = time[0];
    timeData["leftMinutes"] = time[1];
    timeData["leftSeconds"] = time[2];
    timeData["rightHours"] = time[3];
    timeData["rightMinutes"] = time[4];
    timeData["rightSeconds"] = time[5];

    if (_queueManager->sendEvent(std::move(event), 2)) {
        _stats.eventsGenerated++;
        logD("Time event sent: L %d:%02d:%02d R %d:%02d:%02d", time[0], time[1], time[2], time[3], time[4], time[5]);
    }
}

void I2CTaskManager::generateConnectionStatusEvent(bool connected, bool configured) {
    if (!_queueManager) return;
    
    auto event = std::unique_ptr<DGTEvent>(new DGTEvent(DGTEvent::CONNECTION_STATUS));
    event->data["connected"] = connected;
    event->data["configured"] = configured;
    
    _queueManager->sendEvent(std::move(event), 100);
}

void I2CTaskManager::generateErrorEvent(SystemErrorCode errorCode, const char* message) {
    if (!_queueManager) return;
    
    auto event = std::unique_ptr<DGTEvent>(new DGTEvent(DGTEvent::ERROR_EVENT));
    event->data["errorCode"] = static_cast<uint16_t>(errorCode);
    event->data["errorMessage"] = message ? message : getErrorCodeString(errorCode);
    
    if (_queueManager->sendPriorityEvent(std::move(event), 100)) {
        logI("Error event sent: %s", message ? message : getErrorCodeString(errorCode));
    } else {
        logW("Failed to send error event: %s", message ? message : getErrorCodeString(errorCode));
    }
}

// =============================================================================
// DGT3000 MANAGEMENT
// =============================================================================

bool I2CTaskManager::configureDGT3000() {
    if (!_dgt3000) return false;
    
    logD("Configuring DGT3000...");
    if (!_dgt3000->configure()) {
        logW("Failed to configure DGT3000");
        return false;
    }
    
    logI("DGT3000 configured successfully");
    return true;
}

void I2CTaskManager::handleDGT3000Error(int error) {
    logE("DGT3000 error: %d (%s)", error, _dgt3000->getErrorString(error));
    
    SystemErrorCode systemError = mapDGTErrorToSystemError(error);
    generateErrorEvent(systemError, _dgt3000->getErrorString(error));
    
    if (_systemStatus) {
        _systemStatus->lastError = systemError;
        strncpy(_systemStatus->lastErrorMessage, _dgt3000->getErrorString(error), sizeof(_systemStatus->lastErrorMessage) - 1);
        _systemStatus->lastErrorMessage[sizeof(_systemStatus->lastErrorMessage) - 1] = '\0';
    }
    
    // If a communication error occurs, mark the clock as disconnected.
    if (error == DGT_ERROR_I2C_COMM || error == DGT_ERROR_TIMEOUT || error == DGT_ERROR_NO_ACK || error == DGT_ERROR_CLOCK_OFF || error == DGT_ERROR_CRC || error == DGT_ERROR_NOT_CONFIGURED) {
        if (_dgtConnectionState == ConnectionState::CONNECTED) {
            logW("DGT3000 disconnected due to error.");
            _dgtConnectionState = ConnectionState::DISCONNECTED;
            _dgtConfigured = false;
            generateConnectionStatusEvent(false, false);
            updateConnectionState();
        }
    }
}

// =============================================================================
// ERROR RECOVERY
// =============================================================================

bool I2CTaskManager::attemptRecovery() {
    if (I2C_TASK_MAX_RECOVERY_ATTEMPTS > 0 && _recoveryAttempts >= I2C_TASK_MAX_RECOVERY_ATTEMPTS) {
        return false; // Max attempts reached
    }
    
    uint32_t now = millis();
    if (now - _lastRecoveryAttempt < I2C_TASK_RECOVERY_DELAY_MS) {
        return false; // Too soon for another attempt
    }
    
    _recoveryAttempts++;
    _lastRecoveryAttempt = now;
    
    logI("Attempting DGT3000 recovery (attempt %d)...", _recoveryAttempts);
    return performRecovery();
}

bool I2CTaskManager::performRecovery() {
    logI("Starting DGT3000 recovery sequence...");
    if (configureDGT3000()) {
        logI("DGT3000 recovery successful");
        _dgtConnectionState = ConnectionState::CONNECTED;
        _dgtConfigured = true;
        _recoveryAttempts = 0;
        generateConnectionStatusEvent(true, true);
        return true;
    }
    
    logW("DGT3000 recovery failed");
    return false;
}

void I2CTaskManager::resetRecoveryState() {
    _recoveryAttempts = 0;
    _lastRecoveryAttempt = 0;
}

bool I2CTaskManager::shouldAttemptRecovery() const {
    // Attempt recovery only if DGT is disconnected, BLE is connected,
    // and no initialization is currently in progress.
    return !isDGT3000Connected() && _bleConnected && !_initializingDGT && 
           (I2C_TASK_MAX_RECOVERY_ATTEMPTS == 0 || _recoveryAttempts < I2C_TASK_MAX_RECOVERY_ATTEMPTS);
}

// =============================================================================
// STATE MANAGEMENT & UTILITIES
// =============================================================================

void I2CTaskManager::setState(I2CTaskState newState) {
    if (_stateMutex && xSemaphoreTake(_stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _taskState = newState;
        xSemaphoreGive(_stateMutex);
    } else {
        _taskState = newState; // Fallback if mutex fails
    }
}

void I2CTaskManager::updateConnectionState() {
    if (_systemStatus) {
        _systemStatus->dgtConnectionState = _dgtConnectionState;
        _systemStatus->dgtConfigured = _dgtConfigured;
    }
}

void I2CTaskManager::updateStatistics() {
    _stats.uptime = millis();
    _stats.lastUpdateTime = _lastUpdateTime;
    _stats.recoveryAttempts = _recoveryAttempts;
}

const I2CTaskStats& I2CTaskManager::getStatistics() const {
    return _stats;
}

void I2CTaskManager::resetStatistics() {
    _stats = I2CTaskStats();
}

void I2CTaskManager::printStatus() {
    logI("--- I2C Task Status ---");
    logI("Task State: %s", getI2CTaskStateString(_taskState));
    logI("DGT Connected: %s, Configured: %s", isDGT3000Connected() ? "YES" : "NO", isDGT3000Configured() ? "YES" : "NO");
    logI("BLE Connected: %s", _bleConnected ? "YES" : "NO");
    logI("Recovery Attempts: %d", _recoveryAttempts);
}

void I2CTaskManager::printStatistics() {
    logI("--- I2C Task Statistics ---");
    logI("Uptime: %lu ms", _stats.uptime);
    logI("Commands: Rcvd=%lu, Exec=%lu, Fail=%lu", _stats.commandsReceived, _stats.commandsExecuted, _stats.commandsFailed);
    logI("Events Generated: %lu", _stats.eventsGenerated);
    logI("DGT Errors: %lu", _stats.dgtErrors);
    logI("Recovery Attempts: %lu", _stats.recoveryAttempts);
}

bool I2CTaskManager::isTimeout(uint32_t startTime, uint32_t timeoutMs) const {
    return (millis() - startTime) > timeoutMs;
}

void I2CTaskManager::delayWithYield(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

const char* getI2CTaskStateString(I2CTaskState state) {
    switch (state) {
        case I2CTaskState::IDLE: return "IDLE";
        case I2CTaskState::INITIALIZED: return "INITIALIZED";
        case I2CTaskState::RUNNING: return "RUNNING";
        case I2CTaskState::STOPPING: return "STOPPING";
        case I2CTaskState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* I2CTaskManager::getButtonName(uint8_t buttonCode) const {
    // Note: The DGT3000 library generates specific codes for lever and on/off events.
    switch (buttonCode) {
        // Main 5 buttons
        case DGT_BUTTON_BACK: return "back";
        case DGT_BUTTON_MINUS: return "minus";
        case DGT_BUTTON_PLAY_PAUSE: return "play_pause";
        case DGT_BUTTON_PLUS: return "plus";
        case DGT_BUTTON_FORWARD: return "forward";

        // Special event codes based on press/release
        case DGT_EVENT_ON_OFF_PRESS: return "on_off_press";
        case DGT_EVENT_ON_OFF_RELEASE: return "on_off_release";
        case DGT_EVENT_LEVER_RIGHT: return "lever_right";
        case DGT_EVENT_LEVER_LEFT: return "lever_left";
        
        default: return "unknown";
    }
}

SystemErrorCode mapDGTErrorToSystemError(int dgtError) {
    switch (dgtError) {
        case DGT_ERROR_I2C_COMM:
        case DGT_ERROR_I2C_INIT:
            return SystemErrorCode::I2C_COMMUNICATION_ERROR;
        case DGT_ERROR_TIMEOUT:
        case DGT_ERROR_NO_ACK:
            return SystemErrorCode::COMMAND_TIMEOUT;
        case DGT_ERROR_NOT_CONFIGURED:
            return SystemErrorCode::DGT_NOT_CONFIGURED;
        case DGT_ERROR_CRC:
            return SystemErrorCode::I2C_CRC_ERROR;
        case DGT_ERROR_CLOCK_OFF:
            return SystemErrorCode::DGT_NOT_CONNECTED;
        default:
            return SystemErrorCode::UNKNOWN_ERROR;
    }
}
