# ESP32 soot-sensor resistance module

Version: **0.1.0**

Standalone Arduino-ESP32 library and example for a standard **ESP32 DevKit /
WROOM**. It replaces the CC2650 controller while retaining the MCP4728 DAC,
ADS1115 ADC, MCP6002 op-amp circuit, and Adafruit ADG728 feedback resistor bank.
The ESP32 internal ADC and DAC are not used.

This package starts its own version sequence at 0.1.0, based on the acquisition
behavior of TI version 0.2.1. Its code, examples, tests, VERSION, and CHANGELOG
are independent; no files from `TI based` are needed to build it. Both folders
remain in the existing Git repository. For future ESP32 releases, update VERSION,
library.properties, the version macros in src/soot_resistance.h, this README,
and CHANGELOG together. ESP32 release tags can use `esp32-vX.Y.Z` when releases
are published; no separate or nested Git repository is needed.

## Quick start

1. Install the **esp32 by Espressif Systems** board package in Arduino IDE.
   This package targets Arduino-ESP32 **3.3.12** and **ESP32 Dev Module**
   (`esp32:esp32:esp32`). Use the official board-manager index:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. Copy this whole folder into your Arduino sketchbook's `libraries` directory
   under the name `SootResistanceESP32`, keeping `library.properties` and `src`
   immediately inside it. Restart the IDE if the library does not appear.
3. Open `examples/ResistanceMonitor/ResistanceMonitor.ino`, select **ESP32 Dev
   Module** and your board's serial port, then compile and upload.
4. Open Serial Monitor at **115200 baud**. The example prints CSV diagnostics
   including status, selected channel, feedback resistance, raw ADC codes,
   voltages, current, and sensor resistance. Failed current/resistance values
   print as `NA`.

No third-party sensor libraries are required; only Arduino-ESP32's built-in
Arduino and Wire APIs are used. Pins and reporting pause are editable at the
top of the sketch. Resistor calibration is at the top of the driver.

