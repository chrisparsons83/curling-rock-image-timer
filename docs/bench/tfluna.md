# Bench test: TF-Luna on an ESP32-S3 (#14)

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
- `rate` should sit at 250. `max_gap` should stay near 4 ms; much larger values mean frames are being dropped or delayed.
- `unreliable` counts readings with signal strength below 100 or saturated, where the sensor itself says the distance can't be trusted.
- Type `c` and press Enter to clear the counters, and `r` to switch to raw CSV of every frame (`t_us,distance_cm,amplitude,temp_c`). Raw output can be saved with `pio device monitor > run.csv` for analysis.

## What to check

1. **10-minute run (the #14 pass condition):** point the sensor at a wall 1–5 m away, type `c`, wait 10 minutes, and confirm `rate` stayed at 250, `total` is about 150,000, and `checksum_errors` is 0.
2. **Readings make sense:** hold a hand or a box in the beam at a few distances and check `dist` against a tape measure.
3. **Delay between measuring and arriving:** the datasheet doesn't give one. Each frame is timestamped as soon as its last byte arrives, so any delay inside the sensor is the same on every frame. A constant delay cancels out of the splits, because both ends of a split carry it. The on-ice test (#18) measures what's left against 240 fps video.

Post the summary lines from the 10-minute run on #14.
