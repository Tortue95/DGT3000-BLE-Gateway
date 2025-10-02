/*
 * LED Manager for DGT3000 Gateway
 *
 * This header defines a manager for controlling the status LED (NeoPixel)
 * to provide visual feedback on the gateway's state.
 * 
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Adafruit_NeoPixel.h>
#include <logging.hpp>
#include "00-GatewayConstants.h"

/**
 * @enum LedState
 * @brief Defines the visual states of the status LED.
 */
enum LedState {
    LED_STATE_INITIALIZING,           ///< System is starting up. LED is off.
    LED_STATE_WAITING_FOR_CONNECTION, ///< Waiting for a BLE client. LED is slow blinking blue.
    LED_STATE_CLIENT_CONNECTED,       ///< A BLE client is connected. LED is solid blue.
    LED_STATE_DGT_CONFIGURED,         ///< DGT clock is connected and configured. LED is solid green.
    LED_STATE_OFF                     ///< LED is turned off.
};

/**
 * @class LedManager
 * @brief Manages the behavior of the status LEDs (NeoPixel and/or simple LED).
 */
class LedManager : public esp32m::SimpleLoggable {
public:
    /**
     * @brief Constructs a new LedManager.
     * Configuration is read from 00-GatewayConstants.h.
     */
    LedManager();
    
    /**
     * @brief Initializes the LED hardware (NeoPixel and/or simple LED).
     */
    void initialize();
    
    /**
     * @brief Sets a new state for the LED.
     * @param new_state The LedState to transition to.
     */
    void setState(LedState new_state);
    
    /**
     * @brief Gets the current state of the LED.
     * @return The current LedState.
     */
    LedState getState() const;
    
    /**
     * @brief Updates the LED's color or blink status based on the current state.
     * This should be called periodically in the main loop.
     */
    void update();

private:
    // NeoPixel members
    Adafruit_NeoPixel* pixels; ///< The NeoPixel driver instance.

    // Simple LED members
    bool simple_led_on;                 ///< Current on/off state for the simple LED.
    int simple_led_connected_duty_cycle;///< Calculated duty cycle for solid brightness in connected state.
    int simple_led_blink_duty_cycle;    ///< Calculated duty cycle for blinking brightness.

    // General members
    LedState current_state;   ///< The current state of the LED system.
    
    // Blink state members
    unsigned long neopixel_last_update; ///< Timestamp of the last NeoPixel blink toggle.
    bool neopixel_blink_status;         ///< The current on/off status for NeoPixel blinking.
    unsigned long simple_led_last_update; ///< Timestamp of the last simple LED blink toggle.

    /**
     * @brief Updates the NeoPixel based on the current state.
     */
    void updateNeoPixel();

    /**
     * @brief Updates the simple LED based on the current state.
     */
    void updateSimpleLed();
};

#endif // LED_MANAGER_H
