#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <stdint.h>

// Mock Arduino constants
#define PI 3.1415926535897932384626433832795
#define A0 0

// Mock Serial
class MockSerial {
public:
    void begin(unsigned long) {}
    void print(const char*) {}
    void println(const char*) {}
    void print(float, int = 2) {}
    void println(float, int = 2) {}
    void print(int) {}
    void println(int) {}
    void println() {}
    operator bool() { return true; }
};

extern MockSerial Serial;

// Mock functions
unsigned long millis();
void delay(unsigned long);
int analogRead(uint8_t);

#endif
