// Board settings: standard ESP32 DevKit / WROOM.
// Resistor calibration values are at the top of src/soot_resistance.cpp.
constexpr int SOOT_SDA_PIN = 21;
constexpr int SOOT_SCL_PIN = 22;
constexpr unsigned long SOOT_I2C_HZ = 100000;
constexpr unsigned long SOOT_REPORT_PAUSE_MS = 1000;

#include <Arduino.h>
#include <Wire.h>
#include <soot_resistance.h>

static SootResistance sensor = {};
static SootResistance_Config config;
static bool busReady = false;

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.printf("ESP32 soot sensor %u.%u.%u\n",
                  SOOT_RESISTANCE_VERSION_MAJOR,
                  SOOT_RESISTANCE_VERSION_MINOR,
                  SOOT_RESISTANCE_VERSION_PATCH);

    busReady = Wire.begin(SOOT_SDA_PIN, SOOT_SCL_PIN, SOOT_I2C_HZ);
    if (!busReady) {
        Serial.println("I2C initialization failed. Check pins and reset the ESP32.");
        return;
    }
    Wire.setTimeOut(50); // Milliseconds per Wire transaction.
    SootResistance_configDefaults(&config, &Wire);
    config.mcp4728_address = 0x60; // Use 0x64 for MCP4728A4 boards if needed.
    config.ads1115_address = 0x48; // ADDR grounded.
    config.adg728_address = 0x4C;
    // Optional per-board calibration override before init:
    // config.feedback_resistance_ohms[0] = 99.98f;

    Serial.println("status,channel,feedback_ohm,bias_code,delta_code,bias_V,delta_V,output_V,current_A,sensor_ohm");
}

void loop()
{
    if (!busReady) {
        delay(1000);
        return;
    }
    if (!sensor.initialized) {
        const SootResistance_Status status = SootResistance_init(&sensor, &config);
        if (status != SOOT_RESISTANCE_STATUS_OK) {
            Serial.printf("# init: %s; retrying in 5 s\n", SootResistance_statusString(status));
            delay(5000);
            return;
        }
    }

    SootResistance_Measurement result = {};
    const SootResistance_Status status = SootResistance_read(&sensor, &result);
    Serial.printf("%s,%u,%.3f,%d,%d,%.6f,%.6f,%.6f,",
                  SootResistance_statusString(status),
                  static_cast<unsigned>(result.feedback_channel),
                  result.feedback_resistance_ohms,
                  static_cast<int>(result.average_bias_code),
                  static_cast<int>(result.average_delta_code),
                  result.bias_volts, result.delta_volts, result.output_volts);
    if (status == SOOT_RESISTANCE_STATUS_OK) {
        Serial.printf("%.9g,%.3f\n", result.current_amps, result.resistance_ohms);
    } else {
        Serial.println("NA,NA"); // Never publish failed reads as zero ohms.
    }
    // Acquisition adds up to about 1.5 s at default averaging, plus bus overhead.
    delay(SOOT_REPORT_PAUSE_MS);
}
