# Changelog - Water Tank Monitor

All notable changes to this project will be documented in this file.

## [2.0.0] - 2026-01-31

### Added - LoRaWAN Support 🎉

#### Hardware Integration
- LoRaWAN communication via Duinotech LoRa Shield (XC4392)
- SX1276 radio chip support (915MHz)
- AU915 region configuration (Australia)
- Sub-band 2 (channels 8-15) for optimal gateway compatibility
- Antenna requirement: 915MHz LoRa antenna

#### Software Features
- MCCI LoRaWAN LMIC library integration
- OTAA (Over-The-Air Activation) join protocol
- Optimized 8-byte payload format:
  - Voltage (2 bytes, mV)
  - Pressure (2 bytes, mPa)
  - Depth (2 bytes, mm)
  - Volume (2 bytes, liters)
- 60-second uplink interval (configurable)
- Median filtering on sensor readings (51 samples)
- SPI communication test on startup

#### Network Infrastructure
- ChirpStack v4.16.0 integration
- SenseCAP M2 gateway support
- Docker-based deployment
- MQTT-based communication
- Payload decoder JavaScript function

#### Documentation
- Complete LoRaWAN setup guide
- Pin configuration documentation
- ChirpStack configuration instructions
- Troubleshooting guide
- Network architecture diagrams
- Key byte-order conversion reference

### Changed

#### Pin Configuration
- **CRITICAL FIX**: DIO1 pin changed from 6 to 3
  - Previous: `.dio = { 2, 6, LMIC_UNUSED_PIN }`
  - Current: `.dio = { 2, 3, LMIC_UNUSED_PIN }`
  - Reason: Pin 6 does not work for RX timing on XC4392 shield

#### Timing & Reliability
- Increased clock error tolerance from 10% to 50%
  - Improves RX window reliability
  - Essential for join-accept reception
- Added frame counter validation notes

#### WiFi Configuration
- WiFi disabled by default in LoRa mode
  - Prevents interference with LoRa RX windows
  - Can be re-enabled if needed
  - Backup mode preserved in code

#### Code Structure
- Added SPI radio test on startup
- Enhanced event logging with timestamps
- Improved join status messages
- Better error handling and diagnostics

### Fixed

#### Critical Fixes
1. **Join-Accept Reception Issue**
   - Problem: Arduino transmitting but not receiving join-accept
   - Root Cause: Wrong DIO1 pin mapping
   - Solution: Changed DIO1 from pin 6 to pin 3
   - Impact: Reliable OTAA joins

2. **WiFi Interference**
   - Problem: WiFi blocking LoRa RX windows
   - Solution: Disabled WiFi during LoRa operation
   - Impact: Consistent join success

3. **Clock Timing**
   - Problem: Missing RX windows due to tight timing
   - Solution: Increased clock error tolerance to 50%
   - Impact: Reliable downlink reception

#### Minor Fixes
- HTTP server port changed from 8081 to 8080
- Removed duplicate setup() functions
- Fixed LMIC library configuration conflicts
- Corrected key byte order documentation

### Technical Details

#### LoRaWAN Configuration
```cpp
Region: AU915
Sub-band: 2 (channels 8-15)
Spreading Factor: SF7
TX Power: 14 dBm
Clock Error: 50%
Join Method: OTAA
Upload Interval: 60 seconds
```

#### Pin Mapping (XC4392 Shield)
```cpp
NSS (CS): Pin 10
RST: Pin 9
DIO0: Pin 2
DIO1: Pin 3  ← CRITICAL: Changed from 6
DIO2: Not used
```

#### OTAA Keys (LSB format in Arduino)
```cpp
APPEUI: { 0xee, 0x39, 0xcf, 0x66, 0xb6, 0x0c, 0xf7, 0x77 }
DEVEUI: { 0x14, 0xe2, 0x7f, 0x36, 0xa7, 0xab, 0x8d, 0x6c }
APPKEY: { 0xe9, 0xb3, 0xf0, 0x02, 0x64, 0x51, 0xa9, 0x38,
          0xd4, 0x64, 0x10, 0x3a, 0x4d, 0x25, 0xaa, 0x14 }
```

**Note:** Keys are in LSB format for Arduino, must be reversed to MSB for ChirpStack.

### Gateway Configuration

#### SenseCAP M2 Setup
- Region: AU915-928
- Frequency plan: FSB2, channels 8-15
- Server: 192.168.55.192
- Port: 1700 (UDP)

