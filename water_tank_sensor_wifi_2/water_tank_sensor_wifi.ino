#include <WiFiS3.h>

// WiFi credentials
const char* ssid = "IOT";        // Replace with your WiFi name
const char* password = "GU23enY5!"; // Replace with your WiFi password

// Web server details
const char* serverHost = "192.168.55.192";  // rubberduck.local
const int serverPort = 8080;  // Changed from 80 to avoid conflict

// Sensor configuration
const float ADC_REF_V = 5.00f;
const int   ADC_MAX   = 1023;
const float V_MIN  = 0.50f;
const float V_MAX  = 4.50f;
const float FS_KPA = 10.0f;

// Tank configuration
const float TANK_DIAMETER_MM = 100.0f;  // 100mm diameter
const float TANK_RADIUS_M = (TANK_DIAMETER_MM / 2.0f) / 1000.0f;  // Convert to meters
// PI is already defined in Arduino.h

// Timing
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 5000;  // Upload every 5 seconds

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

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
  Serial.println("\n=== Water Tank Sensor with WiFi ===");
  Serial.println("Tank diameter: " + String(TANK_DIAMETER_MM) + "mm");
  
  // Connect to WiFi
  connectWiFi();
  
  Serial.println("Setup complete. Starting measurements...\n");
}

void loop() {
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectWiFi();
  }
  
  // Read sensor
  float voltage = readA0VoltageAveraged();
  float pressure_kpa = voltageToKpa(voltage);
  
  // Convert pressure to depth (kPa to meters of water)
  // 1 kPa ≈ 0.102 meters of water (density = 1000 kg/m³, g = 9.81 m/s²)
  float depth_m = pressure_kpa * 0.10197162f;
  
  // Calculate volume (cylindrical tank)
  // V = π × r² × h
  float volume_m3 = PI * TANK_RADIUS_M * TANK_RADIUS_M * depth_m;
  float volume_liters = volume_m3 * 1000.0f;  // Convert to liters
  
  // Display readings
  Serial.println("--- Measurement ---");
  Serial.print("Voltage: ");
  Serial.print(voltage, 3);
  Serial.println(" V");
  
  Serial.print("Pressure: ");
  Serial.print(pressure_kpa, 2);
  Serial.println(" kPa");
  
  Serial.print("Water Depth: ");
  Serial.print(depth_m, 3);
  Serial.println(" m");
  
  Serial.print("Tank Capacity: ");
  Serial.print(volume_liters, 2);
  Serial.println(" liters");
  Serial.println();
  
  // Upload to server at specified interval
  unsigned long currentTime = millis();
  if (currentTime - lastUploadTime >= uploadInterval) {
    uploadToServer(depth_m, pressure_kpa, volume_liters);
    lastUploadTime = currentTime;
  }
  
  delay(1000);  // Wait 1 second before next reading
}
