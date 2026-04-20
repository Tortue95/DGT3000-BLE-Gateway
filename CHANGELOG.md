# Changelog

All notable changes to this project will be documented in this file.

## [0.6beta] - 2026-04-20

### ⚠️ Protocol Update
- **BLE Protocol Version 1.1**: Major update to the communication protocol required to fix an issue with clock initialization. Clients must be updated to support this version.

### Fixed
- **Clock Mode 0 Resets**: Fixed a critical bug where the clock times were reset to zero when starting (`run`) if the modes were set to 0. This was caused by the gateway overwriting clock values before synchronization was complete. The fix ensures internal states are updated immediately upon `set_time` or `set_and_run`.

### Changed
- **LED Visual Feedback**: The DGT3000 connection status LED now uses **fast blinking orange** (instead of blue) to clearly indicate the gateway is waiting for a clock connection.
- **Test Client**: Updated `dgt3000_ble_client.py` to support and enforce protocol version `1.1`.
- **Version Bumps**: Gateway app version moved to `0.6beta` and DGT3000 library to `1.0.1`.

### Added
- Created this `CHANGELOG.md` to track project evolution.
