# Water Tank Monitor - Unit Tests

This directory contains comprehensive unit tests for the water tank monitoring system.

## Test Coverage

The test suite covers the following functions with multiple test cases:

### 1. `clampf()` - Float Clamping Function
- Values below minimum bound
- Values above maximum bound
- Values within range
- Boundary values (min and max)
- Negative ranges
- Very small numbers

### 2. `voltageToKpa()` - Voltage to Pressure Conversion
- Minimum voltage (0.50V → 0 kPa)
- Maximum voltage (4.50V → 10.0 kPa)
- Mid-point voltage (2.50V → 5.0 kPa)
- Values below minimum (clamping)
- Values above maximum (clamping)
- Quarter and three-quarter range values
- Negative voltage handling

### 3. `readA0VoltageAveraged()` - Analog Reading with Averaging
- Zero ADC reading
- Maximum ADC reading (1023 → 5.0V)
- Mid-point reading
- Quarter reading
- Range validation (0V to 5V)
- Verification of 10-sample averaging with timing

### 4. `connectWiFi()` - WiFi Connection Management
- Already connected scenario (early return)
- Successful connection
- Connection timeout behavior (15 seconds)
- Timeout boundary enforcement
- WiFi.begin() call verification

### 5. `uploadToServer()` - HTTP Data Upload
- WiFi not connected (early return)
- Server connection failure
- Successful connection and request sending
- Parameter formatting (depth, pressure, volume)
- 5-second response timeout
- Negative value handling
- HTTP GET request structure

## Running the Tests

### Prerequisites
Install PlatformIO if not already installed:
```bash
pip install platformio
```

### Run Tests
From the project root directory:
```bash
pio test -e native
```

This will compile and run all unit tests using the native platform (runs on your local machine, not on Arduino hardware).

### Verbose Output
For detailed test output:
```bash
pio test -e native -v
```

### Run Specific Tests
To run tests matching a pattern:
```bash
pio test -e native --filter "test_clampf*"
```

## Test Structure

- `test_main.cpp` - Main test file with all test cases
- `test_functions.h` - Header file declaring functions under test
- `test_functions.cpp` - Implementation of functions extracted from .ino file
- `mocks/Arduino.h` - Mock Arduino framework functions
- `mocks/WiFiS3.h` - Mock WiFi library
- `mocks/mocks.cpp` - Mock implementations with controllable behavior

## Mock System

The test framework uses mocks to simulate Arduino hardware and WiFi behavior:

### Mock Control Functions
- `mock_set_millis(value)` - Control time progression
- `mock_set_wifi_status(status)` - Simulate WiFi connection state
- `mock_set_analog_value(value)` - Control ADC readings
- `mock_set_client_connected(bool)` - Simulate server connection
- `mock_reset()` - Reset all mocks to default state

### WiFi Status Constants
- `WL_CONNECTED` - WiFi connected
- `WL_DISCONNECTED` - WiFi disconnected
- `WL_IDLE_STATUS` - WiFi idle
- Others defined in `mocks/WiFiS3.h`

## Test Results

All tests should pass. If any fail, the output will show:
- Which test failed
- Expected vs actual values
- File and line number of the failure

## Adding New Tests

1. Add test function to `test_main.cpp`:
```cpp
void test_my_new_test(void) {
    // Arrange
    mock_reset();
    
    // Act
    float result = myFunction(input);
    
    // Assert
    TEST_ASSERT_EQUAL_FLOAT(expected, result);
}
```

2. Register the test in `main()`:
```cpp
RUN_TEST(test_my_new_test);
```

3. Run the tests to verify.
