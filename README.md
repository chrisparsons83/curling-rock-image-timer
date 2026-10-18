# Curling rock timer

Split timer for one curling sheet. Four units sit against the side board on each back line and hog line. Each one uses a TF-Luna LiDAR aimed along its line to timestamp the moment a rock crosses. The units talk over nRF24L01+ radios, and an LED panel at each hog line shows the back-to-hog and hog-to-hog splits to 0.01 s, in either direction.

## Layout

- `firmware/`: PlatformIO project for the ESP32 units, plus host unit tests
  - `firmware/lib/`: hardware-independent code (formatting, protocol, timing logic) that also compiles and tests on a laptop
- `tools/`: desk tools such as the data logger, analysis and replay
- `hardware/`: bill of materials, wiring and enclosure files
- `docs/`: design decisions and setup notes

## Building

Install [PlatformIO](https://platformio.org/install/cli), then from `firmware/`:

```sh
pio test -e native     # host unit tests
pio run -e esp32s3     # hog-line units (ESP32-S3)
pio run -e esp32c3     # back-line unit (ESP32-C3 Super Mini)
pio run -e esp32s3 -t upload -t monitor   # flash a board and watch its serial output
```

Planning lives in the GitHub issues, grouped under the milestone issues #1–#8.
