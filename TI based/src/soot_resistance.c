#include "soot_resistance.h"

#include <stddef.h>
#include <string.h>

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>

/* Default seven-bit addresses. */
#define MCP4728_DEFAULT_ADDRESS             0x60u
#define ADS1115_DEFAULT_ADDRESS             0x48u

/* MCP4728 settings used for channel A. */
#define MCP4728_INTERNAL_REFERENCE_VOLTS    2.048f
#define MCP4728_DAC_STEPS                   4096.0f
#define MCP4728_MULTI_WRITE_CHANNEL_A       0x40u
#define MCP4728_INTERNAL_REF_NORMAL_GAIN_1  0x80u

/* ADS1115 register pointers. */
#define ADS1115_REG_CONVERSION              0x00u
#define ADS1115_REG_CONFIG                  0x01u

/* ADS1115 configuration fields. */
#define ADS1115_OS_START_SINGLE             0x8000u
#define ADS1115_OS_CONVERSION_READY         0x8000u
#define ADS1115_MUX_AIN0_AIN1               0x0000u
#define ADS1115_MUX_AIN1_GND                0x5000u
#define ADS1115_PGA_4_096V                  0x0200u
#define ADS1115_MODE_SINGLE_SHOT            0x0100u
#define ADS1115_DATA_RATE_128_SPS           0x0080u
#define ADS1115_COMPARATOR_DISABLED         0x0003u
#define ADS1115_CONFIG_COMMON               \
    (ADS1115_OS_START_SINGLE | ADS1115_PGA_4_096V | \
     ADS1115_MODE_SINGLE_SHOT | ADS1115_DATA_RATE_128_SPS | \
     ADS1115_COMPARATOR_DISABLED)

/* At +/-4.096 V full-scale, each signed ADS1115 count represents 125 uV. */
#define ADS1115_VOLTS_PER_COUNT             0.000125f
#define ADS1115_NEAR_FULL_SCALE_CODE        32700
#define ADS1115_READY_POLL_LIMIT            25u
#define ADS1115_READY_POLL_DELAY_MS         1u

#define SOOT_DEFAULT_FEEDBACK_OHMS          1000411.0f
#define SOOT_DEFAULT_BIAS_VOLTS             1.000f
#define SOOT_DEFAULT_MIN_BIAS_VOLTS         0.800f
#define SOOT_DEFAULT_MAX_BIAS_VOLTS         1.200f
#define SOOT_DEFAULT_MIN_DELTA_VOLTS        0.0005f
#define SOOT_DEFAULT_ANALOG_SUPPLY_VOLTS    3.300f
#define SOOT_DEFAULT_SAMPLE_COUNT           16u
#define SOOT_MAX_SAMPLE_COUNT               256u
#define SOOT_OUTPUT_RAIL_MARGIN_VOLTS       0.050f

static void sleepMilliseconds(uint32_t milliseconds)
{
    uint32_t ticks;
    uint32_t microseconds;

    microseconds = milliseconds * 1000u;
    ticks = (microseconds + (uint32_t)Clock_tickPeriod - 1u) /
            (uint32_t)Clock_tickPeriod;
    if (ticks == 0u) {
        ticks = 1u;
    }
    Task_sleep(ticks);
}

static bool i2cTransfer(I2C_Handle handle,
                        uint8_t address,
                        void *writeBuffer,
                        size_t writeCount,
                        void *readBuffer,
                        size_t readCount)
{
    I2C_Transaction transaction;

    memset(&transaction, 0, sizeof(transaction));
    transaction.slaveAddress = address;
    transaction.writeBuf = writeBuffer;
    transaction.writeCount = writeCount;
    transaction.readBuf = readBuffer;
    transaction.readCount = readCount;

    return I2C_transfer(handle, &transaction);
}

static bool ads1115ReadRegister(SootResistance *device,
                                uint8_t registerAddress,
                                uint16_t *value)
{
    uint8_t writeBuffer[1];
    uint8_t readBuffer[2];

    writeBuffer[0] = registerAddress;
    if (!i2cTransfer(device->config.i2c,
                     device->config.ads1115_address,
                     writeBuffer,
                     sizeof(writeBuffer),
                     readBuffer,
                     sizeof(readBuffer))) {
        return false;
    }

    *value = ((uint16_t)readBuffer[0] << 8) | (uint16_t)readBuffer[1];
    return true;
}

static bool ads1115WriteConfig(SootResistance *device, uint16_t configWord)
{
    uint8_t writeBuffer[3];

    writeBuffer[0] = ADS1115_REG_CONFIG;
    writeBuffer[1] = (uint8_t)(configWord >> 8);
    writeBuffer[2] = (uint8_t)(configWord & 0xFFu);

    return i2cTransfer(device->config.i2c,
                       device->config.ads1115_address,
                       writeBuffer,
                       sizeof(writeBuffer),
                       NULL,
                       0u);
}

