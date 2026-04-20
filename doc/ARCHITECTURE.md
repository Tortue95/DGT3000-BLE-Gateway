# DGT3000 BLE Gateway - System Architecture and Behavior

This document provides a detailed explanation of the internal workings, lifecycle, and architectural decisions of the DGT3000 BLE Gateway firmware.

## 1. Core Architecture

The gateway's firmware is built on a dual-core architecture using FreeRTOS tasks to ensure stable and non-blocking operation.

-   **Core 1 (Arduino Loop)**: This core runs the main `loop()` function. It is responsible for managing the Bluetooth Low Energy (BLE) stack, processing incoming connections, and handling the event/response queues. Separating BLE onto its own core prevents time-sensitive I2C operations from being interrupted by the radio stack.

-   **Core 0 (I2C Task)**: A dedicated FreeRTOS task, `I2CTaskManager`, is pinned to Core 0. This task's sole responsibility is to manage all communication with the DGT3000 clock via the dual-I2C interface. This isolation guarantees that I2C timings are precise and not affected by other system activities.

-   **Queue System**: Communication between the two cores is handled safely using a system of FreeRTOS queues managed by `QueueManager`. This prevents race conditions and ensures that data (commands, events, responses) is passed between tasks in an orderly fashion.

## 2. Connection and Power Lifecycle

The gateway follows a specific startup and connection sequence to ensure that it is only discoverable by clients when it is fully operational.

1.  **Power On & DGT Connection**:
    *   Upon booting, the ESP32 immediately enters a loop where it continuously tries to connect to and configure the DGT3000 clock.
    *   During this phase, **BLE advertising is disabled**. The gateway is not visible to BLE clients.
    *   The gateway can power on the DGT3000 clock via the I2C lines, even if the clock is physically off.

2.  **DGT Connected & Waiting for Client**:
    *   Once the gateway successfully connects to and configures the DGT3000, it starts BLE advertising.
    *   The gateway is now discoverable as `DGT3000-Gateway` and is ready to accept a client connection.

3.  **BLE Client Connects**:
    *   A BLE client connects to the gateway.
    *   The system is now fully active and ready to process commands.

4.  **BLE Client Disconnects**:
    *   When the client disconnects, the gateway sends a power-off command to the DGT3000.
    *   The **ESP32 automatically reboots**. This ensures the system returns to a clean, predictable state, ready for the next session, starting again at step 1.

This lifecycle ensures that a client can only connect when the gateway has a valid, active connection to the DGT3000, preventing connection errors and improving user experience.

## 3. Automatic I2C Reconnection

The gateway is resilient to physical connection interruptions between the ESP32 and the DGT3000 (e.g., if the cable is unplugged and re-plugged).

1.  **Error Detection**: If an I2C communication command fails for any reason (timeout, CRC error, no ACK), the `I2CTaskManager` detects the error.
2.  **State Change**: The gateway marks the DGT3000 connection state as `DISCONNECTED` and sends a `connectionStatus` event to the BLE client to inform it of the issue.
3.  **Recovery Loop**: The `I2CTaskManager`'s `monitorConnection()` function enters a recovery mode. It will periodically (every few seconds) attempt to re-run the full `configureDGT3000()` sequence.
4.  **Re-establishment**: If the physical connection is restored (e.g., the cable is plugged back in), one of the recovery attempts will succeed. The gateway will re-establish control of the clock, mark the state as `CONNECTED` and `CONFIGURED`, and send a new, positive `connectionStatus` event to the client.

This allows for hot-plugging the DGT3000 clock while a BLE client remains connected.

## 4. Status LED Behavior

The firmware provides two configurable options for visual status feedback, which can be enabled or disabled independently in `include/00-GatewayConstants.h`.

### Onboard NeoPixel (ESP32-S3-Zero)

The onboard NeoPixel LED provides at-a-glance, color-coded feedback of the gateway's current status:

-   **Fast Blinking Orange**: The gateway is trying to connect to the DGT3000 clock. BLE advertising is off.
-   **Slow Blinking Blue**: The gateway is connected to the DGT3000 and is now advertising, waiting for a BLE client to connect.
-   **Solid Green (Brightness-Adjustable)**: All systems are go. A BLE client is connected, and the gateway has full control over the DGT3000 clock. The system is ready for commands.

### Optional External LED

For custom enclosures or different hardware, a simple, single-color external LED can be used (defaulting to GPIO 11). It provides status information through different blinking patterns:

-   **Fast Blinking**: The gateway is trying to connect to the DGT3000 clock.
-   **Slow Blinking**: The gateway is connected to the DGT3000 and is waiting for a BLE client.
-   **Solid On (Brightness-Adjustable)**: The gateway is fully connected to both the BLE client and the DGT clock.

This dual-LED approach allows for both easy-to-read color-coding with the NeoPixel and flexible integration with a standard external LED.

## 5. Known Limitations

### Lever Position Inversion at Startup

The DGT3000 clock's "lever left" (`0xC0`) and "lever right" (`0x40`) event codes can sometimes appear inverted depending on the physical position of the lever when the DGT3000 clock is powered on. For example, if the DGT3000 is powered on with the lever physically to the left, the event code `0x40` (typically "right") might be reported when the lever is physically on the left, and `0xC0` (typically "left") when it's on the right.

This behavior is observed in the underlying DGT3000 communication protocol and is not currently compensated for within this gateway's firmware. The gateway reports the raw event codes as received from the DGT3000. Developers consuming these events from the gateway should be aware of this potential inversion and implement any necessary interpretation adjustments on their client-side application if precise physical lever orientation is critical. At present, there is no known method to reliably determine the true physical lever position at DGT3000 startup directly from the clock's initial messages.
