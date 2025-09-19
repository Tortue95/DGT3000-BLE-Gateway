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


#ifndef DGT3000_H
#define DGT3000_H

#define DGT_DEBUG
#include <Arduino.h>
#include <Wire.h>

// DGT3000 I2C Configuration
#define DGT3000_I2C_ADDRESS     0x08    ///< Main I2C address of the DGT3000 clock.
#define DGT3000_I2C_WAKEUP_ADDR 0x28    ///< I2C address used to wake up the clock.
#define DGT3000_I2C_FREQUENCY   100000  ///< I2C communication frequency (100kHz).
#define DGT3000_ESP_ADDR_10     0x10    ///< I2C address for the ESP32 to receive ACKs.
#define DGT3000_ESP_ADDR_00     0x00    ///< I2C address for the ESP32 to receive data (time, buttons).

// Default GPIO pins
#define DGT3000_DEFAULT_MASTER_SDA  8   ///< Default GPIO for I2C Master SDA.
#define DGT3000_DEFAULT_MASTER_SCL  5   ///< Default GPIO for I2C Master SCL.
#define DGT3000_DEFAULT_SLAVE_SDA   7   ///< Default GPIO for I2C Slave SDA.
#define DGT3000_DEFAULT_SLAVE_SCL   6   ///< Default GPIO for I2C Slave SCL.

// Buffer sizes
#define DGT3000_RECEIVE_BUFFER_LENGTH   256 ///< General purpose receive buffer size.
#define DGT3000_BUTTON_BUFFER_SIZE      16  ///< Circular buffer size for button events.
#define DGT3000_DISPLAY_TEXT_MAX        11  ///< Maximum number of characters for display text.
#define DGT3000_MESSAGE_BUFFER_SIZE     32  ///< Buffer for constructing messages to send.

// Timeout and delay values
#define DGT3000_ACK_TIMEOUT_MS          50  ///< Timeout for waiting for an ACK from the clock.
#define DGT3000_RETRY_DELAY_MS          100 ///< Delay between command retries.
#define DGT3000_ADDRESS_SWITCH_DELAY_MS 10  ///< Delay for I2C slave address switching.
#define DGT3000_COMMAND_DELAY_MS        5   ///< Delay between sending configuration commands.

// Error codes
enum DGTError {
    DGT_SUCCESS = 0,                ///< Operation successful.
    DGT_ERROR_I2C_INIT = -1,        ///< I2C initialization failed.
    DGT_ERROR_I2C_COMM = -2,        ///< I2C communication error.
    DGT_ERROR_TIMEOUT = -3,         ///< Operation timed out.
    DGT_ERROR_NO_ACK = -4,          ///< No acknowledgment received from the clock.
    DGT_ERROR_BUFFER_OVERRUN = -5,  ///< Receive buffer overflow.
    DGT_ERROR_CRC = -6,             ///< CRC check failed.
    DGT_ERROR_CLOCK_OFF = -7,       ///< The clock appears to be off.
    DGT_ERROR_NOT_CONFIGURED = -8,  ///< The library is not configured.
    DGT_ERROR_INIT_FAILED = -10     ///< Initialization failed after a recovery attempt.
};

// Run modes for the clock timers
enum DGTRunMode {
    DGT_MODE_STOP = 0,          ///< Timer is stopped.
    DGT_MODE_COUNT_DOWN = 1,    ///< Timer is counting down.
    DGT_MODE_COUNT_UP = 2       ///< Timer is counting up.
};

// Button state bitmasks (for reading the current state)
#define DGT_BUTTON_BACK         0x01    ///< Back button.
#define DGT_BUTTON_MINUS        0x02    ///< Minus button.
#define DGT_BUTTON_PLAY_PAUSE   0x04    ///< Play/Pause button.
#define DGT_BUTTON_PLUS         0x08    ///< Plus button.
#define DGT_BUTTON_FORWARD      0x10    ///< Forward button.
#define DGT_ON_OFF_STATE_MASK   0x20    ///< Bitmask for On/Off button state.
#define DGT_LEVER_STATE_MASK    0x40    ///< Bitmask for lever state (1=right, 0=left).

// Button event codes (for getButtonEvent())
#define DGT_EVENT_LEVER_RIGHT       0x40    ///< Event for lever moved to the right.
#define DGT_EVENT_LEVER_LEFT        0xC0    ///< Event for lever moved to the left.
#define DGT_EVENT_ON_OFF_PRESS      0x20    ///< Event for On/Off button press.
#define DGT_EVENT_ON_OFF_RELEASE    0xA0    ///< Event for On/Off button release (only sent if clock remains on).

