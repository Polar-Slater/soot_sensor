/* Host simulation of bus transactions and a static ideal TIA, not board QA. */
#include "soot_resistance.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

uint32_t Clock_tickPeriod = 1000u;
static uint32_t slept_ticks;
static uint8_t selected_mask, selections[16];
static unsigned selection_count, adc_reads, switch_reads;
static uint16_t adc_config;
static float sensor_ohms, bias_volts, feedback[4];
static int fail_address, mismatch_on_read, saturation_mask;
static bool adc_timeout;

void Task_sleep(uint32_t ticks) { assert(ticks > 0u); slept_ticks += ticks; }

bool I2C_transfer(I2C_Handle handle, I2C_Transaction *t)
{
    uint8_t *w = (uint8_t *)t->writeBuf;
    uint8_t *r = (uint8_t *)t->readBuf;
    uint16_t value;
    unsigned range;
    float delta;
    assert(handle != NULL);
    if ((int)t->slaveAddress == fail_address) { return false; }
    if (t->slaveAddress == 0x4Cu) {
        if (t->writeCount != 0u) {
            assert(t->writeCount == 1u && t->readCount == 0u);
            assert(w[0] == 1u || w[0] == 2u || w[0] == 4u || w[0] == 8u);
            assert(selection_count < sizeof(selections));
            selected_mask = w[0];
            selections[selection_count++] = w[0];
        } else {
            assert(t->readCount == 1u);
            ++switch_reads;
            r[0] = ((int)switch_reads == mismatch_on_read) ? 0u : selected_mask;
        }
        return true;
    }
    if (t->slaveAddress == 0x60u) {
        assert(selected_mask != 0u); /* Feedback must precede applying bias. */
        assert(t->writeCount == 3u && t->readCount == 0u);
        assert(w[0] == 0x40u && w[1] == 0x87u && w[2] == 0xD0u);
        return true;
    }
    assert(t->slaveAddress == 0x48u);
    if (t->writeCount == 3u) {
        assert(t->readCount == 0u && w[0] == 1u);
        adc_config = (uint16_t)(((uint16_t)w[1] << 8) | w[2]);
        assert((adc_config & 0x8FFFu) == 0x8383u);
        return true;
    }
    assert(t->writeCount == 1u && t->readCount == 2u);
    if (w[0] == 1u) {
        value = adc_timeout ? (uint16_t)(adc_config & ~0x8000u) :
                             (uint16_t)(adc_config | 0x8000u);
    } else {
        assert(w[0] == 0u);
        ++adc_reads;
        if ((adc_config & 0x7000u) == 0x5000u) {
            value = (uint16_t)(int16_t)lroundf(bias_volts / 0.000125f);
        } else {
            assert((adc_config & 0x7000u) == 0u);
            for (range = 0u; selected_mask != (1u << range); ++range) {
                assert(range < 4u);
            }
            delta = sensor_ohms == 0.0f ? 3.0f :
                    bias_volts * feedback[range] / sensor_ohms;
            if (selected_mask == saturation_mask || delta > 2.3f) { delta = 2.3f; }
            value = (uint16_t)(int16_t)lroundf(delta / 0.000125f);
        }
    }
    r[0] = (uint8_t)(value >> 8);
    r[1] = (uint8_t)value;
    return true;
}

static SootResistance_Config reset(void)
{
    SootResistance_Config config;
    SootResistance_configDefaults(&config, (I2C_Handle)&selected_mask);
    config.sample_count = 1u;
    selected_mask = 0u;
    selection_count = adc_reads = switch_reads = 0u;
    slept_ticks = 0u;
    adc_config = 0u;
    sensor_ohms = 5000.0f;
    bias_volts = 1.0f;
    fail_address = mismatch_on_read = saturation_mask = -1;
    adc_timeout = false;
    memcpy(feedback, config.feedback_resistance_ohms, sizeof(feedback));
    return config;
}

static void near(float actual, float expected, float relative)
{
    assert(fabsf(actual - expected) <= fabsf(expected) * relative);
}

