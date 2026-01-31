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
const int serverPort = 8081;

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

// ---------- Existing average method ----------
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

// ---------- New median filter method ----------
static const int MEDIAN_SAMPLES = 51; // must be odd; try 21/31/51

float readA0VoltageMedian(int samples = MEDIAN_SAMPLES) {
  if (samples < 5) samples = 5;
  if (samples > 101) samples = 101;
  if ((samples % 2) == 0) samples++; // force odd

  static int vals[101];

  for (int i = 0; i < samples; i++) {
    vals[i] = analogRead(A0);
    delay(2);
  }

  // insertion sort (fast enough for <=101 samples)
  for (int i = 1; i < samples; i++) {
    int key = vals[i];
    int j = i - 1;
    while (j >= 0 && vals[j] > key) {
      vals[j + 1] = vals[j];
      j--;
    }
    vals[j + 1] = key;
  }

  int med = vals[samples / 2];
  return ((float)med * ADC_REF_V) / (float)ADC_MAX;
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
    // Use MEDIAN filtered voltage
    float v = readA0VoltageMedian();
    float kpa = voltageToKpa(v);
    float rho = 1000.0f;
    float g = 9.81f;
    float depth_m = (kpa * 1000.0f) / (rho * g);

    float vol_m3 = 3.14159f * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
    float vol_L = vol_m3 * 1000.0f;

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
    Serial.print(F("  Voltage (median): ")); Serial.print(v, 3); Serial.println(F(" V"));
    Serial.print(F("  Pressure: ")); Serial.print(kpa, 3); Serial.println(F(" kPa"));
    Serial.print(F("  Depth: ")); Serial.print(depth_m, 3); Serial.println(F(" m"));
    Serial.print(F("  Volume: ")); Serial.print(vol_L, 2); Serial.println(F(" L"));
  }
}

void onEvent(ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch(ev) {
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
    case EV_TXSTART:
      Serial.println(F("EV_TXSTART"));
      loraSending = true;
      break;
    default:
      break;
  }
}

// -------------------- setup() and loop() --------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  Serial.println(F("\n=== Water Tank Sensor with LoRaWAN + WiFi ==="));

  Serial.println(F("Initializing LoRaWAN..."));
  os_init();
  LMIC_reset();

  for (int c = 0; c < 72; c++) LMIC_disableChannel(c);
  for (int c = 8; c < 16; c++) LMIC_enableChannel(c);
  LMIC_enableChannel(65);

  LMIC_setDrTxpow(DR_SF7, 14);
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);

  Serial.println(F("Starting OTAA join..."));
  LMIC_startJoining();

  Serial.println(F("Connecting to WiFi backup..."));
  connectWiFi();

  Serial.println(F("Setup complete.\n"));
}

void loop() {
  os_runloop_once();

  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) connectWiFi();
    lastWiFiCheck = millis();
  }

  unsigned long now = millis();

  if (loraJoined && !loraSending && (now - lastLoRaUploadTime >= loraUploadInterval)) {
    lastLoRaUploadTime = now;
    do_send(&sendjob);
  }

  if (now - lastWiFiUploadTime >= wifiUploadInterval) {
    lastWiFiUploadTime = now;

    // Use MEDIAN filtered voltage for WiFi as well
    float v = readA0VoltageMedian();
    float kpa = voltageToKpa(v);
    float rho = 1000.0f;
    float g = 9.81f;
    float depth_m = (kpa * 1000.0f) / (rho * g);
    float vol_m3 = 3.14159f * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
    float vol_L = vol_m3 * 1000.0f;

    sendDataViaWiFi(v, kpa, depth_m, vol_L);
  }
}
#define CFG_au915 1
#define CFG_sx1276_radio 1

#include <WiFiS3.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// ... rest of your code ...

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println(F("\n=== SPI Test for SX1276 ==="));
  
  // Initialize pins
  pinMode(10, OUTPUT);  // NSS/CS
  digitalWrite(10, HIGH);
  pinMode(9, OUTPUT);   // RST
  digitalWrite(9, HIGH);
  
  SPI.begin();
  
  // Hardware reset
  Serial.println(F("Resetting radio..."));
  digitalWrite(9, LOW);
  delay(100);
  digitalWrite(9, HIGH);
  delay(100);
  
  // Read version register (0x42)
  digitalWrite(10, LOW);
  SPI.transfer(0x42 & 0x7F);  // Address, read mode
  uint8_t version = SPI.transfer(0x00);
  digitalWrite(10, HIGH);
  
  Serial.print(F("SX1276 Version Register: 0x"));
  Serial.println(version, HEX);
  
  if (version == 0x12) {
    Serial.println(F("✓✓✓ SX1276 DETECTED! ✓✓✓"));
  } else if (version == 0x00 || version == 0xFF) {
    Serial.println(F("✗✗✗ NO RESPONSE FROM RADIO ✗✗✗"));
    Serial.println(F("Check:"));
    Serial.println(F("  - Shield properly seated"));
    Serial.println(F("  - No bent pins"));
    Serial.println(F("  - Correct SPI connections"));
    while(1) { delay(1000); }  // Stop here
  } else {
    Serial.print(F("⚠ Unexpected version: 0x"));
    Serial.println(version, HEX);
    Serial.println(F("Continuing anyway..."));
  }
  
  Serial.println(F("\nInitializing LoRaWAN..."));
  
  // Continue with your existing setup
  os_init();
  LMIC_reset();
  
  // Use simpler channel selection
  LMIC_selectSubBand(1);  // Sub-band 2 (channels 8-15)
  
  LMIC_setDrTxpow(DR_SF7, 14);
  LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);  // Increase tolerance
  
  Serial.println(F("Starting OTAA join..."));
  LMIC_startJoining();
  
  Serial.println(F("Connecting to WiFi backup..."));
  connectWiFi();
  
  Serial.println(F("Setup complete.\n"));
}

