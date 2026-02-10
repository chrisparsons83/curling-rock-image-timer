# Curling Rock Split Timer — Project Plan

## Overview

A system of 4 camera nodes positioned at ice level on the back lines and hog
lines of a curling sheet, plus 1 display node. When a rock is slid from the
hack, the system measures split times (back-to-hog, hog-to-hog) and displays
them on a dedicated screen at the sheet.

**Target accuracy:** 10ms ideal, 50ms acceptable for proof of concept.

## Architecture

```
                         ~~~~ Curling Sheet ~~~~

  [Hack]                                                      [House]
    |                                                            |
    |   BACK LINE        HOG LINE         HOG LINE    BACK LINE  |
    |      |                |                |           |       |
    |   [Node 1]         [Node 2]         [Node 3]   [Node 4]   |
    |   ESP32-CAM        ESP32-CAM        ESP32-CAM   ESP32-CAM  |
    |      |                |                |           |       |
    |      +-------NRF24L01 radio mesh-------+           |       |
    |                       |                            |       |
    |                  [Display Node]                             |
    |                  ESP32 + LCD/OLED                           |
    |                  + NRF24L01                                 |
```

### Hardware Per Node

**Camera Node (x4):**
| Component | Est. Cost | Notes |
|-----------|-----------|-------|
| ESP32-CAM (AI-Thinker) | ~$5 | OV2640 sensor, built-in |
| NRF24L01+PA+LNA | ~$3 | Long-range version w/ antenna, need range for ~30m sheet |
| 3.3V regulator + capacitor | ~$1 | Clean power for NRF24L01 (10uF + 100nF on Vcc) |
| USB power supply + cable | ~$5 | Wall-powered at rink |
| 3D-printed enclosure | ~$2 | Ice-level mount with camera window |
| **Subtotal** | **~$16** | |

**Display Node (x1):**
| Component | Est. Cost | Notes |
|-----------|-----------|-------|
| ESP32 DevKit | ~$5 | Any ESP32 dev board |
| NRF24L01+PA+LNA | ~$3 | Match camera nodes |
| 2.42" OLED (SSD1309) or 3.5" ILI9488 LCD | ~$8-15 | Visible from ~3m away |
| Buttons (start/reset/mode) | ~$2 | User controls |
| Enclosure + mount | ~$5 | |
| **Subtotal** | **~$25-30** | |

**Total system: ~$90-95**

### NRF24L01 Wiring on ESP32-CAM (Using SD Card Pins)

Since we don't need the SD card, those GPIOs are free:

| NRF24L01 Pin | ESP32-CAM GPIO | Notes |
|-------------|----------------|-------|
| VCC | 3.3V | Add 10uF + 100nF caps! |
| GND | GND | |
| SCK | GPIO 14 | SD_CLK repurposed |
| MISO | GPIO 2 | SD_DATA0 repurposed |
| MOSI | GPIO 15 | SD_CMD repurposed |
| CSN | GPIO 13 | SD_DATA3 repurposed |
| CE | GPIO 12 | SD_DATA2 — **caution: boot-strapping pin** — add 10k pulldown |
| IRQ | — | Not connected (polled instead) |

