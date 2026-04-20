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
#define BLINK_INTERVAL_SLOW 600 // Slow blink interval for simple LED (ms)
#define BLINK_INTERVAL_FAST 190  // Fast blink interval for simple LED (ms)

// Define colors in GRB format (as required by NeoPixel library)
static constexpr uint32_t COLOR_ORANGE= 0xFF3000;
static constexpr uint32_t COLOR_BLUE  = 0x0000FF;
static constexpr uint32_t COLOR_GREEN = 0x00FF00;
static constexpr uint32_t COLOR_OFF   = 0x000000;

// PWM constants for Simple LED
static constexpr int SIMPLE_LED_PWM_CHANNEL = 0;
static constexpr int SIMPLE_LED_PWM_FREQ = 5000;
static constexpr int SIMPLE_LED_PWM_RESOLUTION = 8;

LedManager::LedManager()
    : SimpleLoggable("led"),
      pixels(nullptr),
      simple_led_on(false),
      simple_led_connected_duty_cycle(0),
      simple_led_blink_duty_cycle(0),
      current_state(LED_STATE_INITIALIZING),
      neopixel_last_update(0),
      neopixel_blink_status(false),
      simple_led_last_update(0) {}

void LedManager::initialize() {
    if (NEOPIXEL_LED_ENABLED) {
        pixels = new Adafruit_NeoPixel(1, LED_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
        if (pixels) {
            pixels->begin();
            pixels->setBrightness(NEOPIXEL_GENERAL_BRIGHTNESS);
            logI("NeoPixel LED enabled on pin %d", LED_NEOPIXEL_PIN);
        } else {
            logE("Failed to allocate NeoPixel memory");
        }
    } else {
        logI("NeoPixel LED is disabled.");
    }

    if (SIMPLE_LED_ENABLED) {
        // Configure PWM for the simple LED
        ledcSetup(SIMPLE_LED_PWM_CHANNEL, SIMPLE_LED_PWM_FREQ, SIMPLE_LED_PWM_RESOLUTION);
        ledcAttachPin(SIMPLE_LED_PIN, SIMPLE_LED_PWM_CHANNEL);

        // Calculate duty cycle from percentage for the connected state
        simple_led_connected_duty_cycle = (CONNECTED_STATE_BRIGHTNESS_PERCENT * 255) / 100;
        if (simple_led_connected_duty_cycle < 0) simple_led_connected_duty_cycle = 0;
        if (simple_led_connected_duty_cycle > 255) simple_led_connected_duty_cycle = 255;

        // Calculate duty cycle for blinking
        simple_led_blink_duty_cycle = (SIMPLE_LED_BLINK_BRIGHTNESS_PERCENT * 255) / 100;
        if (simple_led_blink_duty_cycle < 0) simple_led_blink_duty_cycle = 0;
        if (simple_led_blink_duty_cycle > 255) simple_led_blink_duty_cycle = 255;

        logI("Simple LED enabled on pin %d", SIMPLE_LED_PIN);
    } else {
        logI("Simple LED is disabled.");
    }
    
    setState(LED_STATE_DGT_CONNECTING);
    logD("LED Manager initialized");
}

void LedManager::setState(LedState new_state) {
    if (new_state != current_state) {
        logD("LED State changing from %d to %d", current_state, new_state);
        current_state = new_state;
        // Reset timers and blink statuses on state change
        neopixel_last_update = 0;
        simple_led_last_update = 0;
        neopixel_blink_status = false;
        simple_led_on = false;
        update(); // Apply the new state immediately.
    }
}

LedState LedManager::getState() const {
    return current_state;
}

void LedManager::update() {
    if (NEOPIXEL_LED_ENABLED && pixels) {
        updateNeoPixel();
    }
    if (SIMPLE_LED_ENABLED) {
        updateSimpleLed();
    }
}

void LedManager::updateNeoPixel() {
    unsigned long current_millis = millis();
    uint32_t color = COLOR_OFF;

    switch (current_state) {
        case LED_STATE_DGT_CONNECTING:
            // Fast blinking orange
            if (current_millis - neopixel_last_update > BLINK_INTERVAL_FAST) {
                neopixel_last_update = current_millis;
                neopixel_blink_status = !neopixel_blink_status;
            }
            color = neopixel_blink_status ? COLOR_ORANGE : COLOR_OFF;
            break;
        
        case LED_STATE_DGT_CONNECTED_BLE_WAITING:
            // Slow blinking blue
            if (current_millis - neopixel_last_update > BLINK_INTERVAL_SLOW) {
                neopixel_last_update = current_millis;
                neopixel_blink_status = !neopixel_blink_status;
            }
            color = neopixel_blink_status ? COLOR_BLUE : COLOR_OFF;
            break;

        case LED_STATE_CLIENT_CONNECTED:
            {
                // Solid green with adjusted brightness
                uint8_t green_value = 255 * (static_cast<float>(CONNECTED_STATE_BRIGHTNESS_PERCENT) / 100.0f);
                color = pixels->Color(0, green_value, 0);
            }
            break;

        case LED_STATE_INITIALIZING:
        case LED_STATE_OFF:
        default:
            // LED off
            color = COLOR_OFF;
            break;
    }

    if (pixels->getPixelColor(0) != color) {
        pixels->setPixelColor(0, color);
        pixels->show();
    }
}

void LedManager::updateSimpleLed() {
    unsigned long current_millis = millis();
    int blink_interval = 0;

    switch (current_state) {
        case LED_STATE_DGT_CONNECTING:
            // Fast blinking
            blink_interval = BLINK_INTERVAL_FAST;
            break;

        case LED_STATE_DGT_CONNECTED_BLE_WAITING:
            // Slow blinking
            blink_interval = BLINK_INTERVAL_SLOW;
            break;

        case LED_STATE_CLIENT_CONNECTED:
            // Solid on with specified brightness
            ledcWrite(SIMPLE_LED_PWM_CHANNEL, simple_led_connected_duty_cycle);
            return; // Exit, no blinking logic needed

        case LED_STATE_INITIALIZING:
        case LED_STATE_OFF:
        default:
            // Turn the LED off
            ledcWrite(SIMPLE_LED_PWM_CHANNEL, 0);
            return; // Exit, no blinking logic needed
    }

    // Handle blinking logic
    if (blink_interval > 0) {
        if (current_millis - simple_led_last_update > blink_interval) {
            simple_led_last_update = current_millis;
            simple_led_on = !simple_led_on;
            ledcWrite(SIMPLE_LED_PWM_CHANNEL, simple_led_on ? simple_led_blink_duty_cycle : 0);
        }
    }
}
