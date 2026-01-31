# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO-based Arduino project for water tank monitoring using dual connectivity: LoRaWAN (primary) and WiFi (backup). The project is designed for Arduino R4 WiFi boards with SX1276 LoRaWAN shield and includes a native testing framework.

**Current Status**: Fully configured with LoRaWAN support. The main sketch is at the root level for easy Arduino IDE access, with PlatformIO build support via src/main.cpp.

## Development Commands

### Building and Uploading
```bash
# Build the project
pio run

# Upload to Arduino board
pio run --target upload

# Build for specific environment
pio run -e uno_r4_wifi

# Clean build files
pio run --target clean
```

### Testing
```bash
# Run all native tests
pio test -e native

# Run tests with verbose output
pio test -e native -v

# Run specific test
pio test -e native --filter test_name
```

### Monitoring
```bash
# Open serial monitor (115200 baud)
pio device monitor

# Monitor with custom baud rate
pio device monitor -b 115200
```

## Architecture

### Project Structure

- `water_tank_monitor.ino` - **Primary Arduino sketch** (root level for Arduino IDE compatibility)
- `src/main.cpp` - PlatformIO build source (copy of .ino file)
- `test/` - Native unit tests (runs on host machine, not Arduino)
  - `test_main.cpp` - Test entry point (needs implementation)
  - `test_functions.cpp` / `test_functions.h` - Test implementations (needs implementation)
  - `mocks/` - Mock implementations of Arduino APIs for native testing
    - `Arduino.h` - Mock Arduino core functions
    - `WiFiS3.h` - Mock WiFi library for Arduino R4 WiFi
    - `mocks.cpp` - Mock implementations
- `docs/` - Documentation
  - `CLAUDE.md` - This file
  - `LORAWAN_SETUP.md` - LoRaWAN configuration guide
- `platformio.ini` - PlatformIO configuration

### Core Functionality

The main sketch (`water_tank_monitor.ino`) implements:

1. **Sensor Reading**: Reads analog voltage from A0 pin, averages 10 samples to reduce noise
2. **Pressure Calculation**: Converts voltage (0.5-4.5V range) to pressure (0-10 kPa)
3. **Depth Calculation**: Converts pressure to water depth using hydrostatic formula (1 kPa ≈ 0.102m water)
4. **Volume Calculation**: Calculates volume in liters based on cylindrical tank geometry (100mm diameter)
5. **LoRaWAN Connectivity**: Primary data transmission using OTAA on AU915 FSB2
6. **WiFi Connectivity**: Backup connectivity with automatic reconnection
7. **Data Upload**:
   - LoRaWAN: Every 60 seconds (binary payload, 8 bytes) - respects duty cycle
   - WiFi: Every 5 seconds (HTTP GET to local server)

Key utility functions to test:
- `clampf()` - Clamps float value between min/max
- `voltageToKpa()` - Converts sensor voltage to pressure
- `readA0VoltageAveraged()` - Reads and averages analog sensor values

### Testing Approach

The project uses PlatformIO's native testing environment, which allows tests to run on the development machine rather than requiring an Arduino board. The `test/mocks/` directory should provide mock implementations of Arduino and WiFi APIs to enable this native testing.

When writing tests:
- Include mock headers instead of real Arduino libraries
- Mock `analogRead()`, `delay()`, `millis()`, and other Arduino core functions
- Mock WiFi connectivity and HTTP client functionality
- Focus on testing calculation logic (`voltageToKpa`, `clampf`) without hardware dependencies
- Use Unity test framework assertions (`TEST_ASSERT_EQUAL`, `TEST_ASSERT_FLOAT_WITHIN`, etc.)

### LoRaWAN Configuration

**Region**: AU915 (Australia/New Zealand)
**Channel Plan**: FSB2 (Frequency Sub-Band 2)
- Uplink channels: 8-15 (916.8-918.2 MHz)
- 500kHz channel: 65 (917.5 MHz)

**Gateway EUI**: `2CF7F1177440004B`

**OTAA Credentials** (must be configured in code):
- Application EUI (AppEUI/JoinEUI) - 8 bytes, LSB format
- Device EUI (DevEUI) - 8 bytes, LSB format
- Application Key (AppKey) - 16 bytes, MSB format

**Pin Mapping** (SX1276 shield):
- NSS (CS): Pin 10
- RST: Pin 9
- DIO0: Pin 2
- DIO1: Pin 3

**Payload Format** (8 bytes, big-endian):
- Bytes 0-1: Voltage (mV) - divide by 1000 for volts
- Bytes 2-3: Pressure (centi-kPa) - divide by 100 for kPa
- Bytes 4-5: Depth (mm) - divide by 1000 for meters
- Bytes 6-7: Volume (centi-liters) - divide by 100 for liters

Decoder example (JavaScript):
```javascript
function Decoder(bytes, port) {
  var voltage = ((bytes[0] << 8) | bytes[1]) / 1000.0;
  var pressure = ((bytes[2] << 8) | bytes[3]) / 100.0;
  var depth = ((bytes[4] << 8) | bytes[5]) / 1000.0;
  var volume = ((bytes[6] << 8) | bytes[7]) / 100.0;
  return {
    voltage: voltage,
    pressure_kpa: pressure,
    depth_m: depth,
    volume_liters: volume
  };
}
```

### Configuration Requirements

The `platformio.ini` file needs to define:
- `[env:uno_r4_wifi]` - Arduino UNO R4 WiFi board configuration
- `[env:native]` - Native testing environment with Unity framework
- Include paths for test mocks
- Library dependencies:
  - Arduino WiFiS3 library
  - MCCI LoRaWAN LMIC library (for LoRaWAN support)

## Key Considerations

- Target board: Arduino UNO R4 WiFi (uses WiFiS3 library, not standard WiFi library)
- LoRa shield: SX1276-based module configured for AU915 region
- Serial communication at 115200 baud
- Analog reference voltage: 5.0V with 10-bit ADC (0-1023)
- **CRITICAL**: `os_runloop_once()` must be called frequently in main loop for LoRaWAN to function
- LoRaWAN uses OTAA (Over-The-Air Activation) - credentials must be configured before use
- LoRaWAN transmission interval: 60 seconds (respects duty cycle limits)
- WiFi transmission interval: 5 seconds (backup/local monitoring)
- WiFi credentials are hardcoded in sketch (consider moving to separate config file)
- Server endpoint hardcoded (consider making configurable)
- LoRaWAN payload is compact binary format (8 bytes) to minimize airtime
- FSB2 channel configuration is required for AU915 - only channels 8-15 and 65 are enabled
- Tests run natively (not on device) using Unity test framework
- Mock implementations required for Arduino.h, WiFiS3.h, and LMIC library to enable native testing
