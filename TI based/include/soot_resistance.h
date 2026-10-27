#ifndef SOOT_RESISTANCE_H_
#define SOOT_RESISTANCE_H_

/*
 * CC2650/TI-RTOS driver for the soot-sensor resistance measurement circuit.
 *
 * This module contains no radio, 6TiSCH, CoAP, or gateway code.  It only:
 *   1. selects ADG728 feedback and programs MCP4728 channel A for bias;
 *   2. reads the buffered bias and TIA output through the ADS1115; and
 *   3. calculates the resistance between the two sensor clamps.
 */

#include <stdbool.h>
#include <stdint.h>

#include <ti/drivers/I2C.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOOT_RESISTANCE_VERSION_MAJOR 0u
#define SOOT_RESISTANCE_VERSION_MINOR 2u
#define SOOT_RESISTANCE_VERSION_PATCH 0u
#define SOOT_RESISTANCE_RANGE_COUNT 4u

typedef enum {
    SOOT_RESISTANCE_STATUS_OK = 0,
    SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT,
    SOOT_RESISTANCE_STATUS_NOT_INITIALIZED,
    SOOT_RESISTANCE_STATUS_DAC_I2C_ERROR,
    SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR,
    SOOT_RESISTANCE_STATUS_ADC_TIMEOUT,
    SOOT_RESISTANCE_STATUS_ADC_SATURATED,
    SOOT_RESISTANCE_STATUS_BIAS_OUT_OF_RANGE,
    SOOT_RESISTANCE_STATUS_OPEN_OR_OUT_OF_RANGE,
    SOOT_RESISTANCE_STATUS_SWITCH_I2C_ERROR,
    SOOT_RESISTANCE_STATUS_SWITCH_STATE_ERROR
} SootResistance_Status;

typedef struct {
    /* Existing shared TI-RTOS I2C handle. The caller owns this handle. */
    I2C_Handle i2c;

    /* Seven-bit I2C addresses. Do not left-shift these values. */
    uint8_t mcp4728_address;
    uint8_t ads1115_address;
    uint8_t adg728_address;

    /* S1..S4, ascending resistance. Replace nominal values with calibration. */
    float feedback_resistance_ohms[SOOT_RESISTANCE_RANGE_COUNT];
    float switch_resistance_ohms[SOOT_RESISTANCE_RANGE_COUNT];
    uint32_t feedback_settle_ms;

    /* Analog circuit constants. */
    float requested_bias_volts;
    float minimum_valid_bias_volts;
    float maximum_valid_bias_volts;
    float minimum_valid_delta_volts;
    float analog_supply_volts;

    /* Number of A1 and A0-A1 conversions averaged per result. */
    uint16_t sample_count;
} SootResistance_Config;

typedef struct {
    float bias_volts;          /* ADS1115 A1 relative to GND. */
    float output_volts;        /* TIA output on ADS1115 A0. */
    float delta_volts;         /* A0 - A1, voltage across feedback network. */
    float current_amps;        /* delta_volts / feedback resistance. */
    float resistance_ohms;     /* Resistance between clamp 1 and clamp 2. */
    float feedback_resistance_ohms; /* Selected resistor + switch correction. */
    uint8_t feedback_channel;  /* Physical ADG728 channel, 1..4. */
    int16_t average_bias_code;
    int16_t average_delta_code;
} SootResistance_Measurement;

typedef struct {
    SootResistance_Config config;
    bool initialized;
    uint8_t active_range;      /* Internal zero-based S1..S4 index. */
} SootResistance;

/* Load the hardware values used by the current prototype. */
void SootResistance_configDefaults(SootResistance_Config *config,
                                   I2C_Handle sharedI2cHandle);

/*
 * Select S1, configure MCP4728 channel A, and verify ADS1115 response.
 * Call from a TI-RTOS Task context after the shared I2C bus is opened.
 */
SootResistance_Status SootResistance_init(
    SootResistance *device,
    const SootResistance_Config *config);

/* Reapply bias and wait for the feedback network to settle. */
SootResistance_Status SootResistance_setBias(SootResistance *device);

/*
 * Automatically select feedback, average ADC conversions, calculate resistance.
 * Starts at S1 on every call; increases gain while delta is below 0.1 V.
 * Consume resistance/current only on OK. Failed calls clear resistance.
 * The measurement structure is populated even for most analog range errors,
 * allowing a debugger or caller to inspect the voltages that caused the error.
 */
SootResistance_Status SootResistance_read(
    SootResistance *device,
    SootResistance_Measurement *measurement);

/* Human-readable status text for logging or debugger inspection. */
const char *SootResistance_statusString(SootResistance_Status status);

#ifdef __cplusplus
}
#endif

#endif /* SOOT_RESISTANCE_H_ */
