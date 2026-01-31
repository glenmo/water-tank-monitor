# Arduino UNO R4 WiFi + LoRaWAN Water Tank Sensor

Complete documentation for the LoRaWAN-enabled water tank monitoring system using Arduino UNO R4 WiFi with Duinotech LoRa Shield (XC4392).

## 📋 Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Pin Configuration](#pin-configuration)
- [Setup Instructions](#setup-instructions)
- [Configuration](#configuration)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [Network Architecture](#network-architecture)

---

## 🔧 Hardware Requirements

### Main Components
- **Arduino UNO R4 WiFi**
- **Duinotech Long Range LoRa Shield (XC4392)**
  - Chip: SX1276
  - Frequency: 915MHz
  - Region: AU915
- **Pressure Sensor** (0.5-4.5V output, 10kPa range)
- **915MHz LoRa Antenna** (CRITICAL: Must be 915MHz, not 433MHz or 868MHz)

### Gateway
- **SenseCAP M2 LoRaWAN Gateway** (or compatible)
- **ChirpStack v4** LoRaWAN Network Server

### Tank Specifications
- Diameter: 100mm
- Depth: 200mm max
- Capacity: ~1,570ml

---

## 💻 Software Requirements

### Arduino Libraries
1. **MCCI LoRaWAN LMIC** (v4.x or higher)
   - Install via: Tools → Manage Libraries → Search "MCCI LoRaWAN LMIC"
   - Repository: https://github.com/mcci-catena/arduino-lmic

2. **WiFiS3** (built-in for Arduino UNO R4 WiFi)

### Library Configuration

**CRITICAL:** Edit the LMIC library configuration file:

Location: `~/Documents/Arduino/libraries/MCCI_LoRaWAN_LMIC_library/project_config/lmic_project_config.h`

Ensure these settings:
```cpp
// Uncomment only AU915:
#define CFG_au915 1
//#define CFG_us915 1    // Comment out
//#define CFG_eu868 1    // Comment out

// Uncomment SX1276:
#define CFG_sx1276_radio 1
//#define CFG_sx1272_radio 1  // Comment out
```

---

## 📍 Pin Configuration

### Working Pin Mapping (XC4392 Shield)

```cpp
const lmic_pinmap lmic_pins = {
  .nss = 10,                         // SPI Chip Select
  .rxtx = LMIC_UNUSED_PIN,          // Not used
  .rst = 9,                          // Reset pin
  .dio = { 2, 3, LMIC_UNUSED_PIN }  // DIO0=2, DIO1=3
};
```

**IMPORTANT:** DIO1 **MUST** be on pin 3 for the XC4392 shield. Pin 6 does NOT work.

### Sensor Connection
- Pressure sensor analog output → Arduino pin **A0**
- Sensor power: 5V
- ADC resolution: 10-bit (0-1023)
- Voltage range: 0.5V - 4.5V = 0-10 kPa

---

## 🚀 Setup Instructions

### 1. Hardware Assembly

1. **Mount shield** on Arduino UNO R4 WiFi
   - Ensure all pins are properly seated
   - Check for bent pins
   
2. **Connect antenna** to shield's antenna connector
   - MUST be 915MHz antenna
   - Poor/wrong antenna = no communication

3. **Connect pressure sensor** to A0

4. **Power** via USB or external 5V

### 2. ChirpStack Configuration

#### Create Device Profile
1. Navigate to **Device Profiles** → **Add Device Profile**
2. Settings:
   ```
   Name: Arduino-OTAA
   Region: AU915
   MAC version: LoRaWAN 1.0.3
   Regional parameters revision: RP002-1.0.3
   Supports OTAA: ✓ (checked)
   ADR: ✓ (enabled)
   Max EIRP: 30
   ```

#### Create Application
1. Navigate to **Applications** → **Add Application**
2. Settings:
   ```
   Name: Tank Monitor
   Description: Water tank level monitoring
   ```

#### Add Device
1. Go to Application → **Devices** → **Add Device**
2. **CRITICAL - Key Byte Order:**

Your Arduino keys are LSB but ChirpStack needs MSB. Use these converted values:

```
Device name: Tank-Sensor-01
Device EUI: 6C8DABA7367FE214    (reversed from Arduino)
Device profile: Arduino-OTAA
```

3. Click **Keys (OTAA)** tab:
```
Application key: E9B3F0026451A938D464103A4D25AA14
```

**Key Conversion Reference:**
```
Arduino DEVEUI (LSB): { 0x14, 0xe2, 0x7f, 0x36, 0xa7, 0xab, 0x8d, 0x6c }
ChirpStack (MSB):     6C8DABA7367FE214

Arduino APPEUI (LSB): { 0xee, 0x39, 0xcf, 0x66, 0xb6, 0x0c, 0xf7, 0x77 }
ChirpStack (MSB):     77F70CB666CF39EE

Arduino APPKEY (MSB): { 0xe9, 0xb3, 0xf0, 0x02, 0x64, 0x51, 0xa9, 0x38, 
                        0xd4, 0x64, 0x10, 0x3a, 0x4d, 0x25, 0xaa, 0x14 }
ChirpStack (MSB):     E9B3F0026451A938D464103A4D25AA14
```

#### Configure Payload Decoder

1. Go to **Device Profiles** → **Arduino-OTAA** → **Codec** tab
2. Select **JavaScript functions**
3. Paste this decoder:

```javascript
function decodeUplink(input) {
  var bytes = input.bytes;
  
  if (bytes.length !== 8) {
    return {
      errors: ["Expected 8 bytes, got " + bytes.length]
    };
  }
  
  // Decode voltage (mV → V)
  var voltage_mv = (bytes[0] << 8) | bytes[1];
  var voltage_v = voltage_mv / 1000.0;
  
  // Decode pressure (mPa → kPa)
  var pressure_mpa = (bytes[2] << 8) | bytes[3];
  var pressure_kpa = pressure_mpa / 1000.0;
  
  // Decode depth (mm → m)
  var depth_mm = (bytes[4] << 8) | bytes[5];
  var depth_m = depth_mm / 1000.0;
  
  // Decode volume (liters)
  var volume_l = (bytes[6] << 8) | bytes[7];
  
  return {
    data: {
      voltage: voltage_v,
      pressure_kpa: pressure_kpa,
      depth_m: depth_m,
      depth_mm: depth_mm,
      volume_liters: volume_l,
      volume_ml: volume_l * 1000
    }
  };
}
```

### 3. Upload Arduino Sketch

1. Open `sketch_jan31a_FIXED_V2.ino`
2. Update WiFi credentials (optional, currently disabled):
   ```cpp
   const char* ssid = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";
   ```
3. Verify keys match your ChirpStack configuration
4. Upload to Arduino UNO R4 WiFi
5. Open Serial Monitor at **115200 baud**

---

## ⚙️ Configuration

### LoRaWAN Settings

```cpp
// OTAA Keys (LSB format for Arduino)
APPEUI: { 0xee, 0x39, 0xcf, 0x66, 0xb6, 0x0c, 0xf7, 0x77 }
DEVEUI: { 0x14, 0xe2, 0x7f, 0x36, 0xa7, 0xab, 0x8d, 0x6c }
APPKEY: { 0xe9, 0xb3, 0xf0, 0x02, 0x64, 0x51, 0xa9, 0x38, 
          0xd4, 0x64, 0x10, 0x3a, 0x4d, 0x25, 0xaa, 0x14 }

// Radio Configuration
Region: AU915 (Australia 915MHz)
Sub-band: 2 (Channels 8-15)
Spreading Factor: SF7
TX Power: 14 dBm
Clock Error: 50% (for reliable RX window timing)
Upload Interval: 60 seconds
```

### Sensor Calibration

```cpp
// Voltage to Pressure Mapping
V_MIN = 0.50V  →  0 kPa
V_MAX = 4.50V  →  10 kPa

// Tank Geometry
TANK_DIAMETER_MM = 100.0  // 100mm diameter
TANK_RADIUS_M = 0.05      // 50mm radius = 0.05m
MAX_DEPTH = 200mm         // Maximum water depth

// ADC Configuration
ADC_REF_V = 5.00V         // Reference voltage
ADC_MAX = 1023            // 10-bit ADC
```

### Payload Format (8 bytes)

| Bytes | Data | Format | Resolution |
|-------|------|--------|------------|
| 0-1 | Voltage | uint16 | mV |
| 2-3 | Pressure | uint16 | mPa (milliPascals) |
| 4-5 | Depth | uint16 | mm |
| 6-7 | Volume | uint16 | liters |

---

## 📱 Usage

### Normal Operation

1. **Power on** Arduino with connected sensor
2. **Wait for join** (~30 seconds):
   ```
   EV_JOINING
   EV_TXSTART - Freq: 917xxx000 Hz
   ✓✓✓ EV_JOINED ✓✓✓
   ```

3. **Monitor uplinks** (every 60 seconds):
   ```
   LoRa packet queued
     Voltage: 2.750 V
     Pressure: 5.625 kPa
     Depth: 0.573 m
     Volume: 4.51 L
   EV_TXSTART
   EV_TXCOMPLETE
   ```

4. **View data in ChirpStack**:
   - Navigate to device → LoRaWAN frames
   - See uplink frames with decoded sensor data
   - Dashboard shows real-time measurements

### Serial Monitor Output

**Successful startup:**
```
========================================
  Water Tank Sensor - LoRa + WiFi
========================================

--- Testing LoRa Radio (SX1276) ---
Resetting radio...
SX1276 Version: 0x12
✓ SX1276 radio detected!

--- Initializing LoRaWAN ---
Configuring AU915 sub-band 2...
Starting OTAA join...

--- WiFi Disabled for Testing ---

========================================
  Setup Complete - Monitoring Started
========================================

16297: EV_JOINING
16455: EV_TXSTART - Freq: 918000000 Hz, DR: DR2
358728: ✓✓✓ EV_JOINED ✓✓✓
```

---

## 🔧 Troubleshooting

### Problem: No `EV_JOINED` (Stuck at `EV_JOINING`)

**Symptoms:**
```
EV_JOINING
EV_TXSTART
(no EV_JOINED)
```

**Solutions:**

1. **Check Device in ChirpStack:**
   - Device exists with correct DevEUI: `6c8daba7367fe214` (lowercase)
   - Keys match exactly
   - Device profile is AU915 OTAA

2. **Check Gateway:**
   ```bash
   docker logs -f chirpstack-gateway-bridge | grep "event=up"
   ```
   Should see uplink events when Arduino transmits

3. **Check DIO1 Pin:**
   - MUST be pin 3 for XC4392 shield
   - If using different shield, verify DIO1 pin mapping

4. **Increase Clock Error:**
   ```cpp
   LMIC_setClockError(MAX_CLOCK_ERROR * 50 / 100);  // Try 50%
   ```

5. **Disable WiFi:**
   - WiFi can interfere with LoRa RX
   - Comment out `connectWiFi()` in setup

### Problem: `SX1276 Version: 0x00` or `0xFF`

**Cause:** Shield not communicating via SPI

**Solutions:**
1. Reseat shield firmly on Arduino
2. Check for bent pins
3. Verify SPI pins: MOSI, MISO, SCK
4. Test with multimeter if available

### Problem: Gateway Receiving but No Join

**Check ChirpStack logs:**
```bash
docker logs -f chirpstack | grep -i "join"
```

Look for:
- `Device-nonce validated` ✓ Good
- `MIC failed` ✗ Wrong keys
- `Device not found` ✗ Device not added

### Problem: Joined but No Uplinks

**Check in Serial Monitor:**
```cpp
// Should see every 60 seconds:
LoRa packet queued
EV_TXSTART
EV_TXCOMPLETE
```

If not appearing:
1. Check `loraJoined` flag is true
2. Verify 60-second timer logic
3. Ensure `do_send()` is being called

### Problem: Uplinks Sent but Not in ChirpStack

**Check:**
1. Frame counter validation (disable in ChirpStack for testing)
2. Gateway receiving: `docker logs chirpstack-gateway-bridge`
3. ChirpStack processing: `docker logs chirpstack`

---

## 🌐 Network Architecture

```
┌─────────────────────────────────────────┐
│  Arduino UNO R4 WiFi                    │
│  + Duinotech LoRa Shield (XC4392)       │
│  + Pressure Sensor (A0)                 │
│                                         │
│  Every 60s: Send 8-byte payload        │
│  Format: Voltage|Pressure|Depth|Volume  │
└────────────────┬────────────────────────┘
                 │
                 │ LoRa 915MHz
                 │ SF7, 14dBm
                 │ AU915 Sub-band 2
                 │
┌────────────────▼────────────────────────┐
│  SenseCAP M2 Gateway                    │
│  192.168.55.157                         │
│                                         │
│  Receives: LoRa packets                 │
│  Forwards: UDP port 1700                │
└────────────────┬────────────────────────┘
                 │
                 │ UDP 1700
                 │
┌────────────────▼────────────────────────┐
│  Gateway Bridge (Docker)                │
│  172.19.0.5:1700                        │
│                                         │
│  Converts: UDP → MQTT                   │
│  Topics: gateway/+/event/+              │
└────────────────┬────────────────────────┘
                 │
                 │ MQTT tcp://mosquitto:1883
                 │
┌────────────────▼────────────────────────┐
│  ChirpStack v4.16.0 (Docker)            │
│  http://192.168.55.192:8081             │
│                                         │
│  - Device management                    │
│  - OTAA join handling                   │
│  - Payload decoding                     │
│  - Data storage                         │
│  - Web UI                               │
└─────────────────────────────────────────┘
```

### Communication Flow

1. **Uplink (Arduino → ChirpStack):**
   ```
   Arduino → LoRa Radio → Gateway → UDP:1700 → 
   Gateway Bridge → MQTT → ChirpStack
   ```

2. **Downlink (ChirpStack → Arduino):**
   ```
   ChirpStack → MQTT → Gateway Bridge → UDP:1700 →
   Gateway → LoRa Radio → Arduino
   ```

3. **Join Process (OTAA):**
   ```
   Arduino: Send Join-Request
   ↓
   Gateway: Forward to ChirpStack
   ↓
   ChirpStack: Validate keys, generate session
   ↓
   ChirpStack: Send Join-Accept
   ↓
   Gateway: Transmit Join-Accept
   ↓
   Arduino: Receive Join-Accept
   ↓
   Status: JOINED ✓
   ```

---

## 📊 Key Frequencies (AU915 Sub-band 2)

| Channel | Frequency | Bandwidth | Usage |
|---------|-----------|-----------|-------|
| 8 | 916.8 MHz | 125 kHz | Uplink |
| 9 | 917.0 MHz | 125 kHz | Uplink |
| 10 | 917.2 MHz | 125 kHz | Uplink |
| 11 | 917.4 MHz | 125 kHz | Uplink |
| 12 | 917.6 MHz | 125 kHz | Uplink |
| 13 | 917.8 MHz | 125 kHz | Uplink |
| 14 | 918.0 MHz | 125 kHz | Uplink |
| 15 | 918.2 MHz | 125 kHz | Uplink |
| 65 | 923.3 MHz | 500 kHz | Uplink (high DR) |

---

## 📚 Additional Resources

- [ChirpStack Documentation](https://www.chirpstack.io/docs/)
- [MCCI LMIC Library](https://github.com/mcci-catena/arduino-lmic)
- [LoRaWAN Regional Parameters](https://lora-alliance.org/resource_hub/rp2-1-0-3-lorawan-regional-parameters/)
- [AU915 Frequency Plan](https://www.thethingsnetwork.org/docs/lorawan/regional-parameters/)

---

## 🔐 Security Notes

- **Never commit** OTAA keys to public repositories
- **Change default keys** before deployment
- **Use unique keys** for each device
- **Enable frame counter validation** in production
- **Secure your ChirpStack installation** (HTTPS, auth)

---

## 📝 Version History

### v2.0 - LoRaWAN Integration (January 2026)
- Added LoRaWAN support via MCCI LMIC library
- Configured for AU915 region, sub-band 2
- OTAA join with proper key handling
- 8-byte optimized payload format
- Disabled WiFi during testing for reliability
- Fixed DIO1 pin mapping (pin 3 for XC4392)
- 60-second uplink interval
- ChirpStack v4 integration

### v1.0 - WiFi Version
- Basic WiFi connectivity
- HTTP API for sensor data
- Web dashboard
- Real-time monitoring

---

## 👥 Support

For issues or questions:
1. Check [Troubleshooting](#troubleshooting) section
2. Review ChirpStack logs: `docker logs chirpstack`
3. Check Gateway logs: `docker logs chirpstack-gateway-bridge`
4. Verify Serial Monitor output at 115200 baud

---

**Last Updated:** January 31, 2026
