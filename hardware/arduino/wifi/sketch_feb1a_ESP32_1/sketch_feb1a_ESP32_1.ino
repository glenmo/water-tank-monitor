#include <WiFi.h>

// Install a ModbusIP library that supports ESP32.
// Commonly the header is still named like this even on ESP32:
// #include <ModbusIP_ESP8266.h>
#include <ModbusClientTCP.h>


// WiFi (from your sketch)
const char* WIFI_SSID = "DER";
const char* WIFI_PASS = "GU23enY5!";

// Battery / BMS Modbus TCP server (from your sketch)
IPAddress bmsIp(192, 168, 11, 18);
const uint16_t bmsPort = 502; // (Most ModbusIP libs assume 502 internally; kept here for clarity)

// Placeholder register details - replace with your battery register map
const uint16_t SOC_HREG_ADDR = 100;   // holding register address (placeholder)
const float SOC_SCALE = 0.1f;         // raw * 0.1 = % (placeholder)

unsigned long lastPollMs = 0;
const unsigned long POLL_INTERVAL_MS = 1000;

ModbusIP mb;
bool socRequestPending = false;

void handleSocResponse(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  (void)transactionId;
  (void)data;

  socRequestPending = false;

  if (event != Modbus::EX_SUCCESS) {
    Serial.print("- Modbus read failed, code: ");
    Serial.println((int)event);
    return;
  }

  uint16_t raw = mb.Hreg(SOC_HREG_ADDR);
  float soc = raw * SOC_SCALE;

  Serial.print("- SOC: ");
  Serial.print(soc, 1);
  Serial.println("%");
}

static bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("- Connecting WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - start > 15000) {
      Serial.println();
      return false;
    }
  }
  Serial.println();
  Serial.print("- WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("- Nano ESP32 Modbus TCP SOC monitor (ModbusIP)");

  if (!connectWifi()) {
    Serial.println("- WiFi connect failed");
    while (true) delay(1000);
  }

  // Initialise Modbus as a TCP client
  mb.client();

  // Ensure we have local storage for the register we’ll read into
  mb.addHreg(SOC_HREG_ADDR);

  // Some ModbusIP forks allow setting port; others always use 502.
  // If your fork supports it, you may have something like:
  // mb.setPort(bmsPort);
  // If not, ignore bmsPort and ensure your BMS is on 502.
}

void loop() {
  // Must be called frequently
  mb.task();

  // Reconnect WiFi if it drops
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("- WiFi dropped, reconnecting...");
    if (!connectWifi()) {
      delay(2000);
      return;
    }
  }

  // Poll on interval
  if (millis() - lastPollMs >= POLL_INTERVAL_MS && !socRequestPending) {
    lastPollMs = millis();
    socRequestPending = true;

    // Read 1 holding register from the Modbus server:
    // readHreg(remoteIP, remoteStartReg, localDestReg, count, callback)
    bool queued = mb.readHreg(bmsIp, SOC_HREG_ADDR, SOC_HREG_ADDR, 1, handleSocResponse);

    if (!queued) {
      socRequestPending = false;
      Serial.println("- Modbus request not queued (busy or not connected)");
    }
  }
}
