# LoRaWAN Setup Guide

This document explains how to configure the water tank sensor for LoRaWAN connectivity.

## Overview

The sensor uses LoRaWAN OTAA (Over-The-Air Activation) to join the network and send data. You need to configure three credentials in the Arduino sketch before uploading.

## Gateway Information

- **Gateway EUI**: `2CF7F1177440004B`
- **Region**: AU915 (Australia/New Zealand)
- **Frequency Sub-Band**: FSB2 (channels 8-15, 65)

## Required Credentials

You need to obtain the following credentials from your LoRaWAN network server (The Things Network, ChirpStack, etc.):

### 1. Application EUI (AppEUI/JoinEUI)
- 8 bytes in **LSB (Little-endian)** format
- Example: If your AppEUI is `70B3D57ED00012AB`, you need to reverse it:
  ```cpp
  static const u1_t PROGMEM APPEUI[8] = { 0xAB, 0x12, 0x00, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
  ```

### 2. Device EUI (DevEUI)
- 8 bytes in **LSB (Little-endian)** format
- Example: If your DevEUI is `0004A30B001A2B3C`, you need to reverse it:
  ```cpp
  static const u1_t PROGMEM DEVEUI[8] = { 0x3C, 0x2B, 0x1A, 0x00, 0x0B, 0xA3, 0x04, 0x00 };
  ```

### 3. Application Key (AppKey)
- 16 bytes in **MSB (Big-endian)** format (DO NOT reverse)
- Example: If your AppKey is `A1B2C3D4E5F6071819202122232425262`, use it as-is:
  ```cpp
  static const u1_t PROGMEM APPKEY[16] = {
    0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18,
    0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
  };
  ```

## Configuration Steps

### 1. Register Device on Network Server

On The Things Network:
1. Create an application if you haven't already
2. Register a new end device
3. Choose "Over the air activation (OTAA)"
4. Select frequency plan: "Australia 915-928 MHz, FSB 2"
5. Generate or enter DevEUI and AppKey
6. Note down all three credentials

On ChirpStack:
1. Create an application
2. Add a device profile for Class A OTAA
3. Create a device and note the credentials

### 2. Update Arduino Sketch

Edit `water_tank_sensor_wifi.ino` and replace these lines:

```cpp
// Application EUI (8 bytes) - LSB format
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Device EUI (8 bytes) - LSB format
static const u1_t PROGMEM DEVEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Application Key (16 bytes) - MSB format
static const u1_t PROGMEM APPKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
```

### 3. Verify Pin Mapping

Ensure your SX1276 shield uses these pins (adjust if different):

```cpp
const lmic_pinmap lmic_pins = {
    .nss = 10,                       // NSS (Chip Select)
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,                        // Reset pin
    .dio = {2, 3, LMIC_UNUSED_PIN},  // DIO0, DIO1, DIO2
};
```

### 4. Upload and Monitor

1. Upload the sketch to your Arduino UNO R4 WiFi
2. Open the Serial Monitor at 115200 baud
3. Watch for "EV_JOINING" followed by "EV_JOINED"
4. Once joined, data will be sent every 60 seconds

## Payload Decoder

Configure this decoder on your network server to decode the binary payload:

### The Things Network (JavaScript)

```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  var voltage = ((bytes[0] << 8) | bytes[1]) / 1000.0;
  var pressure = ((bytes[2] << 8) | bytes[3]) / 100.0;
  var depth = ((bytes[4] << 8) | bytes[5]) / 1000.0;
  var volume = ((bytes[6] << 8) | bytes[7]) / 100.0;

  return {
    data: {
      voltage_v: voltage,
      pressure_kpa: pressure,
      depth_m: depth,
      volume_liters: volume
    }
  };
}
```

### ChirpStack (JavaScript)

```javascript
function Decode(fPort, bytes) {
  var voltage = ((bytes[0] << 8) | bytes[1]) / 1000.0;
  var pressure = ((bytes[2] << 8) | bytes[3]) / 100.0;
  var depth = ((bytes[4] << 8) | bytes[5]) / 1000.0;
  var volume = ((bytes[6] << 8) | bytes[7]) / 100.0;

  return {
    voltage_v: voltage,
    pressure_kpa: pressure,
    depth_m: depth,
    volume_liters: volume
  };
}
```

## Troubleshooting

### Join Fails (EV_JOIN_FAILED)

1. Verify credentials are correctly entered and in the right byte order (LSB vs MSB)
2. Check that gateway is online and in range
3. Verify FSB2 is configured on both device and network server
4. Ensure AU915 region is selected on network server

### No Data Received

1. Check Serial Monitor for "EV_TXCOMPLETE" messages
2. Verify payload decoder is configured on network server
3. Check gateway traffic in network server console
4. Ensure device has successfully joined (look for "LoRa Status: JOINED")

### Frequent Disconnections

1. Check signal strength (RSSI/SNR in network server)
2. Verify antenna is properly connected to shield
3. Move device closer to gateway or install outdoor antenna

## Data Usage

- **Payload size**: 8 bytes
- **Transmission interval**: 60 seconds (respects duty cycle)
- **Daily uplinks**: ~1,440 messages
- **Daily data**: ~11.5 KB (well within LoRaWAN fair use limits)

## Channel Configuration

The device is configured for AU915 FSB2:
- **Uplink channels**: 8-15 (916.8, 917.0, 917.2, 917.4, 917.6, 917.8, 918.0, 918.2 MHz)
- **500kHz channel**: 65 (917.5 MHz)
- **Downlink channels**: 8-15 (923.3-924.7 MHz)

Ensure your gateway and network server are configured for the same sub-band.
