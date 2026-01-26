// LMIC region - define ONE only

#include <WiFiS3.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

const lmic_pinmap lmic_pins = {
  .nss = 10,                          // SPI CS
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 9,                           // RESET
  .dio = { 2, 6, LMIC_UNUSED_PIN }   // DIO0 = 2, DIO1 = 6, DIO2
};


// -------------------- OTAA KEYS --------------------
// APPEUI and DEVEUI are LSB for LMIC; APPKEY is MSB (as you have it).

static const u1_t PROGMEM APPEUI[8] = { 0xee, 0x39, 0xcf, 0x66, 0xb6, 0x0c, 0xf7, 0x77 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

static const u1_t PROGMEM DEVEUI[8] = { 0x14, 0xe2, 0x7f, 0x36, 0xa7, 0xab, 0x8d, 0x6c };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

static const u1_t PROGMEM APPKEY[16] = { 0xe9, 0xb3, 0xf0, 0x02, 0x64, 0x51, 0xa9, 0x38, 0xd4, 0x64, 0x10, 0x3a, 0x4d, 0x25, 0xaa, 0x14 };
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }


// -------------------- WiFi (backup) --------------------
const char* ssid = "IOT";
const char* password = "GU23enY5!";

const char* serverHost = "192.168.55.192";
const int serverPort = 8080;

WiFiClient client;

// -------------------- Sensor config --------------------
const float ADC_REF_V = 5.00f;
const int   ADC_MAX   = 1023;
const float V_MIN  = 0.50f;
const float V_MAX  = 4.50f;
const float FS_KPA = 10.0f;

// Tank geometry
const float TANK_DIAMETER_MM = 100.0f;
const float TANK_RADIUS_M = (TANK_DIAMETER_MM / 2.0f) / 1000.0f;

// Timing
unsigned long lastLoRaUploadTime = 0;
unsigned long lastWiFiUploadTime = 0;
const unsigned long loraUploadInterval = 60000;
const unsigned long wifiUploadInterval = 5000;

// LoRaWAN state
static osjob_t sendjob;
bool loraJoined = false;
bool loraSending = false;

// Payload (8 bytes used)
static uint8_t loraPayload[8];

// -------------------- Utility --------------------
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
    delay(5);
  }
  float avgRaw = sum / (float)samples;
  return (avgRaw * ADC_REF_V) / (float)ADC_MAX;
}

// -------------------- WiFi --------------------
void connectWiFi() {
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi connected!"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("\nWiFi connection failed!"));
  }
}

void sendDataViaWiFi(float voltage, float kpa, float waterDepthM, float volumeL) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi not connected, skipping HTTP upload"));
    return;
  }

  Serial.print(F("Connecting to server: "));
  Serial.print(serverHost);
  Serial.print(":");
  Serial.println(serverPort);

  if (!client.connect(serverHost, serverPort)) {
    Serial.println(F("Connection to server failed"));
    return;
  }

  // Build GET request with URL parameters
  String url = "/api/sensor-data?voltage=" + String(voltage, 3) +
               "&pressure_kpa=" + String(kpa, 3) +
               "&water_depth_m=" + String(waterDepthM, 3) +
               "&volume_liters=" + String(volumeL, 2);

  client.print("GET ");
  client.print(url);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(serverHost);
  client.println("Connection: close");
  client.println();

  Serial.println(F("HTTP GET request sent:"));
  Serial.println(url);

  delay(100);
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }
  client.stop();
}

// -------------------- LoRaWAN --------------------
void do_send(osjob_t* j) {
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    float v = readA0VoltageAveraged();
    float kpa = voltageToKpa(v);
    float rho = 1000.0f;
    float g = 9.81f;
    float depth_m = (kpa * 1000.0f) / (rho * g);

    float vol_m3 = 3.14159f * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
    float vol_L = vol_m3 * 1000.0f;

    // Pack data into 8 bytes
    uint16_t v_encoded = (uint16_t)(v * 1000.0f);
    uint16_t kpa_encoded = (uint16_t)(kpa * 1000.0f);
    uint16_t depth_encoded = (uint16_t)(depth_m * 1000.0f);
    uint16_t vol_encoded = (uint16_t)(vol_L);

    loraPayload[0] = (v_encoded >> 8) & 0xFF;
    loraPayload[1] = v_encoded & 0xFF;
    loraPayload[2] = (kpa_encoded >> 8) & 0xFF;
    loraPayload[3] = kpa_encoded & 0xFF;
    loraPayload[4] = (depth_encoded >> 8) & 0xFF;
    loraPayload[5] = depth_encoded & 0xFF;
    loraPayload[6] = (vol_encoded >> 8) & 0xFF;
    loraPayload[7] = vol_encoded & 0xFF;

    LMIC_setTxData2(1, loraPayload, sizeof(loraPayload), 0);
    Serial.println(F("LoRa packet queued"));
    Serial.print(F("  Voltage: ")); Serial.print(v, 3); Serial.println(F(" V"));
    Serial.print(F("  Pressure: ")); Serial.print(kpa, 3); Serial.println(F(" kPa"));
    Serial.print(F("  Depth: ")); Serial.print(depth_m, 3); Serial.println(F(" m"));
    Serial.print(F("  Volume: ")); Serial.print(vol_L, 2); Serial.println(F(" L"));
  }
}

void onEvent(ev_t ev) {
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
      LMIC_setLinkCheckMode(0);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        Serial.print(F("Received "));
        Serial.print(LMIC.dataLen);
        Serial.println(F(" bytes of payload"));
      }
      loraSending = false;
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
      loraSending = true;
      break;
    case EV_TXCANCELED:
      Serial.println(F("EV_TXCANCELED"));
      loraSending = false;
      break;
    case EV_RXSTART:
      Serial.println(F("EV_RXSTART"));
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

// -------------------- setup() and loop() --------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }

  Serial.println(F("\n=== Water Tank Sensor with LoRaWAN + WiFi ==="));
  Serial.print(F("Tank diameter: "));
  Serial.print(TANK_DIAMETER_MM);
  Serial.println(F(" mm"));

  // Initialize LoRaWAN
  Serial.println(F("Initializing LoRaWAN..."));
  os_init();
  LMIC_reset();

  // Configure for AU915 region - select FSB2 (channels 8-15 + channel 65)
  // Disable all 72 channels first
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

  // Allow additional clock error (helps with some R4 timing/join issues)
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);

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

  // Send data via LoRa if joined and interval elapsed
  unsigned long now = millis();
  if (loraJoined && !loraSending && (now - lastLoRaUploadTime >= loraUploadInterval)) {
    lastLoRaUploadTime = now;
    do_send(&sendjob);
  }

  // Send data via WiFi at different interval
  if (now - lastWiFiUploadTime >= wifiUploadInterval) {
    lastWiFiUploadTime = now;

    float v = readA0VoltageAveraged();
    float kpa = voltageToKpa(v);
    float rho = 1000.0f;
    float g = 9.81f;
    float depth_m = (kpa * 1000.0f) / (rho * g);
    float vol_m3 = 3.14159f * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
    float vol_L = vol_m3 * 1000.0f;

    sendDataViaWiFi(v, kpa, depth_m, vol_L);
  }
}

