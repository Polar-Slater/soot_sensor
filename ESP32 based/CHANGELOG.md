# ESP32 changelog

This package has its own version sequence, independent of `TI based`.

## 0.2.1 - 2026-11-06

- Switch to the next smaller feedback resistor at output >= 3.0 V with the
  default 3.3 V supply (supply minus 0.30 V), allowing more output headroom.
- Add boundary and simulated 3.155 V clipping regression coverage, including
  descent from S2 to S1. Host tests and the ESP32 Dev Module build pass.
  Physical verification of the plateau fix is pending.

## 0.2.0 - 2026-11-02

- Add the nominal 1 Mohm feedback resistor on ADG728 S5 (mask 0x10), with
  an editable resistor value and optional switch correction. Start at S5 and
  descend through S4 to S1 on saturation. Allow at least 1000 ms for S5
  settling after selection or bias changes.
- Five-range host tests and the ESP32 Dev Module build passed.
- Default the Arduino example to labeled Serial Monitor readings at 115200 baud,
  including resistance in ohms, current in microamps, status, and diagnostics.
  Keep the original CSV format available with `SOOT_CSV_OUTPUT = true`.
  Document Arduino IDE monitor setup and fault behavior.
- Clarify the STEMMA QT chain for shared power/I2C: one ESP32 connection,
  with only the remaining analog and configuration wiring listed separately.

## 0.1.0 - 2026-10-28

- Initial Arduino-ESP32 port of the acquisition behavior in TI version 0.2.1.
- Support the ESP32 DevKit/WROOM using GPIO 21 SDA and GPIO 22 SCL, with editable
  pins and a shared, application-owned TwoWire bus.
- Keep external MCP4728 bias, ADS1115 acquisition, MCP6002 TIA, and ADG728 bank.
- Initialize and start each read at 100 kohms, descending through 10 kohms,
  1 kohm, and 100 ohms on saturation only.
- Keep nominal resistor calibration values at the beginning of the driver,
  optional switch-resistance corrections, 100 ms settling, and 16-sample averages.
- Add checked Wire transfers, repeated START for ADC register reads, short-read
  detection, a serial CSV example with fault handling, and host regression tests.
- The driver and tests are independent copies; building requires no TI files.
- Host tests passed with Zig 0.13.0 C++11. Full ESP32 Dev Module compilation
  passed with Arduino CLI 1.5.1 / Arduino-ESP32 3.3.12 on 2026-10-28.
- Physical bench validation remains required before using measurements.
