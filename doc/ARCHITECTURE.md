# DGT3000 BLE Gateway - System Architecture and Behavior

This document provides a detailed explanation of the internal workings, lifecycle, and architectural decisions of the DGT3000 BLE Gateway firmware.

## 1. Core Architecture

The gateway's firmware is built on a dual-core architecture using FreeRTOS tasks to ensure stable and non-blocking operation.

-   **Core 1 (Arduino Loop)**: This core runs the main `loop()` function. It is responsible for managing the Bluetooth Low Energy (BLE) stack, processing incoming connections, and handling the event/response queues. Separating BLE onto its own core prevents time-sensitive I2C operations from being interrupted by the radio stack.

-   **Core 0 (I2C Task)**: A dedicated FreeRTOS task, `I2CTaskManager`, is pinned to Core 0. This task's sole responsibility is to manage all communication with the DGT3000 clock via the dual-I2C interface. This isolation guarantees that I2C timings are precise and not affected by other system activities.

-   **Queue System**: Communication between the two cores is handled safely using a system of FreeRTOS queues managed by `QueueManager`. This prevents race conditions and ensures that data (commands, events, responses) is passed between tasks in an orderly fashion.

## 2. Connection and Power Lifecycle

The gateway is designed to be power-efficient and responsive, managing the DGT3000's power state based on the BLE connection status.

### On BLE Client Connect

1.  A BLE client connects to the gateway.
2.  The `onBLEConnected()` callback is triggered.
3.  This immediately initiates the `initializeDGT3000()` sequence in the `I2CTaskManager`.
4.  The gateway sends a "wake-up" signal to the DGT3000 clock. **Even if the clock is physically off, the gateway can power it on** via the I2C communication lines.
5.  The gateway then runs the full configuration sequence to take central control of the clock.

### On BLE Client Disconnect

1.  The BLE client disconnects.
2.  The `onBLEDisconnected()` callback is triggered.
3.  The gateway sends a **power-off command** to the DGT3000 clock.
4.  After cleaning up resources, the **ESP32 automatically reboots**. This ensures the system returns to a clean, predictable state, ready for a new connection.

This lifecycle ensures that the DGT3000 clock is only powered on when a client is actively connected, saving battery on the clock.

## 3. Automatic I2C Reconnection

The gateway is resilient to physical connection interruptions between the ESP32 and the DGT3000 (e.g., if the cable is unplugged and re-plugged).

1.  **Error Detection**: If an I2C communication command fails for any reason (timeout, CRC error, no ACK), the `I2CTaskManager` detects the error.
2.  **State Change**: The gateway marks the DGT3000 connection state as `DISCONNECTED` and sends a `connectionStatus` event to the BLE client to inform it of the issue.
3.  **Recovery Loop**: The `I2CTaskManager`'s `monitorConnection()` function enters a recovery mode. It will periodically (every few seconds) attempt to re-run the full `configureDGT3000()` sequence.
4.  **Re-establishment**: If the physical connection is restored (e.g., the cable is plugged back in), one of the recovery attempts will succeed. The gateway will re-establish control of the clock, mark the state as `CONNECTED` and `CONFIGURED`, and send a new, positive `connectionStatus` event to the client.

This allows for hot-plugging the DGT3000 clock while a BLE client remains connected.

## 4. Status LED Behavior

The onboard NeoPixel LED provides at-a-glance visual feedback of the gateway's current status:

-   **Slow Blinking Blue**: The gateway is powered on and advertising, waiting for a BLE client to connect.
-   **Solid Blue**: A BLE client has connected, but the gateway has not yet successfully connected to and configured the DGT3000 clock.
-   **Solid Green**: All systems are go. A BLE client is connected, and the gateway has full control over the DGT3000 clock. The system is ready for commands.

This simple color-coding allows for quick and easy troubleshooting of the system's state.

## 5. Known Limitations

### Lever Position Inversion at Startup

The DGT3000 clock's "lever left" (`0xC0`) and "lever right" (`0x40`) event codes can sometimes appear inverted depending on the physical position of the lever when the DGT3000 clock is powered on. For example, if the DGT3000 is powered on with the lever physically to the left, the event code `0x40` (typically "right") might be reported when the lever is physically on the left, and `0xC0` (typically "left") when it's on the right.

This behavior is observed in the underlying DGT3000 communication protocol and is not currently compensated for within this gateway's firmware. The gateway reports the raw event codes as received from the DGT3000. Developers consuming these events from the gateway should be aware of this potential inversion and implement any necessary interpretation adjustments on their client-side application if precise physical lever orientation is critical. At present, there is no known method to reliably determine the true physical lever position at DGT3000 startup directly from the clock's initial messages.
