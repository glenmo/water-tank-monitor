# Water Tank Monitor

LoRaWAN + WiFi water tank monitoring system for Arduino UNO R4 WiFi with SX1276 LoRa shield.

## Features

- **Dual Connectivity**: LoRaWAN (primary) + WiFi (backup)
- **LoRaWAN**: AU915 FSB2, OTAA activation, 60-second upload intervals
- **WiFi**: HTTP uploads every 5 seconds to local server (optional backup)
- **Sensors**: Analog pressure sensor (0.5-4.5V, 0-10 kPa range)
- **Measurements**: Pressure, water depth, tank volume

## Hardware

- Arduino UNO R4 WiFi
- SX1276 LoRaWAN shield
  - NSS (CS): Pin 10
  - RST: Pin 9
  - DIO0: Pin 2
  - DIO1: Pin 3
- Analog pressure sensor on A0 pin

## Tank Specifications

- **Diameter**: 100mm
- **Surface Area**: 0.00785 m² (π × 0.05²)
- **Sensor**: Pressure sensor (0.5V - 4.5V = 0-10 kPa)

## Quick Start

### Arduino IDE

1. Open `water_tank_monitor.ino` in Arduino IDE
2. Configure LoRaWAN credentials (see [LoRaWAN Setup Guide](docs/LORAWAN_SETUP.md))
3. Configure WiFi credentials (optional)
4. Upload to board

### PlatformIO

```bash
# Build
pio run

# Build and upload
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor
```

## Configuration

### LoRaWAN Credentials (Required)

Edit the following in `water_tank_monitor.ino`:

```cpp
// Application EUI (8 bytes) - LSB format
static const u1_t PROGMEM APPEUI[8] = { 0x00, ... };

// Device EUI (8 bytes) - LSB format
static const u1_t PROGMEM DEVEUI[8] = { 0x00, ... };

// Application Key (16 bytes) - MSB format
static const u1_t PROGMEM APPKEY[16] = { 0x00, ... };
```

**See [docs/LORAWAN_SETUP.md](docs/LORAWAN_SETUP.md) for detailed credential configuration instructions.**

### WiFi Credentials (Optional Backup)

```cpp
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";
const char* serverHost = "192.168.x.x";
const int serverPort = 8080;
```

## LoRaWAN Payload Format

8-byte binary payload (big-endian):
- **Bytes 0-1**: Voltage (mV) - divide by 1000 for volts
- **Bytes 2-3**: Pressure (centi-kPa) - divide by 100 for kPa
- **Bytes 4-5**: Depth (mm) - divide by 1000 for meters
- **Bytes 6-7**: Volume (centi-liters) - divide by 100 for liters

### Decoder (The Things Network / ChirpStack)

```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  return {
    data: {
      voltage_v: ((bytes[0] << 8) | bytes[1]) / 1000.0,
      pressure_kpa: ((bytes[2] << 8) | bytes[3]) / 100.0,
      depth_m: ((bytes[4] << 8) | bytes[5]) / 1000.0,
      volume_liters: ((bytes[6] << 8) | bytes[7]) / 100.0
    }
  };
}
```

## How It Works

### LoRaWAN Path (Primary)
1. Arduino reads pressure sensor on pin A0
2. Converts voltage to pressure (0.5V-4.5V → 0-10 kPa)
3. Calculates water depth from pressure (1 kPa ≈ 0.102m water)
4. Calculates volume using cylinder formula: V = π × r² × h
5. Packs data into 8-byte binary payload
6. Transmits via LoRaWAN every 60 seconds (respects duty cycle)
7. LoRaWAN gateway forwards to network server
8. Network server decodes and forwards to application

### WiFi Path (Backup)
1-4. Same sensor reading and calculations
5. Sends data via HTTP GET every 5 seconds to local server
6. Web dashboard displays real-time data

## Serial Monitor Output

When Arduino is running, you should see:

```
=== Water Tank Sensor with LoRaWAN + WiFi ===
Tank diameter: 100mm
Initializing LoRaWAN...
AU915 FSB2 configured (channels 8-15, 65)
Starting OTAA join...
Connecting to WiFi backup...
WiFi connected!
IP address: 192.168.1.123

--- Measurement ---
Voltage: 2.345 V
Pressure: 4.61 kPa
Water Depth: 0.094 m
Tank Capacity: 0.74 liters
LoRa Status: JOINED

Packet queued for LoRaWAN transmission
EV_TXCOMPLETE (includes waiting for RX windows)
Data uploaded via WiFi!
```

## Troubleshooting

### LoRaWAN Join Fails (EV_JOIN_FAILED)
- Verify credentials are correctly entered and in right byte order (LSB vs MSB)
- Check that gateway is online and in range
- Verify FSB2 is configured on both device and network server
- Ensure AU915 region is selected on network server

### No Data Received on Network Server
- Check Serial Monitor for "EV_TXCOMPLETE" messages
- Verify payload decoder is configured on network server
- Check gateway traffic in network server console
- Ensure device has successfully joined (look for "LoRa Status: JOINED")

### Arduino Not Connecting to WiFi
- Check WiFi credentials are correct
- Ensure WiFi network is 2.4GHz (UNO R4 WiFi doesn't support 5GHz)
- Check signal strength near Arduino location
- WiFi is optional - LoRaWAN is primary connectivity

### Inaccurate Readings
- Calibrate pressure sensor if needed
- Verify voltage reference (should be 5V)
- Check sensor is submerged correctly
- Ensure ADC_REF_V matches your board's actual voltage

## Customization

### Change LoRaWAN Upload Interval
```cpp
const unsigned long loraUploadInterval = 60000;  // milliseconds (respect duty cycle!)
```

### Change WiFi Upload Interval
```cpp
const unsigned long wifiUploadInterval = 5000;  // milliseconds
```

### Change Tank Diameter
```cpp
const float TANK_DIAMETER_MM = 100.0f;  // your tank diameter in mm
```

## Documentation

- **[docs/CLAUDE.md](docs/CLAUDE.md)** - Development guide for Claude Code
- **[docs/LORAWAN_SETUP.md](docs/LORAWAN_SETUP.md)** - LoRaWAN configuration guide (MUST READ)
- **[platformio.ini](platformio.ini)** - PlatformIO build configuration

## Project Structure

```
water-tank-monitor/
├── water_tank_monitor.ino    # Primary Arduino sketch
├── src/main.cpp               # PlatformIO build source (copy of .ino)
├── platformio.ini             # PlatformIO configuration
├── test/                      # Native unit tests
├── docs/                      # Documentation
│   ├── CLAUDE.md
│   └── LORAWAN_SETUP.md
└── README.md                  # This file
```

## License

Open source - use freely for your projects.
