#include <WiFiS3.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// LoRaWAN Configuration (OTAA)
// IMPORTANT: Replace these with your actual credentials from The Things Network/ChirpStack
// These should be in LSB format for APPEUI and DEVEUI, MSB for APPKEY

// Application EUI (8 bytes) - LSB format
// IMPORTANT: Replace with your actual AppEUI from network server
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// Device EUI (8 bytes) - LSB format
// IMPORTANT: Replace with your actual DevEUI from network server
static const u1_t PROGMEM DEVEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// Application Key (16 bytes) - MSB format (do NOT reverse)
// IMPORTANT: Replace with your actual AppKey from network server
static const u1_t PROGMEM APPKEY[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}

// Pin mapping for SX1276 LoRa shield on Arduino UNO R4 WiFi
// Adjust these if your shield uses different pins
const lmic_pinmap lmic_pins = {
    .nss = 10,                       // NSS (Chip Select)
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,                        // Reset pin
    .dio = {2, 3, LMIC_UNUSED_PIN},  // DIO0, DIO1, DIO2
};

// WiFi credentials (backup connectivity)
const char* ssid = "IOT";
const char* password = "GU23enY5!";

// Web server details
const char* serverHost = "192.168.55.192";
const int serverPort = 8080;

// Sensor configuration
const float ADC_REF_V = 5.00f;
const int   ADC_MAX   = 1023;
const float V_MIN  = 0.50f;
const float V_MAX  = 4.50f;
const float FS_KPA = 10.0f;

// Tank configuration
const float TANK_DIAMETER_MM = 100.0f;
const float TANK_RADIUS_M = (TANK_DIAMETER_MM / 2.0f) / 1000.0f;

// Timing
unsigned long lastLoRaUploadTime = 0;
unsigned long lastWiFiUploadTime = 0;
const unsigned long loraUploadInterval = 60000;  // LoRa upload every 60 seconds (respect duty cycle)
const unsigned long wifiUploadInterval = 5000;   // WiFi upload every 5 seconds

// LoRaWAN state
static osjob_t sendjob;
bool loraJoined = false;
bool loraSending = false;

// Data buffer for LoRaWAN
static uint8_t loraPayload[12];

WiFiClient client;

// Utility functions
static float clampf(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

static float voltageToKpa(float v) {
  float span = V_MAX - V_MIN;
  if (span < 0.001f) return 0.0f;

  float ratio = (v - V_MIN) / span;
  ratio = clampf(ratio, 0.0f, 1.0f);
  return ratio * FS_KPA;
}

float readA0VoltageAveraged() {
  const int samples = 10;
  float sum = 0.0f;

  for (int i = 0; i < samples; i++) {
    int raw = analogRead(A0);
    sum += (float)raw;
    delay(10);
  }

  float avgRaw = sum / (float)samples;
  return (avgRaw * ADC_REF_V) / (float)ADC_MAX;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void uploadToServer(float depth_m, float pressure_kpa, float volume_liters) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping WiFi upload.");
    return;
  }

  Serial.print("Connecting to server: ");
  Serial.println(serverHost);

  if (!client.connect(serverHost, serverPort)) {
    Serial.println("Connection to server failed!");
    return;
  }

  String url = "/update?";
  url += "depth=" + String(depth_m, 3);
  url += "&pressure=" + String(pressure_kpa, 2);
  url += "&volume=" + String(volume_liters, 2);

  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: " + String(serverHost) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Server timeout!");
      client.stop();
      return;
    }
  }

  Serial.println("Server response:");
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  client.stop();
  Serial.println("\nData uploaded via WiFi!");
}

// Pack sensor data into compact binary format for LoRaWAN
void packLoRaPayload(float voltage, float pressure_kpa, float depth_m, float volume_liters) {
  // Pack data efficiently to minimize payload size (air time)
  // voltage: 2 bytes (uint16, resolution 0.001V)
  // pressure: 2 bytes (uint16, resolution 0.01 kPa)
  // depth: 2 bytes (uint16, resolution 0.001m)
  // volume: 2 bytes (uint16, resolution 0.01 liters)

  uint16_t volt_packed = (uint16_t)(voltage * 1000.0f);
  uint16_t press_packed = (uint16_t)(pressure_kpa * 100.0f);
  uint16_t depth_packed = (uint16_t)(depth_m * 1000.0f);
  uint16_t vol_packed = (uint16_t)(volume_liters * 100.0f);

  loraPayload[0] = (volt_packed >> 8) & 0xFF;
  loraPayload[1] = volt_packed & 0xFF;
  loraPayload[2] = (press_packed >> 8) & 0xFF;
  loraPayload[3] = press_packed & 0xFF;
  loraPayload[4] = (depth_packed >> 8) & 0xFF;
  loraPayload[5] = depth_packed & 0xFF;
  loraPayload[6] = (vol_packed >> 8) & 0xFF;
  loraPayload[7] = vol_packed & 0xFF;
}

