#ifndef TEST_FUNCTIONS_H
#define TEST_FUNCTIONS_H

#ifdef UNIT_TEST
#include "Arduino.h"
#include "WiFiS3.h"
#else
#include <WiFiS3.h>
#endif

// Sensor configuration (from main file)
extern const float ADC_REF_V;
extern const int ADC_MAX;
extern const float V_MIN;
extern const float V_MAX;
extern const float FS_KPA;

// WiFi credentials
extern const char* ssid;
extern const char* password;
extern const char* serverHost;
extern const int serverPort;

// WiFi client
extern WiFiClient client;

// Function declarations
float clampf(float x, float a, float b);
float voltageToKpa(float v);
float readA0VoltageAveraged();
void connectWiFi();
void uploadToServer(float depth_m, float pressure_kpa, float volume_liters);

// Mock control functions (only available in tests)
#ifdef UNIT_TEST
extern "C" {
    void mock_set_millis(unsigned long value);
    void mock_set_wifi_status(int status);
    void mock_set_analog_value(int value);
    void mock_set_client_connected(bool connected);
    void mock_reset();
}
#endif

#endif
