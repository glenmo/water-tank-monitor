#ifndef WIFIS3_MOCK_H
#define WIFIS3_MOCK_H

#include <string.h>

// WiFi status codes
#define WL_CONNECTED 3
#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_CONNECT_FAILED 4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED 6

class IPAddress {
public:
    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {}
};

class MockWiFiClient {
public:
    bool connect(const char* host, int port);
    void stop();
    size_t print(const char* str);
    size_t print(const String& str);
    int available();
    String readStringUntil(char terminator);
};

class MockWiFiClass {
public:
    void begin(const char* ssid, const char* pass);
    int status();
    IPAddress localIP();
};

extern MockWiFiClass WiFi;
typedef MockWiFiClient WiFiClient;

// String class mock
class String {
public:
    String() : _str(nullptr) {}
    String(const char* str) {
        if (str) {
            _str = new char[strlen(str) + 1];
            strcpy(_str, str);
        } else {
            _str = nullptr;
        }
    }
    String(float val, int decimals) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        _str = new char[strlen(buf) + 1];
        strcpy(_str, buf);
    }
    ~String() { delete[] _str; }
    
    String operator+(const String& other) const {
        if (!_str && !other._str) return String();
        if (!_str) return String(other._str);
        if (!other._str) return String(_str);
        
        char* newStr = new char[strlen(_str) + strlen(other._str) + 1];
        strcpy(newStr, _str);
        strcat(newStr, other._str);
        String result(newStr);
        delete[] newStr;
        return result;
    }
    
    const char* c_str() const { return _str ? _str : ""; }
    
private:
    char* _str;
};

#endif
