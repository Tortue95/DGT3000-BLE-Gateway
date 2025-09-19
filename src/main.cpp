/*
 * DGT3000 BLE Gateway - Main Application
 *
 * This is the main entry point for the DGT3000 BLE Gateway. It initializes and
 * integrates all system components, including the BLE service (Core 1) and
 * the I2C task manager (Core 0), to function as a bridge between a BLE client
 * and the DGT3000 chess clock.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <Arduino.h>
#include "BLEGatewayTypes.h"
#include "QueueManager.h"
#include "BLEService.h"
#include "I2CTaskManager.h"
#include "LedManager.h"
#include "00-GatewayConstants.h"
#include <logging.hpp>
#include "driver/temp_sensor.h"
#include "serial-appender.hpp"

using namespace esp32m;

// =============================================================================
// GLOBAL INSTANCES
// =============================================================================

// Appender for the logging framework to output to the Serial port.
SerialAppender serialAppender;

// Global objects for managing system components.
SystemStatus g_systemStatus;
std::unique_ptr<QueueManager> g_queueManager;
std::unique_ptr<DGT3000BLEService> g_bleService;
std::unique_ptr<I2CTaskManager> g_i2cTaskManager;
std::unique_ptr<LedManager> g_ledManager;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void onBLEConnected();
void onBLEDisconnected();
void printSystemStatus();

// =============================================================================
// INITIALIZATION AND CLEANUP
// =============================================================================

/**
 * @brief Initializes all system components in the correct order.
 * @return true if initialization is successful, false otherwise.
 */
bool initializeSystem() {
    log_i("=== DGT3000 BLE Gateway Starting ===");
    
    g_systemStatus = SystemStatus();
    g_systemStatus.systemState = SystemState::INITIALIZING;
    g_systemStatus.updateUptime();
    log_i("System status initialized.");
    log_d("Free heap before initialization: %d KB", ESP.getFreeHeap() / 1024);

    // Step 0: Initialize ESP32 internal temperature sensor.
    log_d("Step 0: Initializing ESP32 Temperature Sensor...");
    temp_sensor_config_t temp_sensor_config = TSENS_CONFIG_DEFAULT();
    temp_sensor_set_config(temp_sensor_config);
    temp_sensor_start();
    
    // Step 1: Initialize the QueueManager for inter-task communication.
    log_d("Step 1: Creating and initializing Queue Manager...");
    g_queueManager = std::unique_ptr<QueueManager>(new QueueManager());
    if (!g_queueManager || !g_queueManager->initialize()) {
        log_e("ERROR: Failed to initialize Queue Manager");
        return false;
    }
    log_d("Free heap after queue manager: %d KB", ESP.getFreeHeap() / 1024);
    
    // Step 2: Initialize the I2C Task Manager. The task itself is not started yet.
    log_d("Step 2: Creating and initializing I2C Task Manager...");
    g_i2cTaskManager = std::unique_ptr<I2CTaskManager>(new I2CTaskManager(g_queueManager.get(), &g_systemStatus));
    if (!g_i2cTaskManager || !g_i2cTaskManager->initialize()) {
        log_e("ERROR: Failed to initialize I2C Task Manager");
        return false;
    }
    log_d("Free heap after I2C manager: %d KB", ESP.getFreeHeap() / 1024);
    
    // Step 3: Initialize the BLE Service.
    log_d("Step 3: Creating and initializing BLE Service...");
    g_bleService = std::unique_ptr<DGT3000BLEService>(new DGT3000BLEService(g_queueManager.get(), &g_systemStatus));
    if (!g_bleService || !g_bleService->initialize()) {
        log_e("ERROR: Failed to initialize BLE Service");
        return false;
    }
    log_d("Free heap after BLE service: %d KB", ESP.getFreeHeap() / 1024);
    
    // Step 4: Start the I2C task on Core 0. This is done last to ensure all other components are ready.
    log_d("Step 4: Starting I2C Task on Core 0...");
    if (!g_i2cTaskManager->startTask()) {
        log_e("ERROR: Failed to start I2C Task");
        return false;
    }
    log_d("Free heap after I2C task start: %d KB", ESP.getFreeHeap() / 1024);
    
    // Step 5: Initialize the LED Manager for status indication.
    log_d("Step 5: Initializing LED Manager...");
    g_ledManager = std::unique_ptr<LedManager>(new LedManager(LED_NEOPIXEL_PIN, 1));
    if (!g_ledManager) {
        log_e("ERROR: Failed to create LED Manager");
        // This is not a fatal error, so we continue.
    } else {
        g_ledManager->initialize();
    }

    g_systemStatus.systemState = SystemState::IDLE;
    g_systemStatus.updateActivity();
    log_i("=== System Initialization Complete ===");
    return true;
}

/**
 * @brief Cleans up all system components in reverse order of initialization.
 */
void cleanupSystem() {
    log_i("=== System Cleanup Starting ===");
    
    // unique_ptr handles deletion automatically upon reset.
    if (g_i2cTaskManager) {
        g_i2cTaskManager->stopTask();
        g_i2cTaskManager->cleanup();
    }
    g_i2cTaskManager.reset();

    if (g_bleService) {
        g_bleService->cleanup();
    }
    g_bleService.reset();
    
    if (g_queueManager) {
        g_queueManager->cleanup();
    }
    g_queueManager.reset();

    g_ledManager.reset();
    
    log_i("=== System Cleanup Complete ===");
}

