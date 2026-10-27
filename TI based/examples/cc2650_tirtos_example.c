/*
 * Integration example for a CC2650 SensorTag TI-RTOS task.
 *
 * This is intentionally not a complete main.c and contains no wireless code.
 * Copy the relevant initialization and measurement calls into the existing
 * application's sensor/reporting task. I2C_transfer() must run in Task context.
 */

#include <stdbool.h>

#include <ti/drivers/I2C.h>

#include "Board.h"
#include "soot_resistance.h"

static SootResistance sootSensor;

bool SootSensorExample_init(I2C_Handle sharedI2cHandle)
{
    SootResistance_Config config;
    SootResistance_Status status;

    /*
     * The existing SensorTag application should open the DIO_5/DIO_6 I2C bus
     * once and pass that same handle here. Opening the same I2C instance twice
     * normally fails in TI-RTOS.
     */
    SootResistance_configDefaults(&config, sharedI2cHandle);

    /*
     * Adafruit boards normally use MCP4728 address 0x60. Some MCP4728A4
     * versions use 0x64. Change this field after confirming with an I2C scan.
     */
    config.mcp4728_address = 0x60u;
    config.ads1115_address = 0x48u; /* ADDR is wired to GND. */

    config.adg728_address = 0x4Cu; /* Default Adafruit address jumpers. */

    /*
     * S1..S4 resistor values are defined at the beginning of soot_resistance.c.
     * Defaults are 100, 1000, 10000, 100000 ohms; replace with measured values.
     * Optional per-board override: config.feedback_resistance_ohms[0] = 99.98f;
     * SootResistance_read() automatically chooses one feedback channel.
     */

    status = SootResistance_init(&sootSensor, &config);
    return status == SOOT_RESISTANCE_STATUS_OK;
}

SootResistance_Status SootSensorExample_measure(
    SootResistance_Measurement *result)
{
    /*
     * On success, result->resistance_ohms is the resistance between clamps.
     * result->feedback_channel identifies S1..S4; bias_volts/delta_volts and
     * feedback_resistance_ohms provide calibration diagnostics.
     * Coworkers can add that value to their wireless packet outside this file.
     */
    return SootResistance_read(&sootSensor, result);
}

/*
 * Example of opening the I2C bus only for a standalone test application.
 * Do not call this if the existing SensorTag firmware already owns Board_I2C.
 */
I2C_Handle SootSensorExample_openI2cForStandaloneTest(void)
{
    I2C_Params params;

    I2C_Params_init(&params);
    params.transferMode = I2C_MODE_BLOCKING;
    params.bitRate = I2C_100kHz;

    /* Board_I2C maps to SDA DIO_5 and SCL DIO_6 on the CC2650STK. */
    return I2C_open(Board_I2C, &params);
}