// Display icon bitmasks
#define DGT_DOT_FLAG            0x01    ///< Flag icon.
#define DGT_DOT_WHITE_KING      0x02    ///< White king icon.
#define DGT_DOT_BLACK_KING      0x04    ///< Black king icon.
#define DGT_DOT_COLON           0x08    ///< Colon separator.
#define DGT_DOT_DOT             0x10    ///< Dot separator.
#define DGT_DOT_EXTRA           0x20    ///< Extra icon (left side only).

// DGT Command and message codes
#define DGT_CMD_CHANGE_STATE    0x0b
#define DGT_CMD_SET_CC          0x0f
#define DGT_CMD_SET_AND_RUN     0x0a
#define DGT_CMD_END_DISPLAY     0x07
#define DGT_CMD_DISPLAY         0x06
#define DGT_CMD_PING            0x0d
#define DGT_MSG_WAKEUP_RESP     0x02

// Debug logging configuration
#ifdef DGT_DEBUG
    #define DGT_LOG(msg) log_i(msg)
    #define DGT_LOG_F(fmt, ...) log_i(fmt, ##__VA_ARGS__)
    #define DGT_LOG_INFO(msg) log_i(msg)
    #define DGT_LOG_INFO_F(fmt, ...) log_i(fmt, ##__VA_ARGS__)
    #define DGT_LOG_DEBUG(msg) log_d(msg)
    #define DGT_LOG_DEBUG_F(fmt, ...) log_d(fmt, ##__VA_ARGS__)
    #define DGT_LOG_DEBUG_LN(msg) log_d(msg)
    #define DGT_LOG_DEBUG_LN_F(fmt, ...) log_d(fmt, ##__VA_ARGS__)
#else
    #define DGT_LOG(msg)
    #define DGT_LOG_F(fmt, ...)
    #define DGT_LOG_INFO(msg)
    #define DGT_LOG_INFO_F(fmt, ...)
    #define DGT_LOG_DEBUG(msg)
    #define DGT_LOG_DEBUG_F(fmt, ...)
    #define DGT_LOG_DEBUG_LN(msg)
    #define DGT_LOG_DEBUG_LN_F(fmt, ...)
#endif

// Verbose debug configuration
#ifdef DGT_DEBUG_VERBOSE
    #define DGT_LOG_VERBOSE(msg) log_v(msg)
    #define DGT_LOG_VERBOSE_F(fmt, ...) log_v(fmt, ##__VA_ARGS__)
#else
    #define DGT_LOG_VERBOSE(msg)
    #define DGT_LOG_VERBOSE_F(fmt, ...)
#endif

// Global validation functions
bool validateDisplayTextParameters(const char* text, uint8_t beep, uint8_t leftDots, uint8_t rightDots);
bool validateTimeParameters(uint8_t leftMode, uint8_t leftHours, uint8_t leftMinutes, uint8_t leftSeconds,
                           uint8_t rightMode, uint8_t rightHours, uint8_t rightMinutes, uint8_t rightSeconds);
bool validateRunParameters(uint8_t leftMode, uint8_t rightMode);

class DGT3000 {
public:
    /**
     * @brief Construct a new DGT3000 object.
     */
    DGT3000();

    /**
     * @brief Initializes the dual I2C communication with the DGT3000 clock.
     * @param masterSDA GPIO pin for the master I2C SDA line.
     * @param masterSCL GPIO pin for the master I2C SCL line.
     * @param slaveSDA GPIO pin for the slave I2C SDA line.
     * @param slaveSCL GPIO pin for the slave I2C SCL line.
     * @return true if initialization is successful, false otherwise.
     */
    bool begin(int masterSDA = DGT3000_DEFAULT_MASTER_SDA,
               int masterSCL = DGT3000_DEFAULT_MASTER_SCL,
               int slaveSDA = DGT3000_DEFAULT_SLAVE_SDA,
               int slaveSCL = DGT3000_DEFAULT_SLAVE_SCL);

    /**
     * @brief Cleans up resources and stops I2C communication.
     */
    void end();

    /**
     * @brief Performs the initial configuration sequence required to communicate with the clock.
     * This includes waking the clock, taking central control, and setting an initial state.
     * @return true if configuration is successful, false otherwise.
     */
    bool configure();

    /**
     * @brief Sends a "Change State" command to the clock, expecting an ACK.
     * @return true on success, false on failure.
     */
    bool changeState();