From this folder, Arduino CLI can compile without installing the library into
your sketchbook:

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn esp32:esp32:esp32 --library "." "examples/ResistanceMonitor"
```

## ESP32 wiring

Power the external circuit from **3.3 V**, with a common ground. Use the
DevKit's regulated 3V3 pin, not its 5V/VIN pin, for this circuit's supply.
Connect through breakout power pins with a verified cable pinout; STEMMA QT
connector order is not the same as an arbitrary DevKit header order.

| ESP32 DevKit/WROOM | External circuit |
| --- | --- |
| GPIO 21 (SDA) | MCP4728 SDA, ADS1115 SDA, ADG728 SDA |
| GPIO 22 (SCL) | MCP4728 SCL, ADS1115 SCL, ADG728 SCL |
| 3V3 | MCP4728 VCC/VIN, ADS1115 VIN, ADG728 VIN, MCP6002 pin 8 |
| GND | All breakout grounds, ADS1115 ADDR, MCP6002 pin 4, sensor clamp 2 |

All three I2C devices share the same two wires at **100 kHz**. Breakout pull-ups
must go to 3.3 V. GPIO 21/22 are the default for this classic ESP32 example;
verify pin assignments before using a different ESP32 variant.

| Address | Device |
| --- | --- |
| `0x60` | MCP4728; use `0x64` for an MCP4728A4 board if applicable |
| `0x48` | ADS1115 with ADDR grounded |
| `0x4C` | ADG728 with default address jumpers; options `0x4C`..`0x4F` |

## Analog connections

| Connection | Purpose |
| --- | --- |
| MCP4728 VA to MCP6002 pin 3 | Nominal 1.000 V bias source |
| MCP6002 pin 1 to pins 2 and 5 | Buffered bias and TIA reference |
| ADS1115 A1 to MCP6002 pin 1 | Buffered bias measurement |
| ADS1115 A0 to MCP6002 pin 7 | TIA output measurement |
| ADG728 D to MCP6002 pin 6 | Common feedback node |
| ADG728 S1 through 100 ohms to MCP6002 pin 7 | Lowest feedback resistance |
| ADG728 S2 through 1 kohm to MCP6002 pin 7 | Feedback range 2 |
| ADG728 S3 through 10 kohms to MCP6002 pin 7 | Feedback range 3 |
| ADG728 S4 through 100 kohms to MCP6002 pin 7 | Highest feedback resistance |
| ADG728 S5..S8 unconnected | Unused channels |
| ADG728 RST held high, tied to 3.3 V if unused | Prevent unintended switch reset |
| 0.1 uF directly between MCP6002 pins 7 and 6 | Feedback capacitor |
| Sensor clamp 1 to MCP6002 pin 6 | Sensor input |
| Sensor clamp 2 to GND | Sensor return |
| 0.1 uF between MCP6002 pins 8 and 4 | Local supply decoupling |

Remove the old 1 Mohm feedback resistor. Each branch uses one resistor; the four
ends at pin 7 join together. Keep the feedback capacitor directly across pins
7 and 6, outside the switch. ADS1115 ADDR must stay at GND, never VIN.
The old TI wiring pictures show the previous fixed-resistor layout and CC2650
connections; use these tables for the ESP32 switched-resistor circuit.

## Calibration

Edit the first lines of `src/soot_resistance.cpp` when measured values are ready:

```cpp
#define SOOT_FEEDBACK_S1_OHMS 100.0f
#define SOOT_FEEDBACK_S2_OHMS 1000.0f
#define SOOT_FEEDBACK_S3_OHMS 10000.0f
#define SOOT_FEEDBACK_S4_OHMS 100000.0f
```

Optional `SOOT_SWITCH_S1_OHMS` through `SOOT_SWITCH_S4_OHMS` add measured switch
series-resistance corrections. They default to zero, so initial calculations
use only labeled resistor values. Zero does not represent actual switch
resistance: this is especially significant on the 100 ohm branch. If you enter
a measured effective resistor-plus-switch value, keep its separate correction
at zero to avoid counting it twice. The switch correction is an approximation
over operating voltage and temperature, not a guarantee of precision.

Defaults can also be overridden per board in `config.feedback_resistance_ohms`
and `config.switch_resistance_ohms` before initialization. The four effective
values must be finite, positive, and strictly increasing in S1..S4 order.

## Selection criteria and calculation

Initialization and **every measurement** start at **S4, 100 kohms**. On saturation,
selection descends **S3, 10 kohms -> S2, 1 kohm -> S1, 100 ohms**, stopping at the
first valid nonsaturated result. The resistor values remain listed in ascending
channel order in the configuration; do not reverse that array.

- Select exactly one channel, verify readback, and wait **100 ms** for settling.
- Average **16 A1-to-GND** and **16 A0-minus-A1** conversions at 128 SPS.
- Step down if an ADC code is >= 32700 or <= -32700, or the estimated output
  reaches **supply minus 0.05 V** (3.25 V with the default 3.3 V supply).
- Bias outside **0.8..1.2 V** returns a bias fault and stops the scan. This takes
  priority over saturation.
- Delta **<= 0.0005 V** returns `OPEN_OR_OUT_OF_RANGE`. Lower gain cannot improve
  that weak signal, so there is no further switching.
- Saturation at S1 returns `ADC_SATURATED`. Bus, switch-state, and conversion
  failures also stop the read. At most four ranges are evaluated.

The differential polarity and equations are:

```text
Vbias      = ADS1115 A1
delta      = ADS1115 (A0 - A1)
Vout       = Vbias + delta
Rfeedback  = selected resistor + optional switch correction
Isensor    = delta / Rfeedback
Rsensor    = Rfeedback * Vbias / delta
```

Consume resistance/current **only on OK**. Failed reads clear resistance;
available diagnostic fields are for investigation, and may be zero after an
early bus error. Results identify the physical channel (1..4) and effective
feedback used. The 1 V bias is across the sensor while the op-amp operates
linearly; the feedback resistors set gain, not a separately commanded current.

## Integration and timing

```cpp
Wire.begin(21, 22, 100000);
Wire.setTimeOut(50);
SootResistance_Config config;
SootResistance sensor = {};
SootResistance_configDefaults(&config, &Wire);
SootResistance_Status status = SootResistance_init(&sensor, &config);
// Check status before calling SootResistance_read(&sensor, &measurement).
```

The caller owns the TwoWire object, must keep it alive, and starts it once.
The driver never calls Wire.begin/end, changes pins, or changes the bus timeout.
Use the existing Wire instance when integrating into another application.
Serialize entire init/read/setBias calls against tasks that configure these
devices; transaction-level Wire locking alone does not protect a range scan.
Use setup/loop or task context, never interrupts.

Calls block with Arduino delay between polls and selections. A four-range read
takes roughly 1.5 s plus bus/scheduling overhead; the example adds a 1 s pause
after each report. The example retries failed initialization every 5 s and
reinitializes after uncertain switch state. There is no Wi-Fi/BLE code.
Configuration is immutable after init; reinitialize to apply changes.

The DAC uses volatile channel-A writes (internal 2.048 V reference, gain 1,
code 2000 for 1 V). No EEPROM writes are issued.

## Validation

Validated on 2026-10-28:

- Host regression tests and example compilation passed with Zig 0.13.0 C++11,
  using `-Wall -Wextra -Werror -pedantic` for the project sources.
- Full example compilation passed with Arduino CLI 1.5.1 and Arduino-ESP32
  3.3.12 for `esp32:esp32:esp32`, with all compiler warnings enabled.
  Reported usage: 296,388 bytes flash (22%) and 23,644 bytes global RAM (7%).
- Version metadata and whitespace checks passed. The sibling TI package is
  unchanged. No firmware was uploaded and physical hardware has not been tested.

Run the independent host suite with a C++ compiler and Python:

```text
python tests/run_host_tests.py --compiler g++
```

Alternatively use `--compiler zig c++` or clang++. Tests compile this driver
against simulated Arduino/Wire APIs and cover descending order, calibrated
calculations, saturation boundaries, invalid bias, open sensor, timeouts,
switch reset, repeated-start register reads, and short/failed Wire transfers.
The example is also compiled against the stubs. Target compilation uses the
Arduino CLI command above. Neither type of test validates analog accuracy.

For physical bring-up:

1. Confirm 3.3 V/GND, I2C addresses, and the resistor-bank wiring.
2. Measure the buffered bias near 1 V. With open clamps, expect an open/range
   fault rather than a numeric resistance.
3. Test known sensor resistors of 1 kohm, 10 kohms, 100 kohms, and 1 Mohm.
   Expect channels S2, S3, S4, S4 and nominal deltas 1 V, 1 V, 1 V, 0.1 V.
4. Log raw codes, voltages, selected feedback, current, and resistance; compare
   with the known resistor and calibrate the switch path as needed.
5. Scope actual sensor bias (MCP6002 pin 6) and output (pin 7) during startup and
   range changes. Starting at 100 kohms can initially saturate low-resistance
   loads. The 100 ms delay and 3.25 V software cutoff require bench validation.
6. Validate low-resistance current drive and sensor heating before relying on
   the 100 ohm branch. At 1 V, a 100 ohm sensor requires 10 mA; feedback selection
   does not itself guarantee that the op-amp can maintain that bias under load.

References: [Espressif Arduino I2C API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html),
[Adafruit ADG728 pinouts](https://learn.adafruit.com/adafruit-adg728-1-to-8-analog-matrix-switch/pinouts),
[ADG728 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/adg728_729.pdf).