// =============================================================================
// BLE CONNECTION CALLBACKS
// =============================================================================

/**
 * @brief Callback executed when a BLE client connects.
 */
void onBLEConnected() {
    log_i("BLE Client connected");
    
    if (g_ledManager) g_ledManager->setState(LED_STATE_CLIENT_CONNECTED);
    if (g_i2cTaskManager) g_i2cTaskManager->onBLEConnected();
    
    g_systemStatus.systemState = SystemState::ACTIVE;
    g_systemStatus.updateActivity();
}

/**
 * @brief Callback executed when a BLE client disconnects.
 */
void onBLEDisconnected() {
    log_i("BLE Client disconnected. Rebooting system...");
    
    if (g_ledManager) g_ledManager->setState(LED_STATE_WAITING_FOR_CONNECTION);
    if (g_i2cTaskManager) g_i2cTaskManager->onBLEDisconnected();
    
    g_systemStatus.systemState = SystemState::IDLE;
    g_systemStatus.updateActivity();

    // Restart the ESP32 to ensure a clean state for the next connection.
    ESP.restart(); 
}

// =============================================================================
// MAIN LOOP AND SYSTEM TASKS
// =============================================================================

/**
 * @brief Main task loop for Core 1. Handles BLE events and system monitoring.
 */
void processSystemTasks() {
    g_systemStatus.updateUptime();
    
    if (g_bleService) g_bleService->processEvents();
    
    // Update the LED status based on the system state.
    if (g_ledManager) {
        LedState newState = LED_STATE_WAITING_FOR_CONNECTION; // Default: waiting for BLE
        if (g_bleService && g_bleService->isConnected()) {
            newState = LED_STATE_CLIENT_CONNECTED; // Blue: BLE connected
            if (g_i2cTaskManager && g_i2cTaskManager->isDGT3000Connected()) {
                newState = LED_STATE_DGT_CONFIGURED; // Green: DGT is also ready
            }
        }
        g_ledManager->setState(newState);
        g_ledManager->update();
    }
    
    // Periodic health and status checks.
    static uint32_t lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 5000) { // Every 5 seconds
        lastHealthCheck = millis();
        
        if (g_queueManager && !g_queueManager->isHealthy()) log_w("WARNING: Queue system is unhealthy (high utilization).");
        if (g_i2cTaskManager && !g_i2cTaskManager->isTaskRunning()) log_w("WARNING: I2C Task is not running.");
        
        // Print full status every 30 seconds for diagnostics.
        static uint32_t lastStatusPrint = 0;
        if (millis() - lastStatusPrint > 30000) {
            lastStatusPrint = millis();
            printSystemStatus();
        }
    }
}

/**
 * @brief Prints a summary of the current system status to the log.
 */
void printSystemStatus() {
    log_i("--- System Status ---");
    log_i("State: %s", getSystemStateString(g_systemStatus.systemState));
    log_i("BLE Connected: %s", (g_bleService && g_bleService->isConnected()) ? "YES" : "NO");
    log_i("DGT Connected: %s", (g_i2cTaskManager && g_i2cTaskManager->isDGT3000Connected()) ? "YES" : "NO");
    log_i("Uptime: %lu ms", g_systemStatus.uptime);
    log_i("Free Heap: %d KB", ESP.getFreeHeap() / 1024);
    log_i("Commands: %lu, Events: %lu", g_systemStatus.commandsProcessed, g_systemStatus.eventsGenerated);
    
    if (g_queueManager) {
        log_i("Queues (Used/Size): RawCmd=%d/%d, Event=%d/%d, Resp=%d/%d",
              g_queueManager->getRawCommandQueueDepth(), QUEUE_COMMAND_SIZE,
              g_queueManager->getEventQueueDepth(), QUEUE_EVENT_SIZE,
              g_queueManager->getResponseQueueDepth(), QUEUE_COMMAND_SIZE);
    }
    log_i("---------------------");
}

/**
 * @brief Handles fatal errors by attempting a graceful cleanup and restarting the device.
 */
void handleSystemError() {
    log_e("CRITICAL ERROR: System entering recovery and restarting.");
    printSystemStatus(); // Log final status before restart.
    cleanupSystem();
    delay(2000);
    ESP.restart();
}

// =============================================================================
// ARDUINO SETUP AND LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    
    // Configure the logging framework.
    Logging::level(LogLevel::Info); // Set default log level.
    Logging::useQueue(2048); // Enable asynchronous logging.
    Logging::addAppender(&serialAppender); // Direct logs to the Serial port.
    
    log_i("");
    log_i("DGT3000 BLE Gateway v%s", GATEWAY_APP_VERSION);
    log_i("Author: Tortue (2025)");
    log_i("");
    
    // Initialize all system components.
    if (!initializeSystem()) {
        log_e("FATAL: System initialization failed. Restarting.");
        handleSystemError();
    }
    
    log_i("System ready. Waiting for BLE connections...");
}

void loop() {
    // The main loop on Core 1 handles system tasks and monitoring.
    processSystemTasks();
    delay(10); // Yield to other tasks.
}
