#ifndef TEST_I2C_H
#define TEST_I2C_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef void *I2C_Handle;
typedef struct {
    uint8_t slaveAddress;
    void *writeBuf;
    size_t writeCount;
    void *readBuf;
    size_t readCount;
} I2C_Transaction;
typedef struct { int transferMode; int bitRate; } I2C_Params;
#define I2C_MODE_BLOCKING 0
#define I2C_100kHz 100000
bool I2C_transfer(I2C_Handle handle, I2C_Transaction *transaction);
void I2C_Params_init(I2C_Params *params);
I2C_Handle I2C_open(int index, I2C_Params *params);
#endif
