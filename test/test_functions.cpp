#define UNIT_TEST
#include "test_functions.h"

// Sensor configuration
const float ADC_REF_V = 5.00f;
const int   ADC_MAX   = 1023;
const float V_MIN  = 0.50f;
const float V_MAX  = 4.50f;
const float FS_KPA = 10.0f;

// WiFi credentials
const char* ssid = "IOT";
const char* password = "GU23enY5!";

// Web server details
const char* serverHost = "192.168.55.192";
const int serverPort = 8080;

// WiFi client
WiFiClient client;

// Utility functions
float clampf(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

float voltageToKpa(float v) {
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
    Serial.println("WiFi not connected. Skipping upload.");
    return;
  }
  
  Serial.print("Connecting to server: ");
  Serial.println(serverHost);
  
  if (!client.connect(serverHost, serverPort)) {
    Serial.println("Connection to server failed!");
    return;
  }
  
  // Build URL-encoded parameters
  String url = "/update?";
  url += "depth=" + String(depth_m, 3);
  url += "&pressure=" + String(pressure_kpa, 2);
  url += "&volume=" + String(volume_liters, 2);
  
  // Send HTTP GET request
  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: " + String(serverHost) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  
  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Server timeout!");
      client.stop();
      return;
    }
  }
  
  // Read response
  Serial.println("Server response:");
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
  client.stop();
  Serial.println("\nData uploaded successfully!");
}