void do_send(osjob_t* j) {
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // Read sensor data
    float voltage = readA0VoltageAveraged();
    float pressure_kpa = voltageToKpa(voltage);
    float depth_m = pressure_kpa * 0.10197162f;
    float volume_m3 = PI * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
    float volume_liters = volume_m3 * 1000.0f;

    // Pack and send
    packLoRaPayload(voltage, pressure_kpa, depth_m, volume_liters);
    LMIC_setTxData2(1, loraPayload, sizeof(loraPayload), 0);
    Serial.println(F("Packet queued for LoRaWAN transmission"));
    loraSending = true;
  }
}

// LMIC event handler
void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch(ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      Serial.println(F("EV_JOINED"));
      loraJoined = true;
      {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devaddr: ");
        Serial.println(devaddr, HEX);
        Serial.print("AppSKey: ");
        for (size_t i=0; i<sizeof(artKey); ++i) {
          if (i != 0)
            Serial.print("-");
          Serial.print(artKey[i], HEX);
        }
        Serial.println("");
        Serial.print("NwkSKey: ");
        for (size_t i=0; i<sizeof(nwkKey); ++i) {
          if (i != 0)
            Serial.print("-");
          Serial.print(nwkKey[i], HEX);
        }
        Serial.println();
      }
      // Disable link check validation (automatically enabled during join)
      LMIC_setLinkCheckMode(0);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      loraJoined = false;
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      loraJoined = false;
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      loraSending = false;
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        Serial.print(F("Received "));
        Serial.print(LMIC.dataLen);
        Serial.println(F(" bytes of payload"));
      }
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    case EV_TXSTART:
      Serial.println(F("EV_TXSTART"));
      break;
    case EV_TXCANCELED:
      Serial.println(F("EV_TXCANCELED"));
      loraSending = false;
      break;
    case EV_RXSTART:
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
      break;
    default:
      Serial.print(F("Unknown event: "));
      Serial.println((unsigned) ev);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }

  Serial.println(F("\n=== Water Tank Sensor with LoRaWAN + WiFi ==="));
  Serial.println("Tank diameter: " + String(TANK_DIAMETER_MM) + "mm");

  // Initialize LoRaWAN
  Serial.println(F("Initializing LoRaWAN..."));
  os_init();
  LMIC_reset();

  // Configure for AU915 region
  // For AU915, we need to select FSB2 (channels 8-15 + channel 65)
  // Disable all 72 channels
  for (int c = 0; c < 72; c++) {
    LMIC_disableChannel(c);
  }
  // Enable FSB2 channels (8-15)
  for (int c = 8; c < 16; c++) {
    LMIC_enableChannel(c);
  }
  // Enable channel 65 (500kHz channel for FSB2)
  LMIC_enableChannel(65);

  Serial.println(F("AU915 FSB2 configured (channels 8-15, 65)"));

  // Set data rate and transmit power for AU915
  LMIC_setDrTxpow(DR_SF7, 14);  // SF7, 14dBm

  // Start OTAA join
  Serial.println(F("Starting OTAA join..."));
  LMIC_startJoining();

  // Connect to WiFi as backup
  Serial.println(F("Connecting to WiFi backup..."));
  connectWiFi();

  Serial.println(F("Setup complete. Starting measurements...\n"));
}

void loop() {
  // Process LoRaWAN events (CRITICAL - must be called frequently)
  os_runloop_once();

  // Ensure WiFi is connected (backup)
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {  // Check every 30 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("WiFi disconnected. Reconnecting..."));
      connectWiFi();
    }
    lastWiFiCheck = millis();
  }

  // Read and display sensor data
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 5000) {  // Display every 5 seconds
    float voltage = readA0VoltageAveraged();
    float pressure_kpa = voltageToKpa(voltage);
    float depth_m = pressure_kpa * 0.10197162f;
    float volume_m3 = PI * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
    float volume_liters = volume_m3 * 1000.0f;

    Serial.println(F("--- Measurement ---"));
    Serial.print(F("Voltage: "));
    Serial.print(voltage, 3);
    Serial.println(F(" V"));

    Serial.print(F("Pressure: "));
    Serial.print(pressure_kpa, 2);
    Serial.println(F(" kPa"));

    Serial.print(F("Water Depth: "));
    Serial.print(depth_m, 3);
    Serial.println(F(" m"));

    Serial.print(F("Tank Capacity: "));
    Serial.print(volume_liters, 2);
    Serial.println(F(" liters"));

    Serial.print(F("LoRa Status: "));
    if (loraJoined) {
      Serial.println(F("JOINED"));
    } else {
      Serial.println(F("NOT JOINED"));
    }
    Serial.println();

    lastDisplay = millis();

    // Upload via WiFi (more frequent updates)
    if (millis() - lastWiFiUploadTime >= wifiUploadInterval) {
      uploadToServer(depth_m, pressure_kpa, volume_liters);
      lastWiFiUploadTime = millis();
    }
  }

  // Send via LoRaWAN (less frequent due to duty cycle restrictions)
  if (loraJoined && !loraSending && (millis() - lastLoRaUploadTime >= loraUploadInterval)) {
    do_send(&sendjob);
    lastLoRaUploadTime = millis();
  }
}