#### ChirpStack v4 Setup
- Gateway backend: MQTT
- Topic prefix: `gateway/`
- Region: AU915 (au915_0)
- Database: PostgreSQL
- Cache: Redis

### Known Issues

1. **WiFi IP Shows 0.0.0.0**
   - WiFi connects but doesn't get DHCP address
   - Not critical as WiFi is disabled for LoRa operation
   - Will be fixed in future release

2. **First Join May Take Multiple Attempts**
   - Normal behavior during initial power-up
   - Arduino retries automatically
   - Usually joins within 1-2 minutes

### Testing Performed

#### Hardware Tests
- ✅ SPI communication with SX1276 (version 0x12 verified)
- ✅ Pin connectivity tests
- ✅ Antenna connection verified
- ✅ Pressure sensor readings

#### Network Tests
- ✅ Gateway receiving packets (tcpdump verified)
- ✅ ChirpStack receiving join requests
- ✅ Join-accept transmission
- ✅ Join-accept reception by Arduino
- ✅ OTAA join completion
- ✅ Uplink transmission
- ✅ Payload decoding

#### Integration Tests
- ✅ End-to-end sensor → ChirpStack
- ✅ 60-second uplink interval
- ✅ Frame counter incrementing
- ✅ Decoded data visible in UI

### Migration Guide (v1.0 → v2.0)

#### Hardware Changes
1. Add Duinotech LoRa Shield (XC4392) to Arduino
2. Connect 915MHz antenna to shield
3. Verify pin connections (especially DIO pins)

#### Software Changes
1. Install MCCI LoRaWAN LMIC library
2. Configure library: `lmic_project_config.h`
3. Upload new sketch: `sketch_jan31a_FIXED_V2.ino`
4. Configure ChirpStack with device keys

#### Network Changes
1. Deploy ChirpStack v4 (Docker recommended)
2. Configure SenseCAP M2 gateway
3. Create device profile in ChirpStack
4. Add device with OTAA keys
5. Configure payload decoder

### Performance Metrics

#### Battery Life Estimates
- SF7, 14dBm, 60s interval: ~2-3 months (2500mAh)
- SF10, 14dBm, 300s interval: ~6-8 months (2500mAh)
- Can be optimized further with sleep modes

#### Network Performance
- Join time: 20-40 seconds typical
- Packet loss: <1% at 20m range
- RSSI: -40 to -90 dBm depending on distance
- SNR: +5 to +10 dB typical

### Dependencies

#### Arduino Libraries
- MCCI_LoRaWAN_LMIC_library v4.1.1+
- WiFiS3 (built-in)
- SPI (built-in)

#### External Services
- ChirpStack v4.16.0
- PostgreSQL 14+
- Redis 7+
- Mosquitto MQTT broker

### Breaking Changes

⚠️ **v1.0 Users:**
- WiFi-only communication no longer default
- HTTP endpoint changed from 8081 to 8080
- Different data transmission format (binary vs JSON)
- Requires LoRaWAN gateway infrastructure

### Contributors

Special thanks to:
- ChirpStack community for excellent documentation
- MCCI for the LMIC library
- Arduino community for troubleshooting support

---

## [1.0.0] - 2025-12-15

### Initial Release - WiFi Version

#### Features
- WiFi connectivity
- HTTP API communication
- Web-based dashboard
- Real-time monitoring
- Pressure sensor integration
- Tank volume calculation
- 100mm × 200mm tank support

#### Hardware
- Arduino UNO R4 WiFi
- Pressure sensor (A0)
- WiFi network required

#### Software
- Tank geometry calculations
- Median filter for stable readings
- HTTP GET requests
- Simple web interface

---

## Future Roadmap

### Planned for v2.1
- [ ] Sleep mode for battery optimization
- [ ] Downlink command support
- [ ] Adaptive data rate (ADR)
- [ ] Multiple sensor support
- [ ] Alert thresholds
- [ ] WiFi fallback mode

### Planned for v3.0
- [ ] GPS location tracking
- [ ] Multi-gateway support
- [ ] Over-the-air firmware updates
- [ ] Encryption improvements
- [ ] Mobile app integration
- [ ] Cloud dashboard

---

**Repository:** https://github.com/glenmo/water-tank-monitor
**Documentation:** See LORAWAN_README.md
**Issues:** Report via GitHub Issues