    /**
     * @brief Sends a "Change State" command without waiting for an ACK. Used during initial wakeup.
     * @return true on success, false on failure.
     */
    bool changeState_no_ack();

    /**
     * @brief Sends a "Set Central Control" command to take control of the clock.
     * @return true on success, false on failure.
     */
    bool setCentralControl();

    /**
     * @brief Sends a ping command to wake up the clock if it's in a low-power state.
     * @return true if a ping response is received, false on timeout.
     */
    bool sendPing();

    /**
     * @brief Clears the display by sending an empty display command.
     * @return true on success, false on failure.
     */
    bool sendDisplayEmpty();

    /**
     * @brief Checks if the clock has been successfully configured.
     * @return true if configured, false otherwise.
     */
    bool isConfigured();

    /**
     * @brief Checks if the clock is currently connected and responding.
     * @return true if connected, false otherwise.
     */
    bool isConnected();

    /**
     * @brief Displays text on the clock's screen.
     * @param text The string to display (max 11 chars).
     * @param beep Beep duration (0-48, in 62.5ms units).
     * @param leftDots Bitmask for icons on the left display.
     * @param rightDots Bitmask for icons on the right display.
     * @return true on success, false on failure.
     */
    bool displayText(const char* text, uint8_t beep = 0,
                     uint8_t leftDots = 0, uint8_t rightDots = 0);

    /**
     * @brief Clears any text from the display and returns to the time display.
     * @return true on success, false on failure.
     */
    bool endDisplay();

    /**
     * @brief Sets the time and running mode for both players.
     * @param leftMode Running mode for the left timer.
     * @param leftHours Hours for the left timer.
     * @param leftMinutes Minutes for the left timer.
     * @param leftSeconds Seconds for the left timer.
     * @param rightMode Running mode for the right timer.
     * @param rightHours Hours for the right timer.
     * @param rightMinutes Minutes for the right timer.
     * @param rightSeconds Seconds for the right timer.
     * @return true on success, false on failure.
     */
    bool setAndRun(uint8_t leftMode, uint8_t leftHours, uint8_t leftMinutes, uint8_t leftSeconds,
                   uint8_t rightMode, uint8_t rightHours, uint8_t rightMinutes, uint8_t rightSeconds);

    /**
     * @brief Stops both timers, preserving the current time.
     * @return true on success, false on failure.
     */
    bool stop();

    /**
     * @brief Starts the timers with a specified mode, using the currently stored time.
     * @param leftMode The running mode for the left timer.
     * @param rightMode The running mode for the right timer.
     * @return true on success, false on failure.
     */
    bool run(uint8_t leftMode, uint8_t rightMode);

    /**
     * @brief Sends a command to power off the clock.
     * @return true if the command was sent successfully.
     */
    bool powerOff();

    /**
     * @brief Gets the current time from both sides of the clock.
     * @param time A 6-byte array to store the time: [L_H, L_M, L_S, R_H, R_M, R_S].
     * @return true on success.
     */
    bool getTime(uint8_t time[6]);

    /**
     * @brief Checks if a new time update has been received from the clock.
     * This flag is consumed on read.
     * @return true if new time is available, false otherwise.
     */
    bool isNewTimeAvailable();

    /**
     * @brief Retrieves the next button event from the event buffer.
     * @param button Pointer to a byte to store the button event code.
     * @return true if an event was retrieved, false if the buffer is empty.
     */
    bool getButtonEvent(uint8_t* button);

    /**
     * @brief Gets the last known raw state of all buttons and the lever.
     * @return A bitmask representing the button states.
     */
    uint8_t getButtonState();

    /**
     * @brief Gets the last error code.
     * @return The last recorded error code from the DGTError enum.
     */
    int getLastError();

    /**
     * @brief Gets a string description for a given error code.
     * @param error The error code.
     * @return A constant string describing the error.
     */
    const char* getErrorString(int error);

    /**
     * @brief Prints a byte array in hexadecimal format for debugging.
     * @param data Pointer to the data buffer.
     * @param length The number of bytes to print.
     */
    void printHex(const uint8_t* data, uint8_t length);

    #ifdef DGT_DEBUG
    /**
     * @brief Prints a detailed trace of an I2C message for debugging.
     * @param direction "->" for sent, "<-" for received.
     * @param data Pointer to the message data.
     * @param length The length of the message.
     * @param description An optional description of the message.
     */
    void printMessageTrace(const char* direction, const uint8_t* data, uint8_t length, const char* description = nullptr);
    /**
     * @brief Prints the current connection and configuration status for debugging.
     */
    void printConnectionStatus();
    /**
     * @brief Prints the status of internal buffers (e.g., button buffer) for debugging.
     */
    void printBufferStatus();
    #endif

