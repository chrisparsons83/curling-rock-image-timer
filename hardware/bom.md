# Bill of materials

Prices are US retail, checked 2026-10-17. Marketplace listings (Amazon, AliExpress) are often cheaper; the links are a reference point, not a required supplier.

Unit names: **A-back, A-hog, B-hog, B-back**. The hog-line units drive the LED panels, so they need ESP32-S3 boards.

## Already owned

| Part | Qty | Used for |
|---|---|---|
| ESP32-S3 dev board | 3 | A-hog, B-hog, and one back-line unit |
| MakerFocus TF-Luna LiDAR (0.2–8 m) | 1 | First unit for bench and on-ice testing |
| HUB75 64×32 P5 LED panel, 1/16 scan (on order) | 1 | First hog-line display |
| nRF24L01+ radio | 4 | All units |
| 3.3 V regulator | 1 | First unit |
| Capacitors (10–100 µF for the radios) | assorted | All units |
| Perfboard | assorted | All units |

## To buy

| Part | Qty | Link | Unit price | Total |
|---|---|---|---|---|
| Benewake TF-Luna LiDAR | 3 | [GetFPV](https://www.getfpv.com/commercial-industry-drones/cinematography-drones/benewake-tf-luna-lidar.html) | $27.99 | $83.97 |
| ESP32-C3 Super Mini (second back-line unit) | 1 | [espboards.dev price list](https://www.espboards.dev/blog/esp32-super-mini-comparison/) | ~$3 | ~$3 |
| HUB75 64×32 P5 LED panel (second hog line) | 1 | [RobotShop (Waveshare)](https://www.robotshop.com/products/waveshare-rgb-full-color-led-matrix-panel-5mm-pitch-6432-pixels-adjustable-brightness) | $27.15 | $27.15 |
| 5 V 4 A power supply, 5.5×2.1 mm barrel (one per panel) | 2 | [SparkFun](https://www.sparkfun.com/power-supply-5v-4a.html) | $19.50 | $39.00 |
| 3.3 V regulator, e.g. AMS1117-3.3 module (other units) | 3 | marketplace multipack | ~$2 | ~$6 |
| **Required subtotal** | | | | **~$159** |

## Optional

| Part | Qty | Why | Approx. total |
|---|---|---|---|
| 5 mW visible laser diode | 4 | The LiDAR beam is invisible infrared; a visible dot makes lining a unit up with the painted line quick | ~$7 (10-pack) |
| JST-XH connector kit | 1 | Unplug sensors and radios without desoldering; jumper wires are fine for testing | ~$10 |
| nRF24L01+ PA+LNA modules | 2 | Only if the built-in antenna can't cover the ~35 m sheet | ~$8 |

## Total

About **$159** for the required parts, or about **$184** with every optional part. Both fit the $300 budget, not counting parts already owned.

The first unit needs nothing more than what's owned or on order. The 5 V 4 A supply is the only extra needed to test the first panel at full brightness.