static SootResistance_Status ads1115ReadConversion(SootResistance *device,
                                                    uint16_t mux,
                                                    int16_t *rawCode)
{
    uint16_t configWord;
    uint16_t conversionWord;
    uint16_t poll;

    configWord = ADS1115_CONFIG_COMMON | mux;
    if (!ads1115WriteConfig(device, configWord)) {
        return SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR;
    }

    /*
     * The ADS1115 takes about 7.8 ms at 128 samples/s. Polling OS allows this
     * function to tolerate scheduler timing changes without reading stale data.
     */
    for (poll = 0u; poll < ADS1115_READY_POLL_LIMIT; ++poll) {
        sleepMilliseconds(ADS1115_READY_POLL_DELAY_MS);
        if (!ads1115ReadRegister(device, ADS1115_REG_CONFIG, &configWord)) {
            return SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR;
        }
        if ((configWord & ADS1115_OS_CONVERSION_READY) != 0u) {
            break;
        }
    }

    if (poll == ADS1115_READY_POLL_LIMIT) {
        return SOOT_RESISTANCE_STATUS_ADC_TIMEOUT;
    }

    if (!ads1115ReadRegister(device,
                             ADS1115_REG_CONVERSION,
                             &conversionWord)) {
        return SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR;
    }

    *rawCode = (int16_t)conversionWord;
    return SOOT_RESISTANCE_STATUS_OK;
}

static SootResistance_Status writeMcp4728ChannelA(SootResistance *device)
{
    uint8_t writeBuffer[3];
    uint16_t dacCode;
    float unroundedCode;

    /*
     * Internal VREF = 2.048 V, gain = 1:
     * DAC code = requested voltage * 4096 / 2.048.
     * A 1.000 V request therefore produces code 2000 exactly.
     */
    unroundedCode = device->config.requested_bias_volts *
                    MCP4728_DAC_STEPS /
                    MCP4728_INTERNAL_REFERENCE_VOLTS;
    dacCode = (uint16_t)(unroundedCode + 0.5f);
    if (dacCode > 4095u) {
        dacCode = 4095u;
    }

    /* Multi-write channel A, update immediately, normal mode, gain = 1. */
    writeBuffer[0] = MCP4728_MULTI_WRITE_CHANNEL_A;
    writeBuffer[1] = MCP4728_INTERNAL_REF_NORMAL_GAIN_1 |
                     (uint8_t)((dacCode >> 8) & 0x0Fu);
    writeBuffer[2] = (uint8_t)(dacCode & 0xFFu);

    if (!i2cTransfer(device->config.i2c,
                     device->config.mcp4728_address,
                     writeBuffer,
                     sizeof(writeBuffer),
                     NULL,
                     0u)) {
        return SOOT_RESISTANCE_STATUS_DAC_I2C_ERROR;
    }

    /* MCP4728 settling is much shorter; 1 ms is deliberately conservative. */
    sleepMilliseconds(1u);
    return SOOT_RESISTANCE_STATUS_OK;
}

void SootResistance_configDefaults(SootResistance_Config *config,
                                   I2C_Handle sharedI2cHandle)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->i2c = sharedI2cHandle;
    config->mcp4728_address = MCP4728_DEFAULT_ADDRESS;
    config->ads1115_address = ADS1115_DEFAULT_ADDRESS;
    config->feedback_resistance_ohms = SOOT_DEFAULT_FEEDBACK_OHMS;
    config->requested_bias_volts = SOOT_DEFAULT_BIAS_VOLTS;
    config->minimum_valid_bias_volts = SOOT_DEFAULT_MIN_BIAS_VOLTS;
    config->maximum_valid_bias_volts = SOOT_DEFAULT_MAX_BIAS_VOLTS;
    config->minimum_valid_delta_volts = SOOT_DEFAULT_MIN_DELTA_VOLTS;
    config->analog_supply_volts = SOOT_DEFAULT_ANALOG_SUPPLY_VOLTS;
    config->sample_count = SOOT_DEFAULT_SAMPLE_COUNT;
}

SootResistance_Status SootResistance_init(
    SootResistance *device,
    const SootResistance_Config *config)
{
    uint16_t adsConfig;
    SootResistance_Status status;

    if ((device == NULL) || (config == NULL) || (config->i2c == NULL) ||
        (config->feedback_resistance_ohms <= 0.0f) ||
        (config->requested_bias_volts <= 0.0f) ||
        (config->requested_bias_volts >= MCP4728_INTERNAL_REFERENCE_VOLTS) ||
        (config->sample_count == 0u) ||
        (config->sample_count > SOOT_MAX_SAMPLE_COUNT)) {
        return SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT;
    }

    memset(device, 0, sizeof(*device));
    device->config = *config;

    status = writeMcp4728ChannelA(device);
    if (status != SOOT_RESISTANCE_STATUS_OK) {
        return status;
    }

    /* Reading the configuration register verifies the ADS1115 address/ACK. */
    if (!ads1115ReadRegister(device, ADS1115_REG_CONFIG, &adsConfig)) {
        return SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR;
    }

    device->initialized = true;
    return SOOT_RESISTANCE_STATUS_OK;
}

