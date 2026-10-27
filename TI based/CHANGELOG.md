# Changelog

All notable changes to this handoff package are recorded here.

## 0.2.1 - 2026-10-27

- Initialize feedback at S4 (100 kohm) and start every read at the highest
  resistance. On saturation, descend through S3 (10 kohm), S2 (1 kohm), and
  S1 (100 ohm), settling and acquiring fresh data at each step.
- Keep the first nonsaturated valid range; return low-signal/open and other
  faults without reducing gain. Report saturation if S1 is also saturated.
- Remove the previous 0.1 V upward-ranging target and single fallback.
- Give invalid bias priority over saturation so it stops the range scan.
- Update range-order, selection, and fault regression tests and documentation.
- Host regression tests and integration-example compilation pass with Zig 0.13.0
  C99 and strict warnings; `git diff --check` passes.
- Physical settling/recovery checks and final CCS validation remain required.

## 0.2.0 - 2026-10-27

- Replace fixed 1 Mohm feedback with ADG728 S1..S4 selection at `0x4C`:
  nominal 100 ohm, 1 kohm, 10 kohm, and 100 kohm.
- Put resistor calibration values and optional switch corrections at the start
  of the driver; defaults use only the labeled resistor values.
- Automatically scan from lowest gain, targeting at least 0.1 V delta, with
  settling, one-channel writes, readback verification, and bounded fallback.
- Include selected physical channel and effective feedback resistance in results.
- Reject unknown switch state and clear stale resistance on failed reads.
- API change: config.feedback_resistance_ohms is now a four-element array.
- Update wiring/calibration instructions and integration example. Legacy layout
  drawings still show the old fixed resistor and are not assembly instructions
  for this revision.
- Actual CC2650/CCS build and physical bench validation remain required.
- Host regression tests and example object compilation passed with Zig 0.13.0
  C99, `-Wall -Wextra -Werror -pedantic`, using TI-RTOS/I2C stubs.

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
