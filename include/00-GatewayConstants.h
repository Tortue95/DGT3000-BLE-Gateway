/*
 * BLE Gateway System Constants
 *
 * This header defines system-wide constants, such as GPIO pin assignments,
 * BLE UUIDs, queue sizes, JSON buffer sizes, and other configuration
 * parameters for the DGT3000 BLE Gateway application.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef BLE_GATEWAY_CONSTANTS_H
#define BLE_GATEWAY_CONSTANTS_H

#include <Arduino.h>

// =============================================================================
// DGT3000 I2C GPIO PIN CONFIGURATION
// =============================================================================

/**
 * @brief GPIO pin for DGT3000 I2C Master SDA.
 */
constexpr int DGT3000_MASTER_SDA_PIN = 8;

/**
 * @brief GPIO pin for DGT3000 I2C Master SCL.
 */
constexpr int DGT3000_MASTER_SCL_PIN = 5;

/**
 * @brief GPIO pin for DGT3000 I2C Slave SDA.
 */
constexpr int DGT3000_SLAVE_SDA_PIN = 7;

/**
 * @brief GPIO pin for DGT3000 I2C Slave SCL.
 */
constexpr int DGT3000_SLAVE_SCL_PIN = 6;

// =============================================================================
// LED CONFIGURATION
// =============================================================================

/**
 * @brief GPIO pin connected to the NeoPixel LED for status indication.
 */
constexpr int LED_NEOPIXEL_PIN = 21;

// =============================================================================
// BLE PROTOCOL VERSION and FW VERSION
// =============================================================================

/**
 * @brief Current version of the BLE communication protocol.
 */
constexpr const char* BLE_PROTOCOL_VERSION = "1.0";

/**
 * @brief Current version of the DGT3000 BLE Gateway application.
 */
constexpr const char* GATEWAY_APP_VERSION = "0.2-beta";

// =============================================================================
// BLE SERVICE AND CHARACTERISTIC UUIDS
// =============================================================================

/**
 * @brief BLE GATT Service UUID for the DGT3000 Gateway.
 */
constexpr const char* BLE_DGT3000_SERVICE_UUID = "73822f6e-edcd-44bb-974b-93ee97cb0000";

/**
 * @brief BLE GATT Characteristic UUID for the Protocol Version.
 * Clients can read this to get the current communication protocol version.
 */
constexpr const char* BLE_PROTOCOL_VERSION_CHAR_UUID = "73822f6e-edcd-44bb-974b-93ee97cb0001";

/**
 * @brief BLE GATT Characteristic UUID for sending commands to the gateway.
 * Clients write to this characteristic to control the DGT3000 clock.
 */
constexpr const char* BLE_COMMAND_CHAR_UUID = "73822f6e-edcd-44bb-974b-93ee97cb0002";

/**
 * @brief BLE GATT Characteristic UUID for receiving events from the gateway.
 * Clients subscribe to notifications to receive events (e.g., button presses, time updates).
 */
constexpr const char* BLE_EVENT_CHAR_UUID = "73822f6e-edcd-44bb-974b-93ee97cb0003";

/**
 * @brief BLE GATT Characteristic UUID for reading system status.
 * Clients read this to get the current status of the gateway and DGT3000 clock.
 */
constexpr const char* BLE_STATUS_CHAR_UUID = "73822f6e-edcd-44bb-974b-93ee97cb0004";

// =============================================================================
// DEVICE AND APPLICATION CONFIGURATION
// =============================================================================

/**
 * @brief Name of the BLE device as it appears during advertising.
 */
constexpr const char* BLE_DEVICE_NAME = "DGT3000-Gateway";

/**
 * @brief Maximum length for a command ID string.
 * Used to correlate commands with their responses.
 */
constexpr size_t APP_MAX_COMMAND_ID_LENGTH = 32;

/**
 * @brief Maximum length for an error message string.
 */
constexpr size_t APP_MAX_ERROR_MESSAGE_LENGTH = 128;

// =============================================================================
// QUEUE CONFIGURATION
// =============================================================================

/**
 * @brief Size of the command queue (BLE -> I2C task).
 * Commands from a BLE client are queued to be processed by the I2C task.
 */
constexpr uint32_t QUEUE_COMMAND_SIZE = 10;

/**
 * @brief Size of the event queue (I2C task -> BLE).
 * Events from the I2C task are queued to be sent as BLE notifications.
 */
constexpr uint32_t QUEUE_EVENT_SIZE = 20;

/**
 * @brief Default timeout for queue operations in milliseconds.
 */
constexpr uint32_t QUEUE_OPERATION_TIMEOUT_MS = 1000;

// =============================================================================
// JSON BUFFER SIZES
// =============================================================================

/**
 * @brief Maximum size for incoming BLE command JSON documents.
 */
constexpr size_t JSON_COMMAND_BUFFER_SIZE = 512;

/**
 * @brief Maximum size for outgoing BLE event JSON documents.
 */
constexpr size_t JSON_EVENT_BUFFER_SIZE = 256;

/**
 * @brief Maximum size for system status JSON documents.
 */
constexpr size_t JSON_STATUS_BUFFER_SIZE = 512;

// =============================================================================
// I2C TASK CONFIGURATION
// =============================================================================

/**
 * @brief Stack size for the I2C task in bytes.
 */
constexpr uint32_t I2C_TASK_STACK_SIZE = 8192;

/**
 * @brief Priority for the I2C task.
 */
constexpr UBaseType_t I2C_TASK_PRIORITY = 2;

/**
 * @brief Core on which the I2C task will run (Core 0 is recommended for I2C).
 */
constexpr BaseType_t I2C_TASK_CORE = 0;

/**
 * @brief Update interval for the main loop of the I2C task in milliseconds.
 */
constexpr uint32_t I2C_TASK_UPDATE_INTERVAL_MS = 10;

/**
 * @brief Delay between DGT3000 connection recovery attempts in milliseconds.
 */
constexpr uint32_t I2C_TASK_RECOVERY_DELAY_MS = 1000;

/**
 * @brief Maximum number of recovery attempts before stopping. Set to 0 for unlimited.
 */
constexpr uint8_t I2C_TASK_MAX_RECOVERY_ATTEMPTS = 0;

#endif // BLE_GATEWAY_CONSTANTS_H
