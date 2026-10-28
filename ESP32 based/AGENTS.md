# ESP32 package maintenance

- This folder is an independent Arduino-ESP32 acquisition package. It owns no
  radio, gateway, or cloud behavior. Do not edit the sibling TI package as part
  of an ESP32-only change.
- Update VERSION, library.properties, the header version macros, README version,
  and CHANGELOG together for each ESP32 release. Start this sequence at 0.1.0;
  do not synchronize its version number with the TI package or create nested Git.
- Target the standard ESP32 DevKit/WROOM; example SDA GPIO 21, SCL GPIO 22,
  100 kHz, 3.3 V logic. The application owns and initializes Wire only once.
- Use the external ADS1115, MCP4728, MCP6002, and ADG728. Keep seven-bit I2C
  addresses unshifted and preserve A0-minus-A1 polarity.
- Initialize and start every read at S4 (100 kohm). Descend S3, S2, S1 only on
  saturation, using exactly one channel, readback verification, and settling.
- Keep editable resistor values at the top of src/soot_resistance.cpp. Compute
  sensor resistance with selected effective feedback times bias divided by delta.
- Return faults without a valid numeric resistance. Examples must print NA for
  failed current/resistance results. Do not write MCP4728 EEPROM during operation.
- Calls are blocking; use loop or task context, never interrupts. Serialize entire
  driver operations if multiple tasks share these three I2C devices.
- Run host tests and compile the example for esp32:esp32:esp32 after driver changes.
  Do not claim hardware validation without the physical ESP32 analog circuit.