**Important:** GPIO 12 must be LOW at boot or the ESP32 won't start (it
controls flash voltage). Add a 10k pulldown resistor. The NRF24L01 CE pin
is an output from the ESP32, so it won't conflict after boot. Alternatively,
use GPIO 4 for CE (flash LED pin — just don't use the flash).

Use **software SPI** via the RF24 library's `SoftSPI` or configure HSPI
manually to avoid conflicts with the camera's I2S/DMA on the VSPI bus.

---

## Detection Approach: Narrow-ROI Color Change Detection

### Why Not Full Object Detection?

Full-frame ML-based object detection (FOMO, MobileNet, etc.) on ESP32:
- Runs at 5-10 FPS max
- High latency (~100-200ms per inference)
- Overkill — we know exactly what we're looking for and where

### The Line-Scan Approach

The camera is mounted at **ice level**, aimed perpendicular to the sheet so it
looks **across** the line. The field of view covers ~0.5-1m of the line.

```
  Camera FOV (top-down view):

  ←——— line ———→
  |             |
  |   [rock]    |     rock slides through FOV
  |     ●——→    |
  |             |
  ←——— line ———→
       ↑
    camera here, at ice level, looking across
```

**Algorithm:**
1. Capture frames at QQVGA (160x120) in JPEG, decode to grayscale or RGB565
2. Analyze only a **narrow vertical strip** (e.g., columns 60-100, all rows) —
   this is the "virtual tripwire"
3. Compute average color/brightness in the ROI for each frame
4. Curling rocks are distinctly colored (red or yellow) against white ice
5. Detect a significant change in the ROI → rock is crossing the line
6. **Sub-frame interpolation:** Track the rock's leading edge position across
   3-4 frames, fit a linear model, extrapolate exact crossing time

### Expected Performance

| Parameter | Value |
|-----------|-------|
| Resolution | QQVGA 160x120 |
| Format | JPEG (hardware encoder) → partial decode |
| Target FPS | 25-30 |
| Frame interval | 33-40ms |
| ROI processing time | <2ms (just averaging ~2400 pixels) |
| NRF24L01 TX time | ~1ms |
| **Effective accuracy w/ interpolation** | **~10-15ms** |

### Calibration Mode

Each camera node needs a one-time calibration:
1. Capture "empty ice" baseline (average brightness/color of ROI)
2. Set threshold for "rock present" (e.g., 30% brightness drop)
3. Store in ESP32 flash (NVS)
4. Recalibrate via button press or radio command from display node

---

## Communication Protocol (NRF24L01)

### Network Topology

**Star topology** with the display node as the central hub.

- Display node: address `0x4355524C00` ("CURL\0") — listens on pipe 0
- Camera nodes: addresses `0x4355524C01` through `0x4355524C04`
- Each camera node transmits to the display node
- Display node can broadcast to all cameras (for sync, config)

### Message Types

```
Byte 0: Message type
Bytes 1-4: Timestamp (microseconds, uint32_t)
Bytes 5+: Payload (varies by type)

Types:
  0x01 SYNC_REQUEST    — Display → All cameras (broadcast)
  0x02 SYNC_RESPONSE   — Camera → Display (with local timestamp)
  0x03 ROCK_DETECTED   — Camera → Display (node_id, crossing_time_us)
  0x04 CALIBRATE       — Display → Specific camera
  0x05 STATUS          — Camera → Display (heartbeat, battery, FPS)
  0x06 CONFIG          — Display → Camera (threshold, ROI, etc.)
  0x07 RESET           — Display → All (reset timing state)
```

### Time Synchronization

This is critical for accurate split times. Each ESP32 has its own `micros()`
clock that drifts independently.

**Protocol (simplified NTP-like):**
1. Display node sends `SYNC_REQUEST` with its local time `T1`
2. Camera node receives at its local time `T2`, immediately responds with
   `SYNC_RESPONSE` containing `T2` and its current time `T3`
3. Display node receives response at `T4`
4. Round-trip time: `RTT = (T4 - T1) - (T3 - T2)`
5. Clock offset: `offset = ((T2 - T1) + (T3 - T4)) / 2`
6. Repeat every ~5 seconds, use moving average to smooth

**Expected sync accuracy:** ~0.5-1ms (NRF24L01 has very consistent latency)

When a camera detects a rock crossing, it sends the crossing time in its
**local clock**. The display node converts to global time using the known
offset.

---

## Firmware Structure

### Camera Node Firmware

```
camera_node/
├── platformio.ini
├── src/
│   ├── main.cpp              — Setup, main loop
│   ├── camera.h/cpp          — Camera init, frame capture, ROI extraction
│   ├── detector.h/cpp        — Rock detection algorithm + interpolation
│   ├── radio.h/cpp           — NRF24L01 communication
│   ├── sync.h/cpp            — Time synchronization
│   ├── config.h              — Pin definitions, constants
│   └── calibration.h/cpp     — Baseline capture, threshold storage
```

**Main loop (pseudocode):**
```
void loop() {
    // 1. Check for incoming radio messages (sync, config, reset)
    radio.poll();

    // 2. Capture frame
    camera_fb_t* fb = esp_camera_fb_get();

    // 3. Extract ROI and compute metrics
    RoiResult roi = detector.analyzeROI(fb);

    // 4. Update rock tracking state machine
    DetectionEvent event = detector.update(roi, micros());

    // 5. If rock crossing detected, send timestamp
    if (event.type == CROSSING_DETECTED) {
        radio.sendRockDetected(NODE_ID, event.crossing_time_us);
    }

    // 6. Release frame buffer
    esp_camera_fb_return(fb);
}
```

**Detection State Machine:**
```
States:
  IDLE          → No rock in view, monitoring baseline
  ENTERING      → ROI brightness dropping, rock entering FOV
  PRESENT       → Rock fully in ROI
  EXITING       → ROI brightness recovering, rock leaving

Transitions:
  IDLE → ENTERING:      brightness drops below threshold
  ENTERING → PRESENT:   brightness stabilizes at low level
  PRESENT → EXITING:    brightness starts recovering
  EXITING → IDLE:       brightness returns to baseline

The CROSSING timestamp is calculated during the ENTERING phase by
interpolating when the leading edge crossed the center of the ROI.
```

### Display Node Firmware

```
display_node/
├── platformio.ini
├── src/
│   ├── main.cpp              — Setup, main loop
│   ├── radio.h/cpp           — NRF24L01 communication
│   ├── sync.h/cpp            — Time synchronization (master side)
│   ├── timing.h/cpp          — Split time calculation, state machine
│   ├── display.h/cpp         — OLED/LCD rendering
│   ├── buttons.h/cpp         — User input handling
│   └── config.h              — Pin definitions, constants
```

**Timing State Machine:**
```
States:
  READY         → Waiting for first detection (back line)
  SPLIT_1       → Back line triggered, waiting for near hog
  SPLIT_2       → Near hog triggered, waiting for far hog
  SPLIT_3       → Far hog triggered, waiting for far back (optional)
  COMPLETE      → All splits recorded, displaying results

After COMPLETE, auto-reset after 10 seconds or manual reset via button.
```

**Display Layout (example for 2.42" OLED):**
```
┌──────────────────────┐
│  CURLING SPLIT TIMER │
│                      │
│  Back→Hog:   1.23s   │
│  Hog→Hog:    4.56s   │
│  Total:      5.79s   │
│                      │
│  ● Node1 ● Node2    │
│  ● Node3 ● Node4    │
│  [READY]             │
└──────────────────────┘
(● = green dot for connected, red for disconnected)
```

---

## Implementation Phases

### Phase 1: Single Camera Proof of Concept
**Goal:** Prove rock detection works at ice level.

1. Flash one ESP32-CAM with basic camera capture code
2. Mount at ice level (tape to boards, aim across a line)
3. Capture frames, analyze ROI brightness as rock passes
4. Log timestamps to Serial, verify detection reliability
5. Tune threshold and ROI position
6. **Deliverable:** Serial output showing detected crossing times

### Phase 2: Two-Node Communication
**Goal:** Prove NRF24L01 communication and time sync.

1. Add NRF24L01 to one ESP32-CAM (using SD card pins)
2. Build display node (ESP32 + NRF24L01 + OLED)
3. Implement time sync protocol
4. Camera sends detection events, display shows "rock detected"
5. Measure sync accuracy (send known events, compare)
6. **Deliverable:** Single camera triggers display update in <5ms

### Phase 3: Two-Camera Split Timer
**Goal:** Measure one split time (back-to-hog or hog-to-hog).

1. Add second camera node
2. Implement timing state machine on display node
3. Slide rocks, measure splits, compare against stopwatch
4. Implement sub-frame interpolation
5. Tune for accuracy
6. **Deliverable:** Reliable single-split measurement within 20ms of manual timing

### Phase 4: Full Four-Camera System
**Goal:** Complete system with all splits.

1. Build remaining two camera nodes
2. Implement full timing state machine (3 splits)
3. Add calibration mode
4. Add status indicators (node connectivity)
5. Polish display layout
6. **Deliverable:** Full split timer system

### Phase 5: Refinement
**Goal:** Make it robust for regular use.

1. Design and print enclosures (waterproof — ice rinks are wet)
2. Add persistent configuration (NVS)
3. Handle edge cases (rock stops on line, multiple rocks, brooms)
4. Add "direction detection" (which way is the rock traveling?)
5. Consider adding session logging (store times for review)

---

## Key Technical Risks & Mitigations

### 1. Camera FPS Too Low
**Risk:** ESP32-CAM can't sustain 25+ FPS at QQVGA.
**Mitigation:** Even at 15 FPS (67ms frames), interpolation across 3-4 frames
should yield ~20ms accuracy. Acceptable for POC. If needed, try overclocking
XCLK to 24MHz or using grayscale pixel format instead of JPEG.

### 2. False Triggers (Brooms, Feet, Shadows)
**Risk:** Non-rock objects trigger detection.
**Mitigation:**
- Rocks have a distinctive size and shape in the FOV at ice level
- Require sustained brightness change for 2+ frames (rocks take 50-100ms to
  cross FOV, feet/brooms are faster or have different profile)
- Color detection: rocks are red/yellow, brooms are not
- Direction filtering: only count objects moving in the expected direction

### 3. Ice Glare / Lighting Conditions
**Risk:** Overhead arena lights cause glare at ice level.
**Mitigation:**
- Camera positioned low and angled slightly upward captures rock against a
  darker background (boards/glass, not overhead lights)
- Auto-exposure and auto-white-balance on OV2640
- Adaptive baseline: recalibrate periodically when no rock is detected

### 4. NRF24L01 Range (30m+ sheet)
**Risk:** Signal doesn't reach across the full sheet.
**Mitigation:**
- Use PA+LNA version (claimed 1000m+ line of sight)
- 250kbps data rate (longer range than 1/2 Mbps)
- Display node positioned at center of sheet
- Retry logic on critical messages

### 5. Time Sync Drift
**Risk:** ESP32 crystal oscillators drift, degrading sync over time.
**Mitigation:**
- Re-sync every 5 seconds (ESP32 drift is ~50ppm = 50us per second)
- Between syncs, max drift is ~250us — well within our accuracy target
- Use `esp_timer_get_time()` (64-bit microsecond timer) instead of `micros()`

---

## Bill of Materials (Complete System)

| Qty | Item | Unit Cost | Total |
|-----|------|-----------|-------|
| 4 | ESP32-CAM (AI-Thinker) | $5 | $20 |
| 5 | NRF24L01+PA+LNA w/ antenna | $3 | $15 |
| 1 | ESP32 DevKit V1 | $5 | $5 |
| 1 | 2.42" OLED SSD1309 (or 3.5" LCD) | $10 | $10 |
| 5 | 10uF + 100nF capacitors (for NRF24L01) | $0.50 | $2.50 |
| 4 | 10k resistors (GPIO 12 pulldown) | $0.10 | $0.40 |
| 5 | USB-C power supplies (5V 1A) | $5 | $25 |
| 3 | Tactile buttons | $0.50 | $1.50 |
| 5 | 3D printed enclosures | $3 | $15 |
| — | Hookup wire, headers, breadboards | — | $10 |
| | | **Total** | **~$105** |

---

## Development Environment

- **IDE:** PlatformIO (VS Code extension)
- **Framework:** Arduino (for ESP32-CAM camera driver compatibility)
- **Libraries:**
  - `esp32-camera` — camera driver
  - `RF24` — NRF24L01 communication (nRF24/RF24)
  - `U8g2` or `Adafruit_SSD1306` — OLED display
  - `ArduinoJson` — config serialization (optional)
  - `JPEGDEC` — fast JPEG decoding (bitbank2/JPEGDEC, from your link)

---

## Stretch Goals

- **Web dashboard:** Add ESP32 WiFi AP on display node, serve a simple web
  page for session history and charts
- **Bluetooth:** Broadcast splits via BLE for phone notifications
- **Multiple sheets:** Run independent systems on adjacent sheets
- **Rock speed estimation:** With known distances, calculate velocity curves
- **Video replay:** Buffer last N frames on camera nodes, send on request
