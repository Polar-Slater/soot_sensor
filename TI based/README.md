# CC2650 soot-sensor resistance module

Version: **0.2.0**

This folder contains the C code needed for a TI CC2650 SensorTag to control the
MCP4728, ADG728, and ADS1115 in the soot-sensor analog circuit and calculate the
resistance between its two clamps. It deliberately contains no BLE, IEEE
802.15.4, 6TiSCH, 6LoWPAN, CoAP, gateway, or packet-format code.

## Hardware assumed by the code

The CC2650 SensorTag uses its existing I2C bus:

| Signal | CC2650 | External devices |
| --- | --- | --- |
| SDA | DIO_5 | MCP4728, ADS1115, and ADG728 SDA |
| SCL | DIO_6 | MCP4728, ADS1115, and ADG728 SCL |
| Power | 3.3 V Grove VDD | MCP4728 VCC, ADS1115 VIN, ADG728 VIN, MCP6002 pin 8 |
| Ground | Grove GND | All device grounds, ADS1115 ADDR, MCP6002 pin 4, clamp 2 |

The physical bus is daisy-chained from the Debug DevPack GROVE3 connector to the
MCP4728, ADS1115, and ADG728 with STEMMA QT cables. Electrically, the three I2C
devices are in parallel.

The analog connections are:

| Connection | Purpose |
| --- | --- |
| MCP4728 VA to MCP6002 pin 3 | Programmable bias enters op-amp A |
| MCP6002 pin 1 to pins 2 and 5 | Pin 1 is the buffered bias; pin 2 closes the follower; pin 5 biases the TIA |
| ADS1115 A1 to MCP6002 pin 1 | Measures actual buffered bias |
| ADS1115 A0 to MCP6002 pin 7 | Measures TIA output |
| ADG728 D to MCP6002 pin 6 | Common feedback node |
| ADG728 S1 through 100 ohm to MCP6002 pin 7 | Lowest feedback gain |
| ADG728 S2 through 1 kohm to MCP6002 pin 7 | Feedback range 2 |
| ADG728 S3 through 10 kohm to MCP6002 pin 7 | Feedback range 3 |
| ADG728 S4 through 100 kohm to MCP6002 pin 7 | Highest feedback gain |
| ADG728 S5..S8 unconnected | Unused channels always off |
| 0.1 uF capacitor directly from pin 7 to pin 6 | Feedback filter retained across the whole switched network |
| Clamp 1 to MCP6002 pin 6 | Sensor connects to TIA summing node |
| Clamp 2 to GND | Completes sensor bias path |
| 0.1 uF capacitor from pin 8 to pin 4 | MCP6002 local supply decoupling |

Do not connect ADS1115 ADDR to VIN. ADDR is tied to GND for address `0x48`.

Remove the previous 1 Mohm feedback resistor; do not leave it in parallel with
the new bank. Use one resistor of each labeled value, irrespective of purchase
quantities in the screenshot. All four resistor ends on the output side join
pin 7. ADG728 address pads A0/A1 are address selectors, unrelated to the ADS1115
analog inputs A0/A1. Leave its address jumpers at default for `0x4C`, and keep
RST high during operation; tie RST to 3.3 V if reset control is unused.

The existing `hardware/sensor-wiring-latest.*`, perfboard drawings, and soldering
plan show the **previous fixed-resistor revision**. They are historical references;
use the connection table above for this revision's feedback wiring.

## Calibration inputs

The first lines of `src/soot_resistance.c` contain the four values to edit later:

```c
#define SOOT_FEEDBACK_S1_OHMS 100.0f
#define SOOT_FEEDBACK_S2_OHMS 1000.0f
#define SOOT_FEEDBACK_S3_OHMS 10000.0f
#define SOOT_FEEDBACK_S4_OHMS 100000.0f
```

Replace each with its measured resistor value in ohms. Optional
`SOOT_SWITCH_S1_OHMS` through `SOOT_SWITCH_S4_OHMS` add a measured series-switch
correction and default to zero, so initial results use only the labeled values.
The ADG728 has nonzero, signal-dependent on-resistance (the datasheet specifies
6 ohms typical at a 3 V supply); neglecting it can produce appreciable error on
the 100 ohm range. Do not treat zero as a claim of zero physical switch resistance.
If calibrating the complete resistor-plus-switch path, enter that effective
value as the feedback value and leave its separate switch correction at zero.

For individual boards, the same values can be overridden in
`config.feedback_resistance_ohms[0..3]` and `config.switch_resistance_ohms[0..3]`
after `SootResistance_configDefaults()` and before initialization. Effective
values must be finite, positive, and strictly increasing. Treat the device's
configuration as immutable after init; reinitialize to apply changes.
This replaces the old scalar `config.feedback_resistance_ohms` API.

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

`Rfeedback` is the selected resistor plus its optional switch correction. Each
range evaluation averages 16 A1 conversions and 16 A0-A1 conversions.

Every read starts at S1 (100 ohms), then increases to S2, S3, and S4 while
delta is below 0.100 V. Only one channel is enabled, using masks `0x01`, `0x02`,
`0x04`, or `0x08`, with register readback checked. Switching uses one byte with
no register pointer. The ADG728 provides break-before-make switching.
The driver waits 100 ms after each selection and bias update. With the retained
0.1 uF capacitor, this is ten RC time constants at nominal 100 kohms; increase
`feedback_settle_ms` if circuit changes or bench settling tests require it.

