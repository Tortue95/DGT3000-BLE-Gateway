/*
 * BLE Gateway Core Data Structures Implementation
 *
 * This file provides the implementation for utility functions that convert
 * enums to human-readable strings, aiding in logging and debugging.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "BLEGatewayTypes.h"
#include <logging.hpp>

using namespace esp32m;

// =============================================================================
// ERROR CODE TO STRING CONVERSION
// =============================================================================

const char* getErrorCodeString(SystemErrorCode error) {
    switch (error) {
        case SystemErrorCode::SUCCESS:
            return "Success";
            
        // I2C Communication Errors
        case SystemErrorCode::I2C_COMMUNICATION_ERROR:
            return "I2C Communication Error";
        case SystemErrorCode::DGT_NOT_CONFIGURED:
            return "DGT3000 Not Configured";
        case SystemErrorCode::I2C_CRC_ERROR:
            return "I2C CRC Error";
        case SystemErrorCode::DGT_NOT_CONNECTED:
            return "DGT Not Connected";
            
        // JSON Processing Errors
        case SystemErrorCode::JSON_PARSE_ERROR:
            return "JSON Parse Error";
        case SystemErrorCode::JSON_INVALID_COMMAND:
            return "Invalid JSON Command";
        case SystemErrorCode::JSON_INVALID_PARAMETERS:
            return "Invalid JSON Parameters";
            
        // Command Execution Errors
        case SystemErrorCode::COMMAND_TIMEOUT:
            return "Command Timeout";
            
        case SystemErrorCode::UNKNOWN_ERROR:
        default:
            return "Unknown Error";
    }
}

// =============================================================================
// SYSTEM STATE TO STRING CONVERSION
// =============================================================================

const char* getSystemStateString(SystemState state) {
    switch (state) {
        case SystemState::UNINITIALIZED:
            return "Uninitialized";
        case SystemState::INITIALIZING:
            return "Initializing";
        case SystemState::IDLE:
            return "Idle";
        case SystemState::ACTIVE:
            return "Active";
        case SystemState::ERROR_RECOVERY:
            return "Error Recovery";
        default:
            return "Unknown State";
    }
}

// =============================================================================
// CONNECTION STATE TO STRING CONVERSION
// =============================================================================

const char* getConnectionStateString(ConnectionState state) {
    switch (state) {
        case ConnectionState::DISCONNECTED:
            return "Disconnected";
        case ConnectionState::CONNECTED:
            return "Connected";
        case ConnectionState::CONFIGURED:
            return "Configured";
        case ConnectionState::ERROR:
            return "Error";
        default:
            return "Unknown Connection State";
    }
}

// =============================================================================
// EVENT TYPE TO STRING CONVERSION
// =============================================================================

const char* getEventTypeString(DGTEvent::Type type) {
    switch (type) {
        case DGTEvent::TIME_UPDATE:
            return "timeUpdate";
        case DGTEvent::BUTTON_EVENT:
            return "buttonEvent";
        case DGTEvent::CONNECTION_STATUS:
            return "connectionStatus";
        case DGTEvent::ERROR_EVENT:
            return "error";
        case DGTEvent::SYSTEM_STATUS:
            return "systemStatus";
        default:
            return "unknown";
    }
}
