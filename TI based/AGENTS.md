# Maintainer instructions

## Project scope

This folder owns only the CC2650-side resistance acquisition path:

- shared TI-RTOS I2C access;
- MCP4728 channel-A bias setup;
- ADG728 automatic feedback-resistor selection;
- ADS1115 A1 and A0-A1 acquisition;
- current and resistance calculation;
- measurement fault reporting.

Do not add BLE, TSCH, 6LoWPAN, RPL, CoAP, TLV encoding, Node.js, or gateway code
to this module. The testbed team integrates the returned resistance into its
existing wireless application.

## Hardware invariants

- CC2650STK I2C: SDA DIO_5, SCL DIO_6.
- ADS1115 address: `0x48` when ADDR is grounded.
- MCP4728 address: `0x60`, or `0x64` for some A4 boards.
- MCP4728 VA drives MCP6002 pin 3.
- MCP6002 pins 1, 2, and 5 form the buffered bias net.
- ADS1115 A1 measures the buffered bias at MCP6002 pin 1.
- ADS1115 A0 measures the TIA output at MCP6002 pin 7.
- Feedback uses ADG728 D at pin 6; S1..S4 connect through 100 ohm, 1 kohm,
  10 kohm, and 100 kohm resistors respectively to pin 7. S5..S8 are unused.
- ADG728 default seven-bit address is `0x4C`; enable exactly one channel.
- Retain the 0.1 uF feedback capacitor directly from pin 7 to pin 6.
- Calibrated resistor and optional series-switch values live at the start of
  src/soot_resistance.c; use the selected effective resistance in calculations.
- Clamp 1 connects to pin 6; clamp 2 connects to ground.
- All digital and analog components share the 3.3 V/GND delivered through the
  Grove/STEMMA QT I2C chain.

Never join ADS1115 ADDR to VIN. ADDR is on ground; VIN is on 3.3 V.

## Software invariants

- Keep seven-bit I2C addresses unshifted.
- Use the existing application-owned I2C handle. Do not open the same CC2650
  I2C instance a second time.
- Call `I2C_transfer()` only from TI-RTOS Task context.
- Preserve differential polarity as A0 minus A1.
- Preserve the resistance equation:
  `R_sensor = R_feedback * V_bias / (V_A0 - V_A1)`.
- Do not return a numeric resistance when bias, ADC range, or minimum-delta
  validation fails.
- Do not write MCP4728 EEPROM on every boot or sample. The current volatile
  multi-write avoids EEPROM endurance concerns.

## Compatibility

The receiving firmware is a legacy CC2650 TI-RTOS/6TiSCH project. Prefer the
generic APIs in `<ti/drivers/I2C.h>` and avoid dependencies on newer SimpleLink
SDK-only APIs. Match the exact CCS, compiler, TI-RTOS, and XDCtools versions
declared by the parent project.

## Change discipline

- Update `VERSION` and `CHANGELOG.md` for every handoff release.
- Keep comments focused on circuit purpose and units.
- Record changed hardware constants in both the defaults and README.
- Validate with a known resistor and report raw bias/delta voltages before any
  wireless integration.
- Do not claim final hardware validation without testing on the actual
  SensorTag, Debug DevPack, MCP4728, ADS1115, and MCP6002 circuit.
