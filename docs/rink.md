# Rink measurements (#11)

Measured at Chris's club. Blank entries are still to be measured.

| What | Value | Notes |
|------|-------|-------|
| Side board height above the ice | 5.5 in (14.0 cm) | measured 2026-10-23 |
| Side board thickness | 3.5 in (8.9 cm) | measured 2026-10-23 |
| Side board material | wood | |
| Sheet width, board to board | about 13.5 ft (about 411 cm) | measured 2026-10-23, approximate |
| Far side of the sheet | a board between each pair of sheets | not open to the next sheet |
| What the club allows | anything 3.5 in (8.9 cm) wide or less sitting on top of the boards | confirmed 2026-10-26 |
| Back line to hog line | | standard is 6.40 m (21 ft) |
| Hog line to hog line | | standard is 21.95 m (72 ft) |
| Lighting type | | e.g. LED, fluorescent, metal halide |
| Power outlets near the boards | | where, and how far from each line |
| Air temperature at ice level | | |

Photos: not yet taken.

## What this means for the design

- **Adjacent sheet:** the boards between sheets are 14 cm tall, and the sensor beam sits about 5–8 cm above the ice, so the beam stops at the far board and can't see rocks on the next sheet.
- **Detector settings:** the far board will be the empty-line baseline, at about 411 cm minus however far the sensor sits from its own board. With the default 15 cm margin, the trigger threshold will be about 396 cm. A 29 cm rock touching the far board shows up at about 382 cm, so it still triggers, but with only about 14 cm to spare. If on-ice tests miss far-side rocks, lower the margin.
- **Mounting:** the club allows anything 8.9 cm wide or less on top of the wooden boards. The electronics, battery and display go in a box up there. Where the LiDAR goes is still open, and the rink-night test on #16 decides between two options:
  1. **Hanging arm, level beam:** a short arm, about 2 cm thick, hangs down the ice side of the board and holds the LiDAR so the beam crosses level about 5–8 cm above the ice. It covers the whole sheet and the far board stops the beam. We still need to confirm the club is fine with the arm.
  2. **On top, angled down:** the LiDAR sits on the board at about 15 cm and tilts down about 1.7°, so the beam meets the far board a few cm above the ice. Nothing hangs over the edge. The catch is that the beam only drops below the 11.4 cm rock top about 1.4 m out, so rocks closer than that to the sensor's board pass under it or only clip it. That's probably fine at the hog lines and the delivery-end back line, where rocks run within about 1 m of the centre line (2 m from the board). It's riskier at the far back line.
- **Beam spread:** the TF-Luna's beam is about 2° wide, roughly 14 cm across at the far board. In either option the lower edge will graze the ice at the far side. The rink test shows whether that gives false or weak readings.
