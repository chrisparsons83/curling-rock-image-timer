# Design decisions

## Firmware framework: Arduino core on PlatformIO

**Decision:** write the firmware with the Arduino core for ESP32, built with PlatformIO. Drop to ESP-IDF APIs inside the Arduino build wherever timing needs it. Decided 2026-10-18 (#13).

**Why:**

- **Libraries.** The two libraries the design leans on are Arduino libraries. [RF24](https://github.com/nRF24/RF24) is the mature nRF24L01+ driver. [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) drives the LED panels from the S3 using DMA. Using them under pure ESP-IDF means porting or wrapping them.
- **Timing is not a reason to leave Arduino.** The Arduino core runs on top of ESP-IDF, so its APIs are all available from the same build: `esp_timer_get_time()` gives a 64-bit µs clock, and IRAM GPIO interrupt handlers can timestamp the nRF24 IRQ pin and the LiDAR samples. The TF-Luna arrives over UART at 250 Hz (every 4 ms), so µs-level interrupt latency is far below what matters.
- **Iteration speed.** Arduino plus PlatformIO gives one-command build, flash and serial monitor for both chips, and a `native` environment for host unit tests in CI.
- **Testability.** Timing logic, the radio message format and display formatting live in `firmware/lib/` with no Arduino includes, so they're covered by `pio test -e native` on every push.

**Revisit if:** the Arduino core's background tasks cause measurable timestamp jitter in the sync tests (#25), or a needed ESP-IDF feature can't be reached from the Arduino build.
