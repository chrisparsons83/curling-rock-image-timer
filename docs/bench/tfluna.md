# Bench test: TF-Luna on an ESP32-S3 (#14, #16)

## Wiring

The TF-Luna's connector pins are numbered 1 to 6 on the sensor's label and in its datasheet. Logic is 3.3 V, so it connects straight to the ESP32 with no level shifter.

| TF-Luna pin | Signal | ESP32-S3 | ESP32-C3 Super Mini |
|---|---|---|---|
| 1 | +5 V | 5V (or VBUS) | 5V |
| 2 | RXD (into sensor) | GPIO17 | GPIO21 |
| 3 | TXD (out of sensor) | GPIO18 | GPIO20 |
| 4 | GND | GND | GND |
| 5 | Config | leave unconnected (UART mode) | leave unconnected |
| 6 | Multiplex | leave unconnected | leave unconnected |

Grounding pin 5 switches the sensor to I2C, which this firmware doesn't use.

## Running it

From `firmware/`:

- S3 board with two USB ports: plug into the one marked UART or COM and run `pio run -e esp32s3 -t upload -t monitor`.
- S3 board with one USB port: run `pio run -e esp32s3_usb -t upload -t monitor`. If the upload can't find the board, hold BOOT, tap RESET, release BOOT and try again.

Each second it prints a line like:

```
# bytes=2250  rate=250.0 Hz  dist=452 cm  amp=1180  temp=29.0 C  max_gap=4.3 ms  unreliable=0  total=2500  checksum_errors=0  since_clear=10 s
```

- `bytes` is how many bytes arrived from the sensor in the last second, about 2,250 at 250 Hz. If it's 0, nothing is arriving: swap the GPIO17 and GPIO18 wires, then check the sensor's power and that pin 5 isn't grounded. If bytes arrive but `rate` stays at 0, the baud rate or wiring is garbling them.
- `rate` should be steady, at or a little under 250. The TF-Luna's own clock can run a few percent slow, and one tested sensor sent 242 frames a second. `bytes` should be exactly 9 × `rate`, which shows no frames are lost.
- `max_gap` should stay near one frame period (about 4 ms). Much larger values mean frames are being dropped or timestamped late.
- `unreliable` counts readings with signal strength below 100 or saturated, where the sensor itself says the distance can't be trusted.
- Type `c` and press Enter to clear the counters, `b` to relearn the empty-line baseline, `x` to toggle a hex dump of every received byte, `t` to send a test command, and `r` to switch to raw CSV of every frame (`t_us,distance_cm,amplitude,temp_c`). Raw output can be saved with `pio device monitor > run.csv` for analysis.

## Crossings (#16)

For the first second after boot (or after `b`) the firmware learns the empty-line distance, so keep the beam clear. It then prints:

```
# baseline=147 cm  threshold=132 cm
```

`baseline` is what the sensor sees with nothing in the way: the far wall, or the ceiling on a bench. Anything nearer than `threshold` (15 cm short of the baseline, and never beyond 450 cm, the sheet width) counts as something on the line. Each object that passes through the beam then prints two lines:

```
# crossing 3 start t=41.218 s
# crossing 3 end t=41.402 s  duration=184.0 ms  samples=45  min=98 cm  median=101 cm
```

The start time is the first reading that saw the object, which is the time a split will use. A single stray reading doesn't count, and a gap of one or two readings inside an object doesn't split it in two. Something left in the beam for 5 seconds ends with `timed out, baseline relearned`, and the firmware treats it as the new empty line.

To test it on the bench, sweep a hand or a box through the beam at a few distances nearer than `threshold` and check that each pass prints exactly one crossing. An object at the baseline distance itself can't be told apart from the empty line. Crossing lines start with `#`, so they don't break raw CSV captures.

## If bytes= stays at 0

Two tests, typing the key into the serial monitor and pressing Enter:

1. **Board check (loopback):** unplug the LiDAR wires from 17 and 18 and put one jumper wire straight from GPIO17 to GPIO18. Press `t`. The next line should show `bytes=4`. If it does, the board's UART and pins work, and the fault is on the sensor side. If it stays at 0, the pins or the firmware's pin choice are wrong for this board.
2. **Sensor check:** reconnect the LiDAR and press `t`. A sensor that's powered and wired correctly answers with 7 bytes even if its continuous output was switched off. Press `x` to see the raw bytes in hex; a reply starts `5A 07 01`, and normal data frames start `59 59`.

If the board check passes but the sensor never answers, measure about 5 V between pin 1 (red) and pin 4 (black) at the sensor's connector, and check that the connector is fully clicked into the sensor.

## What to check

1. **10-minute run (the #14 pass condition):** point the sensor at a wall 1–5 m away, type `c`, wait 10 minutes, and confirm `rate` stayed steady, `total` is about 600 × rate (about 145,000–150,000), `checksum_errors` is 0, and `max_gap` stayed under about 6 ms.
2. **Readings make sense:** hold a hand or a box in the beam at a few distances and check `dist` against a tape measure.
3. **Delay between measuring and arriving:** the datasheet doesn't give one. Each frame is timestamped as soon as its last byte arrives, so any delay inside the sensor is the same on every frame. A constant delay cancels out of the splits, because both ends of a split carry it. The on-ice test (#18) measures what's left against 240 fps video.

Post the summary lines from the 10-minute run on #14.
