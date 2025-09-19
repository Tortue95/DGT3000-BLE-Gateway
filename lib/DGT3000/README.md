# DGT3000 Library for ESP32

An ESP32 library for communicating with DGT3000 chess clocks using dual I2C communication. This library provides a high-level interface to control the clock's timers, display, and buttons.

Tested on ESP32-S3-Zero.

## Features

- **Dual I2C Communication**: Utilizes separate I2C buses for sending and receiving data, as required by the DGT3000 protocol.
- **Dynamic Address Switching**: Automatically handles switching between I2C slave addresses (0x00 for data, 0x10 for acknowledgments).
- **Automatic CRC Calculation**: Manages CRC-8-ATM checksums for all outgoing commands.
- **Timer Control**: Start, stop, and set custom times for both players with various run modes (count down, count up).
- **Text Display**: Display custom text on the clock's screen, with options for beeps and status icons (e.g., king, flag).
- **Button & Lever Handling**: Captures and buffers all button presses and lever movements as distinct events.
- **Power Management**: Power the clock on and off programmatically.
- **Auto-Recovery**: Includes a configuration sequence to wake the clock and take control.

## Installation

### Arduino IDE
1. Download this repository as a ZIP file.
2. In the Arduino IDE, go to `Sketch` -> `Include Library` -> `Add .ZIP Library...` and select the downloaded file.

### PlatformIO
1. Place the `DGT3000` folder into the `lib` directory of your PlatformIO project.
2. PlatformIO will automatically detect and use the library.

## Hardware Connection

The library requires a dual I2C setup. Connect your ESP32 to the DGT3000 clock's I2C port.

**Default Pin Configuration:**
- **Master I2C (ESP32 -> Clock)**
  - `SDA`: GPIO 8
  - `SCL`: GPIO 5
- **Slave I2C (Clock -> ESP32)**
  - `SDA`: GPIO 7
  - `SCL`: GPIO 6

You can specify custom pins in the `dgt.begin()` method.

## Basic Usage

Here is a simple example of how to initialize the library, configure the clock, and display a message.

```cpp
#include <DGT3000.h>

DGT3000 dgt;

void setup() {
    Serial.begin(115200);
    
    // Initialize with default pins.
    // You can also specify custom pins: dgt.begin(SDA_M, SCL_M, SDA_S, SCL_S);
    if (dgt.begin()) {
        Serial.println("DGT3000 initialized.");
        
        // The configure() method performs the necessary handshake to take control of the clock.
        if (dgt.configure()) {
            Serial.println("DGT3000 configured and ready.");
            
            // Example 1: Display text
            dgt.displayText("Hello World!");
            delay(2000);

            // Example 2: Set timers to 5 minutes each and stop them.
            dgt.setAndRun(
                DGT_MODE_COUNT_DOWN, 0, 5, 0,  // Left player: 0h 5m 0s, counting down
                DGT_MODE_COUNT_DOWN, 0, 5, 0   // Right player: 0h 5m 0s, counting down
            );
        } else {
            Serial.print("DGT3000 configuration failed. Error: ");
            Serial.println(dgt.getErrorString(dgt.getLastError()));
        }
    } else {
        Serial.println("DGT3000 initialization failed.");
    }
}

void loop() {
    // The library uses an I2C slave callback to process incoming data,
    // so no periodic polling is required in the main loop for time or button updates.
    
    uint8_t button_event;
    if (dgt.getButtonEvent(&button_event)) {
        Serial.print("Button event received: 0x");
        Serial.println(button_event, HEX);
    }
    
    delay(100);
}
```

## API Overview

For a detailed API reference, please see the Doxygen-style comments in `DGT3000.h`.

- `dgt.configure()`: Establishes connection and takes control.
- `dgt.displayText(...)`: Shows text on the screen.
- `dgt.setAndRun(...)`: Sets time and mode for both players.
- `dgt.stop()` / `dgt.run(...)`: Controls the timers.
- `dgt.getTime(...)`: Fetches the current time for both sides.
- `dgt.getButtonEvent(...)`: Retrieves the next button event from the queue.
- `dgt.getButtonState()`: Gets the instantaneous state of all buttons.
- `dgt.powerOff()`: Turns the clock off.