    /**
     * @brief Calculates the CRC-8 value for a DGT message buffer and appends it.
     * @param buffer The message buffer.
     * @param length The total length of the message including the CRC byte.
     * @return The calculated CRC value.
     */
    uint8_t calculateCRC(uint8_t* buffer, uint8_t length);

    /**
     * @brief Verifies the CRC-8 value of a received DGT message.
     * @param buffer The received message buffer.
     * @param length The length of the message including the CRC byte.
     * @return true if the CRC is valid, false otherwise.
     */
    bool verifyCRC(uint8_t* buffer, uint8_t length);

    /**
     * @brief Gets a pointer to the internal CRC lookup table
     * @return A const pointer to the 256-byte CRC table.
     */
    const uint8_t* getCRCTable();

    /**
     * @brief Core function to send a command to the DGT clock.
     * @param name A descriptive name for the command
     * @param cmd The command buffer to send.
     * @param length The length of the command buffer.
     * @param ackListenAddress The I2C address to listen for the ACK on.
     * @param expectedAckCmd The command code expected in the ACK response.
     * @param numAcks The number of ACKs expected (usually 0 or 1).
     * @param targetAddress The I2C address of the clock to send the command to.
     * @param withRetry If true, the command will be retried on failure.
     * @return true on success, false on failure.
     */
    bool sendDGTCommand(const char* name, uint8_t* cmd, int length,
                       uint8_t ackListenAddress, uint8_t expectedAckCmd, int numAcks = 1,
                       uint8_t targetAddress = DGT3000_I2C_ADDRESS, bool withRetry = true);

    /**
     * @brief Switches the slave I2C to listen for time and button messages (address 0x00).
     */
    void listenForTimeMessages();

    /**
     * @brief Switches the slave I2C to listen for ACK messages (address 0x10).
     */
    void listenForAckMessages();

private:
    // Internal state
    bool _initialized;              ///< True if begin() has been called successfully.
    bool _connected;                ///< True if communication with the clock is active.
    bool _configured;               ///< True if the clock has been configured for central control.
    int _lastError;                 ///< Stores the last error code.
    uint8_t _currentListenAddress;  ///< Tracks the current I2C address the slave is listening on.
    bool _recoveryInProgress;       ///< Flag to prevent recursive recovery attempts.

    // Communication pins
    int _masterSDA;
    int _masterSCL;
    int _slaveSDA;
    int _slaveSCL;

    // I2C instances
    TwoWire* _i2cMaster;
    TwoWire* _i2cSlave;

    // ACK tracking
    volatile uint8_t _receivedAckCmd;
    volatile bool _newAckReceived;
    volatile bool _newPingResponseReceived;

    // Data event tracking
    volatile bool _newTimeAvailable;

    // Internal data structure for received clock data
    struct {
        uint8_t time[6];                                    ///< Current clock time [L_H, L_M, L_S, R_H, R_M, R_S].
        uint8_t buttonState;                                ///< Current raw button state from the last message.
        uint8_t lastButtonState;                            ///< Previous raw button state for change detection.
        uint8_t buttonBuffer[DGT3000_BUTTON_BUFFER_SIZE];   ///< Circular buffer for button press events.
        int buttonStart;                                    ///< Start index of the button buffer.
        int buttonEnd;                                      ///< End index of the button buffer.
    } _rxData;

    // Slave I2C management
    void setSlaveListenAddress(uint8_t address);

    // I2C message processing
    void onSlaveReceive(int bytesReceived);
    static void onSlaveReceiveStatic(int bytesReceived);
    void processAckMessage(uint8_t* buffer, uint8_t length);
    void processPingResponseMessage(uint8_t* buffer, uint8_t length);
    void processTimeMessage(uint8_t* buffer, uint8_t length);
    void processButtonMessage(uint8_t* buffer, uint8_t length);
    bool waitForAck(uint8_t expectedCmd, uint32_t timeout_ms = DGT3000_ACK_TIMEOUT_MS);

    // Internal helper methods
    void resetRxData();
    void addButtonEvent(uint8_t button);
    bool isButtonBufferFull();

    // Timeout helper
    bool isTimeout(uint32_t startTime, uint32_t timeout_ms);

    // Static instance for I2C callback
    static DGT3000* _instance;
};

#endif // DGT3000_H