SootResistance_Status SootResistance_setBias(SootResistance *device)
{
    if (device == NULL) {
        return SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT;
    }
    if (!device->initialized) {
        return SOOT_RESISTANCE_STATUS_NOT_INITIALIZED;
    }

    return writeMcp4728ChannelA(device);
}

SootResistance_Status SootResistance_read(
    SootResistance *device,
    SootResistance_Measurement *measurement)
{
    int32_t biasCodeSum;
    int32_t deltaCodeSum;
    int16_t biasCode;
    int16_t deltaCode;
    uint16_t sample;
    bool saturated;
    SootResistance_Status status;

    if ((device == NULL) || (measurement == NULL)) {
        return SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT;
    }
    if (!device->initialized) {
        return SOOT_RESISTANCE_STATUS_NOT_INITIALIZED;
    }

    memset(measurement, 0, sizeof(*measurement));
    biasCodeSum = 0;
    deltaCodeSum = 0;
    saturated = false;

    for (sample = 0u; sample < device->config.sample_count; ++sample) {
        /* A1 is the buffered DAC bias voltage. */
        status = ads1115ReadConversion(device, ADS1115_MUX_AIN1_GND,
                                       &biasCode);
        if (status != SOOT_RESISTANCE_STATUS_OK) {
            return status;
        }

        /* A0 - A1 is the voltage developed across the feedback resistor. */
        status = ads1115ReadConversion(device, ADS1115_MUX_AIN0_AIN1,
                                       &deltaCode);
        if (status != SOOT_RESISTANCE_STATUS_OK) {
            return status;
        }

        biasCodeSum += (int32_t)biasCode;
        deltaCodeSum += (int32_t)deltaCode;

        if ((biasCode >= ADS1115_NEAR_FULL_SCALE_CODE) ||
            (biasCode <= -ADS1115_NEAR_FULL_SCALE_CODE) ||
            (deltaCode >= ADS1115_NEAR_FULL_SCALE_CODE) ||
            (deltaCode <= -ADS1115_NEAR_FULL_SCALE_CODE)) {
            saturated = true;
        }
    }

    measurement->average_bias_code =
        (int16_t)(biasCodeSum / (int32_t)device->config.sample_count);
    measurement->average_delta_code =
        (int16_t)(deltaCodeSum / (int32_t)device->config.sample_count);
    measurement->bias_volts =
        (float)measurement->average_bias_code * ADS1115_VOLTS_PER_COUNT;
    measurement->delta_volts =
        (float)measurement->average_delta_code * ADS1115_VOLTS_PER_COUNT;
    measurement->output_volts = measurement->bias_volts +
                                measurement->delta_volts;
    measurement->current_amps = measurement->delta_volts /
                                device->config.feedback_resistance_ohms;

    if (saturated ||
        (measurement->output_volts >=
         (device->config.analog_supply_volts -
          SOOT_OUTPUT_RAIL_MARGIN_VOLTS))) {
        return SOOT_RESISTANCE_STATUS_ADC_SATURATED;
    }

    if ((measurement->bias_volts <
         device->config.minimum_valid_bias_volts) ||
        (measurement->bias_volts >
         device->config.maximum_valid_bias_volts)) {
        return SOOT_RESISTANCE_STATUS_BIAS_OUT_OF_RANGE;
    }

    /*
     * A zero or negative delta means an open circuit, a reversed current,
     * excessive resistance, or wiring outside the expected topology.  The
     * threshold prevents unstable division when the signal is near ADC noise.
     */
    if (measurement->delta_volts <=
        device->config.minimum_valid_delta_volts) {
        return SOOT_RESISTANCE_STATUS_OPEN_OR_OUT_OF_RANGE;
    }

    /*
     * At the TIA summing node:
     *   I_sensor = V_bias / R_sensor
     *   V_output - V_bias = I_sensor * R_feedback
     * Therefore R_sensor = R_feedback * V_bias / (V_output - V_bias).
     */
    measurement->resistance_ohms =
        device->config.feedback_resistance_ohms *
        measurement->bias_volts /
        measurement->delta_volts;

    return SOOT_RESISTANCE_STATUS_OK;
}

const char *SootResistance_statusString(SootResistance_Status status)
{
    switch (status) {
    case SOOT_RESISTANCE_STATUS_OK:
        return "OK";
    case SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case SOOT_RESISTANCE_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case SOOT_RESISTANCE_STATUS_DAC_I2C_ERROR:
        return "DAC_I2C_ERROR";
    case SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR:
        return "ADC_I2C_ERROR";
    case SOOT_RESISTANCE_STATUS_ADC_TIMEOUT:
        return "ADC_TIMEOUT";
    case SOOT_RESISTANCE_STATUS_ADC_SATURATED:
        return "ADC_SATURATED";
    case SOOT_RESISTANCE_STATUS_BIAS_OUT_OF_RANGE:
        return "BIAS_OUT_OF_RANGE";
    case SOOT_RESISTANCE_STATUS_OPEN_OR_OUT_OF_RANGE:
        return "OPEN_OR_OUT_OF_RANGE";
    default:
        return "UNKNOWN_STATUS";
    }
}
