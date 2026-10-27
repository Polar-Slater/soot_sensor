# Perfboard soldering plan

New-build plan: ADS1115 rotated 180° relative to the initial layout. MCP6002 pin 4 goes to MCP4728 GND; pin 8 is fed from ADS1115 VIN via C2. Supply is 3.3 V from the QT chain.

## Assembly

1. Mark A1 on the component side. Match the illustrated numbering before soldering; physical board labels may face a different direction.
2. Dry-fit the sockets and modules. DAC headers: B4–B9 / G4–G9; ADC: B16–B21 / G16–G21. Check QT plug access.
3. Install the DIP-8 socket at K11–N11 / K14–N14, notch toward A. Pin 1 = K11; pin 4 = N11; pin 8 = K14; pin 5 = N14.
4. Install the resistor socket at O15–O22 (end contacts assumed), C1 at M16–M17, C2 at J12–J14, and sensor contacts P24/Q24. Check actual resistor body and lead spacing.
5. Flip the board left-to-right while keeping edge A at the top. The solder view reverses columns, not rows.
6. Fit insulated jumpers in numerical order. Solder only endpoints; leave insulation intact where wires cross or pass other pads. Crossing wires never join unless the table shares an endpoint.
7. Inspect and continuity-check the named nets before fitting IC/modules. Verify no hard short from supply to ground and verify 3.3 V at the empty socket pin 8 relative to pin 4. Disconnect power before inserting modules.

## Jumpers

| ID | From | To | Connection |
|---|---|---|---|
| 01 | G16 | J14 | ADS VIN → C2 supply |
| 02 | J14 | K14 | C2 supply → U1 pin 8 |
| 03 | G9 | N11 | DAC GND → U1 pin 4 |
| 04 | N11 | J12 | U1 pin 4 → C2 ground |
| 05 | G20 | G17 | ADS ADDR → ADS GND |
| 06 | N11 | Q24 | U1 pin 4 → sensor return |
| 07 | G8 | M11 | DAC VA → U1 pin 3 |
| 08 | K11 | L11 | U1 pin 1 → pin 2 |
| 09 | K11 | N14 | U1 pin 1 → pin 5 |
| 10 | K11 | B19 | U1 pin 1 → ADS A1 |
| 11 | L14 | B20 | U1 pin 7 → ADS A0 |
| 12 | L14 | M17 | U1 pin 7 → C1 output |
| 13 | M17 | O22 | C1 output → R1 output |
| 14 | M14 | M16 | U1 pin 6 → C1 sense |
| 15 | M14 | O15 | U1 pin 6 → R1 sense |
| 16 | M14 | P24 | U1 pin 6 → sensor sense |

## Parts and external connections

- C1: 100 nF per V1, between pin-6 sense and pin-7 output. C2: 100 nF between supply and ground, near the DIP socket.
- R1: 1 MΩ between O15 (sense) and O22 (output). The sensor is between P24 (sense) and Q24 (ground), not parallel with R1.
- CC2650 interface cable exits left into MCP4728 QT. The other QT socket links to ADS1115. Confirm the CC2650 interface wiring; no controller pinout is assumed.
- QT provides the common ground between modules. W05 grounds ADDR locally at ADS1115, selecting 0x48.
- Drawings assume the previously specified 24 × 18 grid at 2.54 mm pitch. Board offsets, socket bodies and cable clearance are not fabrication-verified. Do not print this as a drilling template.

## Validation

The generator checks that all 16 jumpers connect the intended six separate electrical nets, including QT ground continuity. It does not electrically test the physical assembly.
