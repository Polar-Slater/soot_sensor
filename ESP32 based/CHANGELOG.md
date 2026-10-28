# ESP32 changelog

This package has its own version sequence, independent of `TI based`.

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