If a higher range saturates, the driver steps back once and takes fresh data.
There are at most four forward evaluations plus one fallback; it never loops
indefinitely. A small signal on S4 is still valid if above the minimum delta
of 0.5 mV; otherwise the result is `OPEN_OR_OUT_OF_RANGE`. Saturation on S1
is reported as an error. Communication, invalid-bias, and conversion faults
stop the read. Failed reads clear `resistance_ohms`; only consume numeric
resistance/current when status is `OK`. Results include `feedback_channel`
(1..4), effective `feedback_resistance_ohms`, bias, output, and delta voltages.

At default averaging, a scan can take roughly 1.5 seconds plus scheduler/I2C
overhead. Feedback is selected before programming bias during initialization.
At power-up the ADG728 has all channels off: the existing circuit has no DC
feedback until initialization, so startup transients still need bench checking.

## Files

```text
include/soot_resistance.h          Public API and measurement structures
src/soot_resistance.c              Calibration, ADG728, DAC/ADC, resistance logic
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
I2C locking mechanism if multiple tasks access the same handle. Serialize the
entire init/read/setBias call against any other caller that changes this switch,
DAC, or ADC configuration; individual bus-transaction locking is insufficient.

The example includes a standalone I2C-open helper only for isolated bench tests.
Do not use it when the production firmware has already opened `Board_I2C`.

## Addresses and initial bring-up

- ADS1115 with ADDR grounded: `0x48`
- MCP4728: normally `0x60`
- Some Adafruit MCP4728A4 boards: `0x64`
- Adafruit ADG728: `0x4C` by default, `0x4C`..`0x4F` with address jumpers

If initialization returns `DAC_I2C_ERROR`, scan or probe `0x60` and `0x64` and
update `mcp4728_address`. All addresses passed to TI-RTOS are seven-bit values;
do not shift them left.

Start at 100 kHz during bring-up. All three breakout boards contain I2C pull-ups.

## Expected range and status handling

At 1 V bias and 3.3 V supply, the software output cutoff is 3.25 V. The ideal
lower sensor-resistance bound is therefore `Rfeedback / 2.25`; minimum accepted
delta requires sensor resistance below `Rfeedback / 0.0005`. These are equation
bounds, not guaranteed accuracy/current-drive limits:

| Channel | Nominal feedback | Ideal sensor lower bound | Ideal sensor upper bound |
| --- | --- | --- | --- |
| S1 | 100 ohm | 44.4 ohm | 200 kohm |
| S2 | 1 kohm | 444 ohm | 2 Mohm |
| S3 | 10 kohm | 4.44 kohm | 20 Mohm |
| S4 | 100 kohm | 44.4 kohm | 200 Mohm |

Low sensor resistances demand `1 V / Rsensor` current. MCP6002 output drive,
output swing under load, switch resistance, offsets, leakage, and noise limit
the usable range. In particular, the ideal 44.4 ohm bound must not be treated
as a validated operating point. Validate the intended low-resistance loads.

Important statuses:

| Status | Meaning |
| --- | --- |
| `OK` | `resistance_ohms` is valid |
| `DAC_I2C_ERROR` | MCP4728 did not acknowledge or bus transfer failed |
| `ADC_I2C_ERROR` | ADS1115 did not acknowledge or bus transfer failed |
| `SWITCH_I2C_ERROR` | ADG728 write/read failed; reinitialize before retrying |
| `SWITCH_STATE_ERROR` | ADG728 readback mismatched; reinitialize before retrying |
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

Host regression tests compile the actual driver against simulated I2C/TI-RTOS
interfaces, including all four ranges, calibration, changing loads, settling,
fallback, open/saturated inputs, and communication/reset failures:

```text
python tests/run_host_tests.py --compiler gcc
```

Clang or `--compiler zig cc` can also be used. The example is compiled to an object
against the same stubs. These tests do not establish analog accuracy, stability,
or compatibility with the final TI toolchain. The host tests and example object
compilation passed with Zig 0.13.0's C compiler using C99 and strict warnings.

This repository does not contain the coworkers' full CCS project or TI-RTOS
installation, so the module cannot be linked here against the final board
configuration. During handoff, perform these checks in order:

1. Verify 3.3 V and ground at all three breakout boards and the new feedback wiring.
2. Verify I2C ACK at ADS1115 `0x48`, MCP4728 `0x60` or `0x64`, and ADG728 `0x4C`.
3. After initialization, measure approximately 1.000 V at MCP4728 VA and
   MCP6002 pins 1, 3, and 5.
4. With clamps open, confirm `OPEN_OR_OUT_OF_RANGE` rather than a random value.
5. Test known sensor resistors of 1 kohm, 10 kohm, 100 kohm, and 1 Mohm. Expect
   roughly 0.1 V delta at nominal matching feedback decades; near a boundary,
   tolerances/ADC quantization may select the next channel. Log channel, effective
   feedback, raw codes, bias/delta voltages, and calculated resistance; compare
   against a multimeter. Confirm switch correction especially on S1.
6. Check startup, range-change settling, changing loads, saturation, and switch
   disconnection/reset. Faults must never be consumed as valid resistance.
7. Confirm other SensorTag I2C sensors still operate after sharing the bus.

Device protocol references: [Adafruit ADG728 pinouts](https://learn.adafruit.com/adafruit-adg728-1-to-8-analog-matrix-switch/pinouts)
and [Analog Devices ADG728/729 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/adg728_729.pdf).

## Scope boundary

Wireless communication belongs to the existing 6TiSCH application and is not
implemented here. A coworker should package `measurement.resistance_ohms` only
after this module returns `SOOT_RESISTANCE_STATUS_OK`.