int main(void)
{
    SootResistance device;
    SootResistance_Config config;
    SootResistance_Measurement result;
    SootResistance_Status status;
    unsigned range;
    const float loads[] = {500.0f, 5000.0f, 50000.0f, 500000.0f};

    for (range = 0u; range < 4u; ++range) {
        config = reset();
        config.sample_count = 16u;
        sensor_ohms = loads[range];
        assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
        assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_OK);
        assert(result.feedback_channel == range + 1u);
        assert(selection_count == range + 2u);
        assert(adc_reads == (range + 1u) * 32u);
        near(result.resistance_ohms, sensor_ohms, 0.001f);
        near(result.current_amps, bias_volts / sensor_ohms, 0.001f);
        near(result.delta_volts, 0.2f, 0.001f);
        assert(slept_ticks >= (range + 3u) * config.feedback_settle_ms);
    }

    /* Large gain from last call must not persist when the sensor changes. */
    sensor_ohms = 500.0f;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_OK);
    assert(result.feedback_channel == 1u);
    near(result.resistance_ohms, sensor_ohms, 0.001f);

    config = reset();
    config.feedback_resistance_ohms[0] = 101.0f;
    config.switch_resistance_ohms[0] = 6.0f;
    feedback[0] = 107.0f;
    sensor_ohms = 535.0f;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_OK);
    assert(result.feedback_resistance_ohms == 107.0f);
    near(result.resistance_ohms, sensor_ohms, 0.001f);

    config = reset();
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
    sensor_ohms = INFINITY;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_OPEN_OR_OUT_OF_RANGE);
    assert(result.feedback_channel == 4u && result.resistance_ohms == 0.0f);
    sensor_ohms = 10000000.0f; /* S4 valid below target, above minimum delta. */
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_OK);
    near(result.resistance_ohms, sensor_ohms, 0.001f);
    sensor_ohms = 0.0f;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_ADC_SATURATED);
    assert(result.feedback_channel == 1u && result.resistance_ohms == 0.0f);

    config = reset();
    saturation_mask = 2; /* A load change while stepping up forces fresh fallback. */
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_OK);
    assert(selection_count == 4u && selections[3] == 1u && adc_reads == 6u);
    near(result.resistance_ohms, sensor_ohms, 0.001f);

    config = reset();
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
    bias_volts = 0.7f;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_BIAS_OUT_OF_RANGE);
    assert(result.resistance_ohms == 0.0f);
    bias_volts = 1.0f;
    adc_timeout = true;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_ADC_TIMEOUT);
    assert(result.resistance_ohms == 0.0f);
    adc_timeout = false;
    fail_address = 0x48;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_ADC_I2C_ERROR);
    assert(result.resistance_ohms == 0.0f);
    fail_address = 0x4C;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_SWITCH_I2C_ERROR);
    assert(!device.initialized && result.resistance_ohms == 0.0f);
    result.resistance_ohms = 123.0f;
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_NOT_INITIALIZED);
    assert(result.resistance_ohms == 0.0f);

    config = reset();
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
    mismatch_on_read = 3; /* Init read, selection read, then reset during ADC. */
    assert(SootResistance_read(&device, &result) == SOOT_RESISTANCE_STATUS_SWITCH_STATE_ERROR);
    assert(!device.initialized && result.resistance_ohms == 0.0f);

    config = reset();
    fail_address = 0x60;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_DAC_I2C_ERROR);
    assert(!device.initialized);
    config = reset();
    mismatch_on_read = 1;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_SWITCH_STATE_ERROR);
    assert(!device.initialized && adc_reads == 0u);

    config = reset();
    config.feedback_resistance_ohms[1] = 1.0f;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT);
    config = reset();
    config.feedback_resistance_ohms[0] = NAN;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT);
    config = reset();
    config.switch_resistance_ohms[0] = INFINITY;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT);
    config = reset();
    config.requested_bias_volts = NAN;
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT);
    assert(selection_count == 0u);

    config = reset();
    assert(SootResistance_init(&device, &config) == SOOT_RESISTANCE_STATUS_OK);
    slept_ticks = 0u;
    assert(SootResistance_setBias(&device) == SOOT_RESISTANCE_STATUS_OK);
    assert(slept_ticks >= config.feedback_settle_ms);
    fail_address = 0x60;
    assert(SootResistance_setBias(&device) == SOOT_RESISTANCE_STATUS_DAC_I2C_ERROR);
    assert(!device.initialized);

    status = SootResistance_read(NULL, &result);
    assert(status == SOOT_RESISTANCE_STATUS_INVALID_ARGUMENT);
    assert(strcmp(SootResistance_statusString(SOOT_RESISTANCE_STATUS_SWITCH_I2C_ERROR),
                  "SWITCH_I2C_ERROR") == 0);
    puts("PASS: auto-ranging, calibration, settling, fallback, bus/state faults, validation");
    return 0;
}
