#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H
#include <stddef.h>
#include <stdint.h>
void delay(unsigned long milliseconds);
class TestSerial {
public:
    void begin(unsigned long baud);
    size_t printf(const char *format, ...);
    void println(const char *message);
};
extern TestSerial Serial;
#endif
