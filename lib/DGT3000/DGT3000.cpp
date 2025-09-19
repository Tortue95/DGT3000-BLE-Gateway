/*
 * DGT3000 Library for ESP32
 *
 * This library provides communication with DGT3000 chess clocks via dual I2C.
 *
 * Helped by the original implementation for DGTPi
 * https://github.com/jromang/dgtpi
 *
 * Copyright (C) 2025 Tortue - ESP32 Implementation
 * Original work Copyright (C) jromang and contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <Arduino.h>
#include "DGT3000.h"

// Static instance for handling I2C slave callbacks.
DGT3000* DGT3000::_instance = nullptr;

// Pre-calculated CRC-8-ATM table (x^8 + x^2 + x + 1).
static const uint8_t crc_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

DGT3000::DGT3000() {
    _initialized = false;
    _connected = false;
    _configured = false;
    _lastError = DGT_SUCCESS;
    _currentListenAddress = 0xFF;  // Invalid address
    _recoveryInProgress = false;
    
    _masterSDA = DGT3000_DEFAULT_MASTER_SDA;
    _masterSCL = DGT3000_DEFAULT_MASTER_SCL;
    _slaveSDA = DGT3000_DEFAULT_SLAVE_SDA;
    _slaveSCL = DGT3000_DEFAULT_SLAVE_SCL;
    
    _i2cMaster = nullptr;
    _i2cSlave = nullptr;
    
    _receivedAckCmd = 0x00;
    _newAckReceived = false;
    _newPingResponseReceived = false;
    _newTimeAvailable = false;
    
    resetRxData();
    
    _instance = this;
}

bool DGT3000::begin(int masterSDA, int masterSCL, int slaveSDA, int slaveSCL) {
    // Validate GPIO pins for ESP32
    if (masterSDA < 0 || masterSDA > 48 || masterSCL < 0 || masterSCL > 48 ||
        slaveSDA < 0 || slaveSDA > 48 || slaveSCL < 0 || slaveSCL > 48) {
        DGT_LOG_INFO("DGT3000: Invalid GPIO pin numbers.");
        _lastError = DGT_ERROR_I2C_INIT;
        return false;
    }
    
    _masterSDA = masterSDA;
    _masterSCL = masterSCL;
    _slaveSDA = slaveSDA;
    _slaveSCL = slaveSCL;
    
    // Clean up any existing I2C instances
    if (_i2cMaster) {
        _i2cMaster->end();
        delete _i2cMaster;
        _i2cMaster = nullptr;
    }
    if (_i2cSlave) {
        _i2cSlave->end();
        delete _i2cSlave;
        _i2cSlave = nullptr;
    }
    
    // Initialize I2C Master (for sending commands)
    _i2cMaster = new TwoWire(0);
    if (!_i2cMaster->begin(_masterSDA, _masterSCL, DGT3000_I2C_FREQUENCY)) {
        DGT_LOG_INFO("DGT3000: Failed to initialize I2C Master.");
        DGT_LOG_INFO_F("DGT3000: Master pins - SDA: %d, SCL: %d", _masterSDA, _masterSCL);
        delete _i2cMaster;
        _i2cMaster = nullptr;
        _lastError = DGT_ERROR_I2C_INIT;
        return false;
    }
    
    // Initialize I2C Slave (for receiving data)
    _i2cSlave = new TwoWire(1);
    
    _initialized = true;
    _lastError = DGT_SUCCESS;
    
    DGT_LOG_INFO("DGT3000: Initialized successfully.");
    DGT_LOG_INFO_F("DGT3000: Master SDA=%d, SCL=%d", _masterSDA, _masterSCL);
    DGT_LOG_INFO_F("DGT3000: Slave SDA=%d, SCL=%d", _slaveSDA, _slaveSCL);
    
    // Set default listening address for data messages.
    setSlaveListenAddress(DGT3000_ESP_ADDR_00);
    
    return true;
}

void DGT3000::end() {
    powerOff();
    delay(500);
    
    if (_i2cSlave) {
        _i2cSlave->end();
        delete _i2cSlave;
        _i2cSlave = nullptr;
    }
    
    if (_i2cMaster) {
        _i2cMaster->end();
        delete _i2cMaster;
        _i2cMaster = nullptr;
    }
    
    _initialized = false;
    _connected = false;
    _configured = false;
    
    DGT_LOG_INFO("DGT3000: Ended.");
}

bool DGT3000::isConfigured() { return _configured; }
bool DGT3000::isConnected() { return _connected; }
int DGT3000::getLastError() { return _lastError; }

const char* DGT3000::getErrorString(int error) {
    switch (error) {
        case DGT_SUCCESS: return "Success";
        case DGT_ERROR_I2C_INIT: return "I2C initialization failed";
        case DGT_ERROR_I2C_COMM: return "I2C communication error";
        case DGT_ERROR_TIMEOUT: return "Timeout";
        case DGT_ERROR_NO_ACK: return "No acknowledgment";
        case DGT_ERROR_BUFFER_OVERRUN: return "Buffer overrun";
        case DGT_ERROR_CRC: return "CRC error";
        case DGT_ERROR_CLOCK_OFF: return "Clock is off";
        case DGT_ERROR_NOT_CONFIGURED: return "Not configured";
        case DGT_ERROR_INIT_FAILED: return "Initialization failed after recovery";
        default: return "Unknown error";
    }
}

void DGT3000::resetRxData() { 
    memset(&_rxData, 0, sizeof(_rxData)); 
}

bool DGT3000::isButtonBufferFull() {
    return ((_rxData.buttonEnd + 1) % DGT3000_BUTTON_BUFFER_SIZE) == _rxData.buttonStart;
}

void DGT3000::addButtonEvent(uint8_t button) {
    if (!isButtonBufferFull()) {
        _rxData.buttonBuffer[_rxData.buttonEnd] = button;
        _rxData.buttonEnd = (_rxData.buttonEnd + 1) % DGT3000_BUTTON_BUFFER_SIZE;
    } else {
        // If the buffer is full, overwrite the oldest event to avoid losing the newest one.
        _rxData.buttonStart = (_rxData.buttonStart + 1) % DGT3000_BUTTON_BUFFER_SIZE;
        _rxData.buttonBuffer[_rxData.buttonEnd] = button;
        _rxData.buttonEnd = (_rxData.buttonEnd + 1) % DGT3000_BUTTON_BUFFER_SIZE;
        DGT_LOG_INFO("DGT3000: Button buffer full, overwriting oldest event.");
    }
}

// Parameter validation functions
bool validateDisplayTextParameters(const char* text, uint8_t beep, uint8_t leftDots, uint8_t rightDots) {
    if (!text) {
        DGT_LOG_INFO("DGT3000: Validation Error: Text is null.");
        return false;
    }
    if (strlen(text) > DGT3000_DISPLAY_TEXT_MAX) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Text length %d exceeds max %d.", strlen(text), DGT3000_DISPLAY_TEXT_MAX);
        return false;
    }
    // Beep duration is in 62.5ms units, max 48 (3 seconds).
    if (beep > 48) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Beep duration %d exceeds max 48.", beep);
        return false;
    }
    
    // Validate leftDots: only allow specified flags.
    const uint8_t VALID_LEFT_DOTS_MASK = DGT_DOT_FLAG | DGT_DOT_WHITE_KING | DGT_DOT_BLACK_KING | DGT_DOT_COLON | DGT_DOT_DOT | DGT_DOT_EXTRA;
    if ((leftDots & ~VALID_LEFT_DOTS_MASK) != 0) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Invalid bits set in leftDots (0x%02X).", leftDots);
        return false;
    }
    
    // Validate rightDots: DGT_DOT_EXTRA is left-side only.
    const uint8_t VALID_RIGHT_DOTS_MASK = DGT_DOT_FLAG | DGT_DOT_WHITE_KING | DGT_DOT_BLACK_KING | DGT_DOT_COLON | DGT_DOT_DOT;
    if ((rightDots & ~VALID_RIGHT_DOTS_MASK) != 0) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Invalid bits set in rightDots (0x%02X).", rightDots);
        return false;
    }
    
    return true;
}

bool validateTimeParameters(uint8_t leftMode, uint8_t leftHours, uint8_t leftMinutes, uint8_t leftSeconds,
                           uint8_t rightMode, uint8_t rightHours, uint8_t rightMinutes, uint8_t rightSeconds) {
    if (leftMode > 2 || rightMode > 2) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Invalid run mode (left: %d, right: %d). Must be 0-2.", leftMode, rightMode);
        return false;
    }
    if (leftHours > 9 || rightHours > 9) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Invalid hours (left: %d, right: %d). Must be 0-9.", leftHours, rightHours);
        return false;
    }
    if (leftMinutes > 59 || rightMinutes > 59 || leftSeconds > 59 || rightSeconds > 59) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Invalid minutes/seconds (left: %d:%d, right: %d:%d). Must be 0-59.", leftMinutes, leftSeconds, rightMinutes, rightSeconds);
        return false;
    }
    return true;
}

bool validateRunParameters(uint8_t leftMode, uint8_t rightMode) {
    if (leftMode > 2 || rightMode > 2) {
        DGT_LOG_INFO_F("DGT3000: Validation Error: Invalid run mode (left: %d, right: %d). Must be 0-2.", leftMode, rightMode);
        return false;
    }
    return true;
}

bool DGT3000::isTimeout(uint32_t startTime, uint32_t timeout_ms) {
    return (millis() - startTime) >= timeout_ms;
}

void DGT3000::printHex(const uint8_t* data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        DGT_LOG_DEBUG_F("%02X ", data[i]);
    }
    DGT_LOG_DEBUG_LN("");
}

#ifdef DGT_DEBUG
void DGT3000::printMessageTrace(const char* direction, const uint8_t* data, uint8_t length, const char* description) {
    DGT_LOG_DEBUG_F("%s ", direction);
    for (int i = 0; i < length; i++) {
        DGT_LOG_DEBUG_F("%02X ", data[i]);
    }
    if (description) {
        DGT_LOG_DEBUG_LN_F("= %s", description);
    } else {
        DGT_LOG_DEBUG_LN("");
    }
}

void DGT3000::printConnectionStatus() {
    DGT_LOG_DEBUG("=== DGT3000 Connection Status ===");
    DGT_LOG_DEBUG_F("Initialized: %s", _initialized ? "YES" : "NO");
    DGT_LOG_DEBUG_F("Connected: %s", _connected ? "YES" : "NO");
    DGT_LOG_DEBUG_F("Configured: %s", _configured ? "YES" : "NO");
    DGT_LOG_DEBUG_F("Current Listen Address: 0x%02X", _currentListenAddress);
    DGT_LOG_DEBUG_F("Last Error: %s (%d)", getErrorString(_lastError), _lastError);
    DGT_LOG_DEBUG_F("Master I2C: SDA=%d, SCL=%d", _masterSDA, _masterSCL);
    DGT_LOG_DEBUG_F("Slave I2C: SDA=%d, SCL=%d", _slaveSDA, _slaveSCL);
    DGT_LOG_DEBUG("================================");
}

void DGT3000::printBufferStatus() {
    DGT_LOG_DEBUG("=== DGT3000 Buffer Status ===");
    DGT_LOG_DEBUG_F("Button Buffer: Start=%d, End=%d, Count=%d", 
                   _rxData.buttonStart, _rxData.buttonEnd,
                   (_rxData.buttonEnd - _rxData.buttonStart + DGT3000_BUTTON_BUFFER_SIZE) % DGT3000_BUTTON_BUFFER_SIZE);
    DGT_LOG_DEBUG_F("Last Button State: 0x%02X", _rxData.lastButtonState);
    DGT_LOG_DEBUG_F("Current Time: %d:%02d:%02d | %d:%02d:%02d",
                   _rxData.time[0], _rxData.time[1], _rxData.time[2],
                   _rxData.time[3], _rxData.time[4], _rxData.time[5]);
    DGT_LOG_DEBUG("=============================");
}
#endif // DGT_DEBUG

bool DGT3000::configure() {
    if (!_initialized) {
        DGT_LOG_INFO("DGT3000: CONFIGURE - Not initialized, aborting.");
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (_recoveryInProgress) {
        DGT_LOG_INFO("DGT3000: CONFIGURE - Recovery already in progress, aborting.");
        return false; // Prevent recursive calls
    }
    _recoveryInProgress = true;

    _configured = false;
    _connected = false;

    // Step 1: Send a "Change State" command without expecting an ACK.
    // This can wake the clock up. If it fails, the clock is likely off.
    if (!changeState_no_ack()) {
        delay(100);
        // Try to wake the clock with a ping and repeat.
        if (!sendPing() || !changeState_no_ack()) {
            _lastError = DGT_ERROR_CLOCK_OFF;
            _recoveryInProgress = false;
            return false;
        }
    }
    delay(DGT3000_COMMAND_DELAY_MS);

    // Step 2: Take central control of the clock.
    if (!setCentralControl()) {
        _lastError = DGT_ERROR_I2C_COMM;
        _recoveryInProgress = false;
        return false;
    }
    delay(DGT3000_COMMAND_DELAY_MS);

    // Step 3: Send another "Change State" command, this time expecting an ACK.
    if (!changeState()) {
        _lastError = DGT_ERROR_I2C_COMM;
        _recoveryInProgress = false;
        return false;
    }
    delay(DGT3000_COMMAND_DELAY_MS);

    // Step 4: Initialize the clock time to 00:00:00.
    if (!setAndRun(DGT_MODE_STOP, 0, 0, 0, DGT_MODE_STOP, 0, 0, 0)) {
        _lastError = DGT_ERROR_I2C_COMM;
        _recoveryInProgress = false;
        return false;
    }

    // Configuration successful.
    _configured = true;
    _connected = true;
    _lastError = DGT_SUCCESS;
    _recoveryInProgress = false;
    DGT_LOG_INFO("DGT3000: Configuration successful.");
    return true;
}

bool DGT3000::changeState_no_ack() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    uint8_t cmd[] = {0x20, 0x06, 0x0b, 0x39, 0xb9}; // Command payload
    return sendDGTCommand("Change State (no ACK)", cmd, sizeof(cmd), DGT3000_ESP_ADDR_00, 0, 0);
}

bool DGT3000::changeState() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    uint8_t cmd[] = {0x20, 0x06, 0x0b, 0x39, 0xb9}; // Command payload
    return sendDGTCommand("Change State", cmd, sizeof(cmd), DGT3000_ESP_ADDR_10, DGT_CMD_CHANGE_STATE);
}

bool DGT3000::sendPing() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    uint8_t pingCmd[] = {0x20, 0x05, DGT_CMD_PING, 0x46}; // Ping command

    // Send the ping command without retry logic. It's normal for this to fail
    // if the clock is off, so we just send and listen for a response.
    sendDGTCommand("Ping (Wakeup)", pingCmd, sizeof(pingCmd), DGT3000_ESP_ADDR_00, 0, 0, DGT3000_I2C_WAKEUP_ADDR, false);

    // Wait for the specific ping response message.
    _newPingResponseReceived = false;
    uint32_t startTime = millis();
    while (!isTimeout(startTime, DGT3000_ACK_TIMEOUT_MS * 2)) { // Use a longer timeout for wakeup
        if (_newPingResponseReceived) {
            DGT_LOG_INFO("DGT3000: Ping response received.");
            _newPingResponseReceived = false; // Consume the response
            return true;
        }
        delay(5);
    }

    DGT_LOG_INFO("DGT3000: Timeout waiting for Ping response.");
    _lastError = DGT_ERROR_TIMEOUT;
    return false;
}

bool DGT3000::setCentralControl() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    uint8_t setCCCmd[] = {0x20, 0x05, 0x0f, 0x48};
    return sendDGTCommand("Set Central Control", setCCCmd, sizeof(setCCCmd), DGT3000_ESP_ADDR_10, DGT_CMD_SET_CC);
}

bool DGT3000::endDisplay() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    uint8_t endDisplayCmd[] = {0x20, 0x05, 0x07, 0x70};

    // I removed the check ACK of command End Display to avoid issues with the long time to change Listen address from 0x10 to 0x00
    // (with ACK we lost BTN event and connection with dgt 3000 if we miss some messages on 0x00...)
    
    // old command with ACK:
    // return sendDGTCommand("End Display", endDisplayCmd, sizeof(endDisplayCmd), DGT3000_ESP_ADDR_10, DGT_CMD_END_DISPLAY);
    return sendDGTCommand("End Display", endDisplayCmd, sizeof(endDisplayCmd), DGT3000_ESP_ADDR_00, DGT_CMD_END_DISPLAY, 0);
}

bool DGT3000::sendDisplayEmpty() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    uint8_t displayEmptyCmd[] = {0x20, 0x15, 0x06, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xff, 0x00, 0x03, 0x01, 0x01, 0xfc};
    return sendDGTCommand("Display Empty", displayEmptyCmd, sizeof(displayEmptyCmd), DGT3000_ESP_ADDR_00, DGT_CMD_DISPLAY);
}

bool DGT3000::displayText(const char* text, uint8_t beep, uint8_t leftDots, uint8_t rightDots) {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (!validateDisplayTextParameters(text, beep, leftDots, rightDots)) {
        _lastError = DGT_ERROR_I2C_COMM;
        return false;
    }
    
    // First, send End Display to clear any previous text.
    if (!endDisplay()) {
        DGT_LOG_INFO("DGT3000: Failed to clear display before showing text.");
        return false;
    }
    
    // Construct the display command payload.
    uint8_t displayCmd[20];
    displayCmd[0] = 0x20;  // Source address
    displayCmd[1] = 0x15;  // Length
    displayCmd[2] = DGT_CMD_DISPLAY;
    
    // Copy text, padding with spaces to 11 characters.
    int textLen = strlen(text);
    for (int i = 0; i < DGT3000_DISPLAY_TEXT_MAX; i++) {
        displayCmd[3 + i] = (i < textLen) ? text[i] : 0x20;
    }
    
    displayCmd[14] = 0xFF;
    displayCmd[15] = beep;
    displayCmd[16] = 0x03;
    displayCmd[17] = leftDots;
    displayCmd[18] = rightDots;
    
    calculateCRC(displayCmd, sizeof(displayCmd));
    
    return sendDGTCommand("Display", displayCmd, sizeof(displayCmd), DGT3000_ESP_ADDR_00, DGT_CMD_DISPLAY);
}

bool DGT3000::setAndRun(uint8_t leftMode, uint8_t leftHours, uint8_t leftMinutes, uint8_t leftSeconds,
                        uint8_t rightMode, uint8_t rightHours, uint8_t rightMinutes, uint8_t rightSeconds) {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (!validateTimeParameters(leftMode, leftHours, leftMinutes, leftSeconds, rightMode, rightHours, rightMinutes, rightSeconds)) {
        _lastError = DGT_ERROR_I2C_COMM;
        return false;
    }
    
    uint8_t setAndRunCmd[11];
    setAndRunCmd[0] = 0x20;  // Source address
    setAndRunCmd[1] = 0x0c;  // Length
    setAndRunCmd[2] = DGT_CMD_SET_AND_RUN;
    
    // Left timer values (minutes and seconds in BCD)
    setAndRunCmd[3] = leftHours;
    setAndRunCmd[4] = ((leftMinutes / 10) << 4) | (leftMinutes % 10);
    setAndRunCmd[5] = ((leftSeconds / 10) << 4) | (leftSeconds % 10);
    
    // Right timer values
    setAndRunCmd[6] = rightHours;
    setAndRunCmd[7] = ((rightMinutes / 10) << 4) | (rightMinutes % 10);
    setAndRunCmd[8] = ((rightSeconds / 10) << 4) | (rightSeconds % 10);
    
    // Pack run modes into a single byte.
    setAndRunCmd[9] = leftMode | (rightMode << 2);
    
    calculateCRC(setAndRunCmd, sizeof(setAndRunCmd));
    
    
    // I removed the check ACK of command Set And Run to avoid issues with the long time to change Listen address from 0x10 to 0x00
    // (with ACK we lost BTN event and connection with dgt 3000 if we miss some messages on 0x00...)
    
    // old command with ACK:
    // return sendDGTCommand("Set And Run", setAndRunCmd, sizeof(setAndRunCmd), DGT3000_ESP_ADDR_10, DGT_CMD_SET_AND_RUN);
    return sendDGTCommand("Set And Run", setAndRunCmd, sizeof(setAndRunCmd), DGT3000_ESP_ADDR_00, DGT_CMD_SET_AND_RUN, 0);
}

bool DGT3000::stop() {
    DGT_LOG_INFO("DGT3000: Stopping timers.");
    return setAndRun(DGT_MODE_STOP, _rxData.time[0], _rxData.time[1], _rxData.time[2],
                     DGT_MODE_STOP, _rxData.time[3], _rxData.time[4], _rxData.time[5]);
}

bool DGT3000::run(uint8_t leftMode, uint8_t rightMode) {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (!validateRunParameters(leftMode, rightMode)) {
        _lastError = DGT_ERROR_I2C_COMM;
        return false;
    }
    DGT_LOG_INFO("DGT3000: Running timers.");
    return setAndRun(leftMode, _rxData.time[0], _rxData.time[1], _rxData.time[2],
                     rightMode, _rxData.time[3], _rxData.time[4], _rxData.time[5]);
}

bool DGT3000::powerOff() {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }

    // The power-off command is a special variant of "Change State".
    uint8_t powerOffCmd[5];
    powerOffCmd[0] = 0x20; // Source address
    powerOffCmd[1] = 0x06; // Length
    powerOffCmd[2] = DGT_CMD_CHANGE_STATE;
    powerOffCmd[3] = 0x00; // Special data byte for power off
    
    calculateCRC(powerOffCmd, sizeof(powerOffCmd));

    // Send without expecting an ACK.
    bool result = sendDGTCommand("Power Off", powerOffCmd, sizeof(powerOffCmd), DGT3000_ESP_ADDR_00, 0, 0, DGT3000_I2C_ADDRESS, false);

    if (result) {
        DGT_LOG_INFO("DGT3000: Power Off command sent.");
        _connected = false;
        _configured = false;
    }
    return result;
}

bool DGT3000::getTime(uint8_t time[6]) {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (time == nullptr) {
        _lastError = DGT_ERROR_I2C_COMM;
        return false;
    }
    
    memcpy(time, _rxData.time, 6);
    _lastError = DGT_SUCCESS;
    return true;
}

bool DGT3000::isNewTimeAvailable() {
    if (_newTimeAvailable) {
        _newTimeAvailable = false; // Consume the flag
        return true;
    }
    return false;
}

bool DGT3000::getButtonEvent(uint8_t* button) {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (button == nullptr) {
        _lastError = DGT_ERROR_I2C_COMM;
        return false;
    }
    
    // Check if the circular buffer is empty.
    if (_rxData.buttonStart == _rxData.buttonEnd) {
        *button = 0;
        _lastError = DGT_SUCCESS;
        return false;
    }
    
    // Retrieve the oldest event from the buffer.
    *button = _rxData.buttonBuffer[_rxData.buttonStart];
    _rxData.buttonStart = (_rxData.buttonStart + 1) % DGT3000_BUTTON_BUFFER_SIZE;
    
    _lastError = DGT_SUCCESS;
    return true;
}

uint8_t DGT3000::getButtonState() {
    if (!_initialized) {
        return 0;
    }
    
    // Return current button state 
    // Button state format 
    // 0x01 = back button
    // 0x02 = minus button  
    // 0x04 = play/pause button
    // 0x08 = plus button
    // 0x10 = forward button
    // 0x20 = on/off button
    // 0x40 = lever position (right side down = 0x40)
    
    _lastError = DGT_SUCCESS;
    return _rxData.lastButtonState;
}

bool DGT3000::sendDGTCommand(const char* name, uint8_t* cmd, int length,
                            uint8_t ackListenAddress, uint8_t expectedAckCmd, int numAcks,
                            uint8_t targetAddress, bool withRetry) {
    if (!_initialized || !_i2cMaster) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    if (cmd == nullptr || length <= 0) {
        _lastError = DGT_ERROR_I2C_COMM;
        return false;
    }

    int maxAttempts = withRetry ? 3 : 1;

    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        DGT_LOG_DEBUG_F("-> 10 ");
        printHex(cmd, length);
        DGT_LOG_DEBUG_LN_F("= %s", name);

        setSlaveListenAddress(ackListenAddress);
        _newAckReceived = false;
        _receivedAckCmd = 0x00;

        _i2cMaster->beginTransmission(targetAddress);
        _i2cMaster->write(cmd, length);
        uint8_t result = _i2cMaster->endTransmission();

        if (result != 0) {
            DGT_LOG_DEBUG("       Send error: I2C transmission failed.");
            _lastError = DGT_ERROR_I2C_COMM;
            if (withRetry) {
                delay(DGT3000_RETRY_DELAY_MS);
                continue;
            } else {
                // For non-retry sends (like wakeup ping), failure is not a critical error.
                return true; 
            }
        }

        if (numAcks == 0) {
            _lastError = DGT_SUCCESS;
            return true;
        }

        if (waitForAck(expectedAckCmd)) {
            setSlaveListenAddress(DGT3000_ESP_ADDR_00); // Revert to default listen address
            _lastError = DGT_SUCCESS;
            return true;
        } else if (attempt < maxAttempts) {
            DGT_LOG_DEBUG("       Send error: ACK not received, retrying...");
            _lastError = DGT_ERROR_NO_ACK;
        }
    }

    DGT_LOG_INFO_F("       Sending %s command failed after all attempts.", name);
    setSlaveListenAddress(DGT3000_ESP_ADDR_00);
    _connected = false;
    _configured = false;
    return false;
}

void DGT3000::setSlaveListenAddress(uint8_t address) {
    if (!_initialized || !_i2cSlave) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return;
    }
    if (_currentListenAddress == address) {
        return; // Already listening on this address
    }
    
    _i2cSlave->end();
    delay(DGT3000_ADDRESS_SWITCH_DELAY_MS);
    
    if (!_i2cSlave->begin(address, _slaveSDA, _slaveSCL, DGT3000_I2C_FREQUENCY)) {
        DGT_LOG_INFO_F("DGT3000: Failed to set slave listen address 0x%02X", address);
        _lastError = DGT_ERROR_I2C_INIT;
        _currentListenAddress = 0xFF; // Mark as invalid
        return;
    }
    
    _i2cSlave->onReceive(onSlaveReceiveStatic);
    _currentListenAddress = address;
    DGT_LOG_DEBUG_F("       (listening on 0x%02X)", address);
}

void DGT3000::listenForTimeMessages() {
    setSlaveListenAddress(DGT3000_ESP_ADDR_00);
}

void DGT3000::listenForAckMessages() {
    setSlaveListenAddress(DGT3000_ESP_ADDR_10);
}

void DGT3000::onSlaveReceive(int bytesReceived) {
    if (!_initialized || !_i2cSlave || bytesReceived == 0) {
        return;
    }
    
    uint8_t rxBuffer[DGT3000_RECEIVE_BUFFER_LENGTH];
    int idx = 0;
    while (_i2cSlave->available() && idx < sizeof(rxBuffer)) {
        rxBuffer[idx++] = _i2cSlave->read();
    }
    
    DGT_LOG_DEBUG_F("<- ");
    printHex(rxBuffer, idx);

    // A valid message from the clock should be at least 3 bytes and addressed to the ESP.
    if (idx >= 3 && rxBuffer[0] == DGT3000_ESP_ADDR_10) {
        const uint8_t message_type = rxBuffer[2];
        
        switch (message_type) {
            case 1:  // ACK message
                processAckMessage(rxBuffer, idx);
                break;
            case DGT_MSG_WAKEUP_RESP: // Wakeup response
                processPingResponseMessage(rxBuffer, idx);
                break;
            case 4:  // Time message
                processTimeMessage(rxBuffer, idx);
                break;
            case 5:  // Button message
                processButtonMessage(rxBuffer, idx);
                break;
            default:
                DGT_LOG_DEBUG_LN_F("= Unknown message type %d", message_type);
                break;
        }
    } else {
        DGT_LOG_DEBUG_LN("");
    }
}

void DGT3000::onSlaveReceiveStatic(int bytesReceived) {
    if (_instance) {
        _instance->onSlaveReceive(bytesReceived);
    }
}

void DGT3000::processAckMessage(uint8_t* buffer, uint8_t length) {
    // ACK format: 10 08 01 [CMD] [STATUS] ...
    if (length < 5 || buffer[2] != 0x01) {
        DGT_LOG_DEBUG("DGT3000: Invalid ACK message.");
        return;
    }
    
    const uint8_t cmd_code = buffer[3];
    _receivedAckCmd = cmd_code;
    _newAckReceived = true;
    
    DGT_LOG_DEBUG_F("= Ack for command 0x%02X", cmd_code);
}

void DGT3000::processPingResponseMessage(uint8_t* buffer, uint8_t length) {
    // Expected response: 10 07 02 22 01 05
    const uint8_t expectedResponse[] = {0x10, 0x07, 0x02, 0x22, 0x01, 0x05};
    if (length >= sizeof(expectedResponse) && memcmp(buffer, expectedResponse, sizeof(expectedResponse)) == 0) {
        DGT_LOG_DEBUG_LN("= Ping Response OK");
        _newPingResponseReceived = true;
    } else {
        DGT_LOG_DEBUG_LN("= Invalid Ping Response");
    }
}

void DGT3000::processTimeMessage(uint8_t* buffer, uint8_t length) {
    // Some time messages are echoes and should be ignored.
    if (length > 19 && buffer[19] == 1) {
        DGT_LOG_DEBUG_LN("= Time: Ignoring no-update message");
        return;
    }
    if (length < 14 || buffer[1] != 0x18) {
        DGT_LOG_INFO("DGT3000: Invalid time message.");
        return;
    }
    
    auto bcdToDec = [](uint8_t bcd) -> uint8_t { return ((bcd >> 4) * 10) + (bcd & 0x0F); };

    uint8_t left_h = buffer[4] & 0x0F;
    uint8_t left_m = bcdToDec(buffer[5]);
    uint8_t left_s = bcdToDec(buffer[6]);
    
    uint8_t right_h = buffer[10] & 0x0F;
    uint8_t right_m = bcdToDec(buffer[11]);
    uint8_t right_s = bcdToDec(buffer[12]);

    // Validate parsed values to prevent data corruption.
    if (right_h > 9 || right_m > 59 || right_s > 59 || left_h > 9 || left_m > 59 || left_s > 59) {
        DGT_LOG_DEBUG_LN("DGT3000: Invalid time values in message, ignoring.");
        return;
    }

    DGT_LOG_DEBUG_LN_F("= Time: Left %d:%02d:%02d, Right %d:%02d:%02d", left_h, left_m, left_s, right_h, right_m, right_s);
    
    // Store time.
    _rxData.time[0] = left_h;
    _rxData.time[1] = left_m;
    _rxData.time[2] = left_s;
    _rxData.time[3] = right_h;
    _rxData.time[4] = right_m;
    _rxData.time[5] = right_s;
    _newTimeAvailable = true;
    
    // If we receive time, we are connected.
    if (!_connected) {
        DGT_LOG_INFO("DGT3000: Time messages received - connection restored.");
        _connected = true;
        _configured = false; // May need reconfiguration.
    }
}

void DGT3000::processButtonMessage(uint8_t* buffer, uint8_t length) {
    if (buffer == nullptr || length < 5 || buffer[2] != 5) return;

    uint8_t currentButtons = buffer[3];
    uint8_t previousButtons = buffer[4];

    DGT_LOG_DEBUG_LN_F("= Button Msg: current=0x%02X, previous=0x%02X", currentButtons, previousButtons);

    // Update the internal state. This is the source of truth for getButtonState().
    _rxData.lastButtonState = currentButtons;

    uint8_t changedButtons = currentButtons ^ previousButtons;
    if (!changedButtons) return; // No change, no event.
    
    // --- Event Generation Logic based on state changes ---

    // 1. On/Off Button
    if (changedButtons & DGT_ON_OFF_STATE_MASK) {
        // If the button bit is now 1, it's a press; if 0, it's a release.
        uint8_t event = (currentButtons & DGT_ON_OFF_STATE_MASK) 
                        ? DGT_EVENT_ON_OFF_PRESS 
                        : DGT_EVENT_ON_OFF_RELEASE;
        addButtonEvent(event);
    }
    // 2. Lever
    else if (changedButtons & DGT_LEVER_STATE_MASK) {
        uint8_t event = (currentButtons & DGT_LEVER_STATE_MASK) 
                        ? DGT_EVENT_LEVER_LEFT 
                        : DGT_EVENT_LEVER_RIGHT;
        addButtonEvent(event);
    }
    // 3. Main 5 buttons (these don't have release events)
    else {
        uint8_t mainButtonPressed = changedButtons & currentButtons & 0x1F;
        if (mainButtonPressed) {
            addButtonEvent(mainButtonPressed);
        }
    }
}

bool DGT3000::waitForAck(uint8_t expectedCmd, uint32_t timeout_ms) {
    if (!_initialized) {
        _lastError = DGT_ERROR_NOT_CONFIGURED;
        return false;
    }
    
    _newAckReceived = false;
    _receivedAckCmd = 0x00;
    
    uint32_t startTime = millis();
    while (!isTimeout(startTime, timeout_ms)) {
        if (_newAckReceived && _receivedAckCmd == expectedCmd) {
            _newAckReceived = false;
            _lastError = DGT_SUCCESS;
            return true;
        }
        delay(5);
    }
    
    DGT_LOG_INFO_F("DGT3000: ACK timeout waiting for command 0x%02X.", expectedCmd);
    _lastError = DGT_ERROR_TIMEOUT;
    return false;
}

uint8_t DGT3000::calculateCRC(uint8_t* buffer, uint8_t length) {
    if (buffer == nullptr || length < 3) {
        _lastError = DGT_ERROR_CRC;
        return 0;
    }

    uint8_t crc = 0;
    // The DGT protocol includes the destination address (0x10) in the CRC calculation,
    // which is not part of the command buffer itself.
    crc = crc_table[crc ^ 0x10];
    
    // The length field (buffer[1]) is the total payload length including the CRC byte.
    // We calculate CRC on the bytes from buffer[0] up to (but not including) the CRC byte.
    uint8_t crc_length = buffer[1] - 1;
    if (crc_length >= length) {
        crc_length = length - 1; // Prevent buffer overflow
    }
    
    for (int i = 0; i < crc_length; i++) {
        crc = crc_table[crc ^ buffer[i]];
    }

    // Store the calculated CRC at the end of the message payload.
    buffer[crc_length] = crc;
    return crc;
}

bool DGT3000::verifyCRC(uint8_t* buffer, uint8_t length) {
    if (buffer == nullptr || length < 3) {
        _lastError = DGT_ERROR_CRC;
        return false;
    }

    uint8_t crc = 0;
    crc = crc_table[crc ^ 0x10];
    
    uint8_t crc_length = buffer[1] - 1;
    if (crc_length >= length) {
        crc_length = length - 1;
    }
    
    for (int i = 0; i < crc_length; i++) {
        crc = crc_table[crc ^ buffer[i]];
    }

    uint8_t receivedCRC = buffer[crc_length];
    bool isValid = (crc == receivedCRC);

    if (!isValid) {
        DGT_LOG_DEBUG_F("DGT3000: CRC mismatch! Calculated: 0x%02X, Received: 0x%02X", crc, receivedCRC);
        _lastError = DGT_ERROR_CRC;
    }
    return isValid;
}

const uint8_t* DGT3000::getCRCTable() {
    return crc_table;
}
