# Bench test: HUB75 panel on an ESP32-S3 (#36)

This test runs the 64×32 P5 panel and the TF-Luna together. The panel shows the time between your last two swipes through the beam, the same way it will show a split. The serial monitor reports whether driving the panel slows down reading the sensor.

## Power

The panel needs its own 5 V supply. Never power it from the ESP32's 5V pin. At full white it can draw several amps.

- Connect the panel's red/black power lead to a 5 V supply: the 5 V 4 A barrel supply from the BOM, or for this test only, a USB power bank or phone charger through a USB-to-bare-wire cable. The test firmware starts at a low brightness (40 of 255) and only lights a few pixels, so a 2 A USB supply is enough.
- Join the panel supply's ground to an ESP32 GND pin. The data lines need a shared ground.

## Wiring

The ribbon cable plugs into the panel's **input** connector (the one with the arrow pointing into the panel, usually marked `IN` or `JIN`). Male-to-female jumper wires push into the other end of the ribbon. The ribbon socket, looking into it with the key notch on the left:

```
        ┌───────────┐
  R1  1 │ ●       ● │ 2  G1
  B1  3 │ ●       ● │ 4  GND
  R2  5 │ ●       ● │ 6  G2
  B2  7 │ ●       ● │ 8  E (unused, leave loose)
  A   9 │ ●       ● │ 10 B
  C  11 │ ●       ● │ 12 D
  CLK 13│ ●       ● │ 14 LAT
  OE 15 │ ●       ● │ 16 GND
        └───────────┘
```

Some panels print the labels on the board next to the connector. If yours disagree with this drawing, follow the panel's labels.

| Panel | ESP32-S3 |
|-------|----------|
| R1 | GPIO4 |
| G1 | GPIO5 |
| B1 | GPIO6 |
| R2 | GPIO7 |
| G2 | GPIO15 |
| B2 | GPIO16 |
| A | GPIO1 |
| B | GPIO2 |
| C | GPIO42 |
| D | GPIO41 |
| CLK | GPIO40 |
| LAT | GPIO39 |
| OE | GPIO38 |
| GND (either) | GND |

Keep the TF-Luna on GPIO17/18 as before. GPIO9–14 and GPIO21 are left free for the nRF24 radio (#22).

## Running it

From `firmware`:

```
py -m platformio run -e esp32s3_display -t upload -t monitor
```

1. Keep the beam clear for the first second. The panel shows `learning`, then the live distance at the top and `-.--` in large digits.
2. Swipe a hand through the beam, wait a few seconds, and swipe again. The large digits show the time between the two swipes, for example `3.71`. They turn orange while something is in the beam.
3. Type `+` or `-` and press Enter to change the brightness, and `b` to relearn the baseline.

## What to check

- Every second the monitor prints `rate=` and `max_gap=`. With the panel running, `rate` should stay around 243 Hz and `max_gap` near 4 ms, the same as without the panel. If `max_gap` jumps every time the panel redraws, the display is delaying the sensor.
- Swipe pairs timed with a phone stopwatch should roughly match the panel (within your reaction time).

## If the panel looks wrong

- **Nothing lights:** check the panel's 5 V supply, the shared ground, and that the ribbon is in the input connector, not the output.
- **Wrong colours:** two of R/G/B are swapped, so check those wires.
- **Rows doubled, shifted or in the wrong place:** check the A, B, C and D wires.
- **Ghosting or garbage pixels:** shorten the jumper wires if you can. Some panels also need a different clock setting in the firmware. Send a photo and I'll adjust it.
