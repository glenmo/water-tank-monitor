#include "Arduino.h"
#include "WiFiS3.h"
#include <time.h>

// Global state
MockSerial Serial;
MockWiFiClass WiFi;

static unsigned long mock_millis_value = 0;
static int mock_wifi_status = WL_DISCONNECTED;
static int mock_analog_value = 512;
static bool mock_client_connected = false;

// Arduino mock implementations
unsigned long millis() {
    return mock_millis_value;
}

void delay(unsigned long ms) {
    mock_millis_value += ms;
}

int analogRead(uint8_t pin) {
    return mock_analog_value;
}

// WiFi mock implementations
void MockWiFiClass::begin(const char* ssid, const char* pass) {
    // Simulate connection attempt
}

int MockWiFiClass::status() {
    return mock_wifi_status;
}

IPAddress MockWiFiClass::localIP() {
    return IPAddress(192, 168, 1, 100);
}

bool MockWiFiClient::connect(const char* host, int port) {
    return mock_client_connected;
}

void MockWiFiClient::stop() {
}

size_t MockWiFiClient::print(const char* str) {
    return strlen(str);
}

size_t MockWiFiClient::print(const String& str) {
    return strlen(str.c_str());
}

int MockWiFiClient::available() {
    return 0;
}

String MockWiFiClient::readStringUntil(char terminator) {
    return String("HTTP/1.1 200 OK");
}

// Test helper functions to control mock behavior
extern "C" {
    void mock_set_millis(unsigned long value) {
        mock_millis_value = value;
    }
    
    void mock_set_wifi_status(int status) {
        mock_wifi_status = status;
    }
    
    void mock_set_analog_value(int value) {
        mock_analog_value = value;
    }
    
    void mock_set_client_connected(bool connected) {
        mock_client_connected = connected;
    }
    
    void mock_reset() {
        mock_millis_value = 0;
        mock_wifi_status = WL_DISCONNECTED;
        mock_analog_value = 512;
        mock_client_connected = false;
    }
}
