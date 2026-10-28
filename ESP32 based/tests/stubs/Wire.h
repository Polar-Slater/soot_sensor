#ifndef TEST_WIRE_H
#define TEST_WIRE_H
#include <stddef.h>
#include <stdint.h>
class TwoWire {
public:
    bool begin(int sda, int scl, uint32_t frequency);
    void setTimeOut(uint16_t milliseconds);
    void beginTransmission(uint8_t address);
    size_t write(const uint8_t *buffer, size_t count);
    uint8_t endTransmission(bool stopBit);
    size_t requestFrom(uint8_t address, size_t count, bool stopBit);
    int available();
    int read();
};
extern TwoWire Wire;
#endif
