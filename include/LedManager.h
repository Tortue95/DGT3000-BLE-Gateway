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
 * @brief Manages the behavior of the NeoPixel status LED.
 */
class LedManager : public esp32m::SimpleLoggable {
public:
    /**
     * @brief Constructs a new LedManager.
     * @param pin The GPIO pin the NeoPixel is connected to.
     * @param num_pixels The number of pixels in the strip (usually 1).
     */
    LedManager(int pin = LED_NEOPIXEL_PIN, int num_pixels = 1);
    
    /**
     * @brief Initializes the NeoPixel library.
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
    Adafruit_NeoPixel pixels; ///< The NeoPixel driver instance.
    LedState current_state;   ///< The current state of the LED.
    unsigned long last_update;///< Timestamp of the last blink toggle.
    bool blink_status;        ///< The current on/off status for blinking.

    /**
     * @brief Sets the physical color of the NeoPixel.
     * @param color The 24-bit color value (e.g., 0x0000FF for blue).
     */
    void setPixelColor(uint32_t color);
};

#endif // LED_MANAGER_H
