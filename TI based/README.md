# CC2650 soot-sensor resistance module

Version: **0.1.0**

This folder contains the C code needed for a TI CC2650 SensorTag to control the
MCP4728 and ADS1115 in the existing soot-sensor analog circuit and calculate the
resistance between its two clamps. It deliberately contains no BLE, IEEE
802.15.4, 6TiSCH, 6LoWPAN, CoAP, gateway, or packet-format code.

## Hardware assumed by the code

The CC2650 SensorTag uses its existing I2C bus:

| Signal | CC2650 | External devices |
| --- | --- | --- |
| SDA | DIO_5 | MCP4728 SDA and ADS1115 SDA |
| SCL | DIO_6 | MCP4728 SCL and ADS1115 SCL |
| Power | 3.3 V Grove VDD | MCP4728 VCC, ADS1115 VIN, MCP6002 pin 8 |
| Ground | Grove GND | All device grounds, ADS1115 ADDR, MCP6002 pin 4, clamp 2 |

The physical bus is daisy-chained from the Debug DevPack GROVE3 connector to the
MCP4728 and then to the ADS1115 with STEMMA QT cables. Electrically, the two I2C
devices are in parallel.

The analog connections are:

| Connection | Purpose |
| --- | --- |
| MCP4728 VA to MCP6002 pin 3 | Programmable bias enters op-amp A |
| MCP6002 pin 1 to pins 2 and 5 | Pin 1 is the buffered bias; pin 2 closes the follower; pin 5 biases the TIA |
| ADS1115 A1 to MCP6002 pin 1 | Measures actual buffered bias |
| ADS1115 A0 to MCP6002 pin 7 | Measures TIA output |
| 1,000,411 ohm resistor from pin 7 to pin 6 | TIA feedback/gain |
| 0.1 uF capacitor from pin 7 to pin 6 | Feedback filter, approximately 1.59 Hz with 1 Mohm |
| Clamp 1 to MCP6002 pin 6 | Sensor connects to TIA summing node |
| Clamp 2 to GND | Completes sensor bias path |
| 0.1 uF capacitor from pin 8 to pin 4 | MCP6002 local supply decoupling |

Do not connect ADS1115 ADDR to VIN. ADDR is tied to GND for address `0x48`.

## Measurement performed

MCP4728 channel A uses its internal 2.048 V reference, gain 1, and DAC code 2000:

```text
Vbias = 2.048 V * 2000 / 4096 = 1.000 V
```

The ADS1115 reads:

```text
A1 relative to GND = measured bias voltage
A0 - A1           = voltage across the TIA feedback resistor
```

The module calculates:

```text
sensor current = (A0 - A1) / Rfeedback
sensor resistance = Rfeedback * A1 / (A0 - A1)
```

The default feedback resistance is the measured value `1,000,411 ohms`. Each
result averages 16 A1 conversions and 16 A0-A1 conversions, matching the
existing Arduino test behavior.

## Files

```text
include/soot_resistance.h          Public API and measurement structures
src/soot_resistance.c              MCP4728, ADS1115, and resistance logic
examples/cc2650_tirtos_example.c   Example calls for an existing TI-RTOS task
AGENTS.md                          Handoff instructions for future maintainers
CHANGELOG.md                       Release history
VERSION                            Machine-readable version
```

## Adding this module to the existing CCS project

1. Add `src/soot_resistance.c` to the Code Composer Studio project.
2. Add this folder's `include` directory to the compiler include path.
3. Include `soot_resistance.h` in the existing sensor task.
4. Use the I2C handle that the existing firmware opened for `Board_I2C`.
5. Call `SootResistance_init()` once after the I2C bus is ready.
6. Call `SootResistance_read()` whenever a resistance measurement is needed.
7. On `SOOT_RESISTANCE_STATUS_OK`, consume `measurement.resistance_ohms`.

Minimal integration:

```c
static SootResistance soot;

void applicationSensorInit(I2C_Handle sharedI2c)
{
    SootResistance_Config config;
    SootResistance_Status status;

    SootResistance_configDefaults(&config, sharedI2c);
    status = SootResistance_init(&soot, &config);
    /* Handle status rather than ignoring an I2C wiring/address failure. */
}

SootResistance_Status applicationReadResistance(float *resistanceOhms)
{
    SootResistance_Measurement measurement;
    SootResistance_Status status;

    status = SootResistance_read(&soot, &measurement);
    if (status == SOOT_RESISTANCE_STATUS_OK) {
        *resistanceOhms = measurement.resistance_ohms;
    }
    return status;
}
```

`I2C_transfer()` must be called from a TI-RTOS Task context. Do not call the
measurement function directly from a hardware interrupt or Clock callback.

## I2C ownership

The CC2650 SensorTag already uses this I2C controller for onboard sensors. This
module accepts an existing `I2C_Handle` and never opens or closes the bus. Keep
all accesses in the existing sensor task, or use the application's established
I2C locking mechanism if multiple tasks access the same handle.

The example includes a standalone I2C-open helper only for isolated bench tests.
Do not use it when the production firmware has already opened `Board_I2C`.

## Addresses and initial bring-up

- ADS1115 with ADDR grounded: `0x48`
- MCP4728: normally `0x60`
- Some Adafruit MCP4728A4 boards: `0x64`

If initialization returns `DAC_I2C_ERROR`, scan or probe `0x60` and `0x64` and
update `mcp4728_address`. All addresses passed to TI-RTOS are seven-bit values;
do not shift them left.

Start at 100 kHz during bring-up. Both breakout boards contain I2C pull-ups.

## Expected range and status handling

With a 1 V bias, 1 Mohm feedback, and a 3.3 V supply, the TIA reaches its upper
rail at roughly 435 kohm. Allowing output headroom, approximately 500 kohm and
higher is the practical range for this feedback value.

Important statuses:

| Status | Meaning |
| --- | --- |
| `OK` | `resistance_ohms` is valid |
| `DAC_I2C_ERROR` | MCP4728 did not acknowledge or bus transfer failed |
| `ADC_I2C_ERROR` | ADS1115 did not acknowledge or bus transfer failed |
| `ADC_TIMEOUT` | ADS1115 conversion-ready bit did not assert |
| `ADC_SATURATED` | Signal reached the ADC or 3.3 V analog-output limit |
| `BIAS_OUT_OF_RANGE` | Measured A1 is outside 0.8 to 1.2 V |
| `OPEN_OR_OUT_OF_RANGE` | A0-A1 is too small/negative for a stable result |

Inspect the voltage fields in `SootResistance_Measurement` when a range status
occurs. The module populates them before returning the status.

## Toolchain target

The source uses the legacy TI-RTOS APIs available for the CC2650 SensorTag:

```c
#include <ti/drivers/I2C.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
```

It is intended to be imported into the coworkers' existing CCS/TI-RTOS 6TiSCH
project. Match that project's Code Composer Studio, TI ARM compiler, TI-RTOS,
and XDCtools versions rather than migrating the whole project during this port.

## Validation required on hardware

This repository does not contain the coworkers' full CCS project or TI-RTOS
installation, so the module cannot be linked here against the final board
configuration. During handoff, perform these checks in order:

1. Verify 3.3 V and ground at both breakout boards.
2. Verify I2C ACK at ADS1115 `0x48` and MCP4728 `0x60` or `0x64`.
3. After initialization, measure approximately 1.000 V at MCP4728 VA and
   MCP6002 pins 1, 3, and 5.
4. With clamps open, confirm `OPEN_OR_OUT_OF_RANGE` rather than a random value.
5. Test a known resistor greater than 500 kohm and compare against a multimeter.
6. Confirm other SensorTag I2C sensors still operate after sharing the bus.

## Scope boundary

Wireless communication belongs to the existing 6TiSCH application and is not
implemented here. A coworker should package `measurement.resistance_ohms` only
after this module returns `SOOT_RESISTANCE_STATUS_OK`.
