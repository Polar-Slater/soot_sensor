# Changelog

All notable changes to this handoff package are recorded here.

## 0.1.0 - 2026-09-23

### Added

- Initial CC2650/TI-RTOS C driver for the MCP4728 and ADS1115.
- MCP4728 channel-A configuration for a nominal 1.000 V bias using the internal
  2.048 V reference and gain 1.
- ADS1115 single-shot reads of A1-to-ground and differential A0-minus-A1.
- Sixteen-sample averaging matching the existing Arduino prototype.
- Resistance and sensor-current calculations using the measured 1,000,411 ohm
  feedback resistor.
- Error states for I2C failures, conversion timeout, saturation, invalid bias,
  and open/out-of-range clamps.
- Example integration functions for an existing TI-RTOS sensor task.
- README and AGENTS handoff documentation.

### Excluded intentionally

- Wireless initialization, radio protocols, packet encoding, and gateway code.

### Validation status

- Algorithm and register construction reviewed against the existing Arduino
  sketch and component datasheets.
- Final compilation in the coworkers' CCS project and physical hardware tests
  remain required because that complete project/toolchain is not in this
  workspace.
