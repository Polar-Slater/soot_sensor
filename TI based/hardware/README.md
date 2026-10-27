# Hardware revision notice

The SVG/PNG drawings and perfboard soldering plan in this directory document
the previous fixed 1 Mohm feedback circuit. They predate the ADG728 resistor
bank introduced in version 0.2.0, including files named `sensor-wiring-latest`.

Use the [current module wiring table](../README.md#hardware-assumed-by-the-code)
for the ADG728 revision: remove the old feedback resistor, connect D to MCP6002
pin 6, and connect S1/S2/S3/S4 through 100 ohm/1 kohm/10 kohm/100 kohm respectively
to pin 7. Keep the 0.1 uF feedback capacitor directly between pins 7 and 6.
S5..S8 remain unconnected. This wiring still requires physical validation.
