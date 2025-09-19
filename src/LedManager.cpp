/*
 * LED Manager Implementation for DGT3000 Gateway
 *
 * This file implements the logic for controlling the status LED.
 *
 * Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "LedManager.h"

using namespace esp32m;

// Constants for LED behavior
#define BLINK_INTERVAL 500 // Blink interval in milliseconds

// Define colors in GRB format (as required by NeoPixel library)
static constexpr uint32_t COLOR_BLUE  = 0x0000FF;
static constexpr uint32_t COLOR_GREEN = 0x002200;
static constexpr uint32_t COLOR_OFF   = 0x000000;

LedManager::LedManager(int pin, int num_pixels)
    : SimpleLoggable("led"),
      pixels(num_pixels, pin, NEO_GRB + NEO_KHZ800),
      current_state(LED_STATE_INITIALIZING),
      last_update(0),
      blink_status(false) {}

void LedManager::initialize() {
    pixels.begin();
    pixels.setBrightness(50); // Set a moderate brightness to avoid being too bright.
    setState(LED_STATE_WAITING_FOR_CONNECTION);
    logD("LED Manager initialized");
}

void LedManager::setState(LedState new_state) {
    if (new_state != current_state) {
        logD("LED State changing from %d to %d", current_state, new_state);
        current_state = new_state;
        last_update = 0;    // Reset blink timer on state change.
        blink_status = false; // Ensure blink starts from a known state.
        update();           // Apply the new state immediately.
    }
}

LedState LedManager::getState() const {
    return current_state;
}

void LedManager::update() {
    unsigned long current_millis = millis();

    switch (current_state) {
        case LED_STATE_WAITING_FOR_CONNECTION:
            // Slow blinking blue to indicate waiting for a BLE client.
            if (current_millis - last_update > BLINK_INTERVAL) {
                last_update = current_millis;
                blink_status = !blink_status;
                setPixelColor(blink_status ? COLOR_BLUE : COLOR_OFF);
            }
            break;

        case LED_STATE_CLIENT_CONNECTED:
            // Solid blue when a BLE client is connected but DGT is not yet ready.
            setPixelColor(COLOR_BLUE);
            break;

        case LED_STATE_DGT_CONFIGURED:
            // Solid green when both BLE and DGT clock are connected and configured.
            setPixelColor(COLOR_GREEN);
            break;

        case LED_STATE_INITIALIZING:
        case LED_STATE_OFF:
        default:
            // Turn the LED off in default or off states.
            setPixelColor(COLOR_OFF);
            break;
    }
}

void LedManager::setPixelColor(uint32_t color) {
    pixels.setPixelColor(0, color);
    pixels.show();
}
