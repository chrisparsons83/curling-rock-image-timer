#pragma once

// ============================================================================
// Curling Rock Split Timer — Camera Node Configuration
// Board: Freenove ESP32-S3-WROOM CAM (N8R8)
// ============================================================================

// --- Camera pins (Freenove ESP32-S3-WROOM specific) ---
// Verify these against your board revision's pinout diagram.
#define CAM_PIN_PWDN    -1   // No power-down pin
#define CAM_PIN_RESET   -1   // No reset pin (software reset only)
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD     4   // I2C SDA (SCCB)
#define CAM_PIN_SIOC     5   // I2C SCL (SCCB)
#define CAM_PIN_Y9      16
#define CAM_PIN_Y8      17
#define CAM_PIN_Y7      18
#define CAM_PIN_Y6      12
#define CAM_PIN_Y5      10
#define CAM_PIN_Y4       8
#define CAM_PIN_Y3       9
#define CAM_PIN_Y2      11
#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_PCLK    13

// --- Frame settings ---
// QQVGA = 160x120, small enough for fast capture + processing
#define FRAME_WIDTH   160
#define FRAME_HEIGHT  120

// Camera XCLK frequency (Hz). 20MHz is standard; 24MHz can boost FPS.
#define XCLK_FREQ_HZ 20000000

// --- ROI (Region of Interest) for the virtual tripwire ---
// The ROI is a vertical strip of columns in the center of the frame.
// The rock detection algorithm only analyzes pixels within this strip.
//
// Default: center 40 columns (columns 60-99 of 160-wide frame).
// These are overridden at runtime by auto-calibration if it finds the
// painted line.
#define ROI_DEFAULT_X_START  60
#define ROI_DEFAULT_X_END    99
#define ROI_Y_START           0   // Full height
#define ROI_Y_END           119

// --- Detection thresholds ---
// Brightness drop (0-255) from baseline that indicates a rock is present.
// A curling rock at ice level appears as a dark silhouette against bright ice.
// 40 = ~15% drop from typical ice brightness (~240-250).
#define DETECTION_THRESHOLD_DEFAULT  40

// Minimum consecutive frames with detection before triggering.
// Filters out single-frame noise (shadows, glare flicker).
#define MIN_FRAMES_FOR_DETECTION  2

// Minimum frames the ROI must be clear before re-arming.
// Prevents double-triggers from a single slow-moving rock.
#define MIN_FRAMES_CLEAR  5

// --- Calibration ---
// Number of frames to average for baseline capture.
#define CALIBRATION_FRAME_COUNT  30

// How often to refresh the baseline when idle (microseconds).
// 10 seconds — re-averages the "empty ice" brightness periodically.
#define BASELINE_REFRESH_INTERVAL_US  10000000

// --- Line auto-detection ---
// When scanning for the painted line, we look for a dark stripe in the
// horizontal brightness profile. These control the detection sensitivity.
//
// Minimum brightness dip (from local average) to consider as a line.
#define LINE_DETECT_MIN_DIP  30

// Expected line width in pixels at QQVGA from ~0.5m distance.
// Real line is ~2.5cm (1 inch); at 160px across ~1m FOV, that's ~4px.
#define LINE_DETECT_MIN_WIDTH  2
#define LINE_DETECT_MAX_WIDTH  12

// Margin (pixels) added on each side of detected line for the ROI.
#define LINE_DETECT_ROI_MARGIN  15

// --- Serial output ---
#define SERIAL_BAUD  115200

// Print raw ROI brightness every frame (verbose, useful for tuning).
// Comment out for normal operation.
// #define DEBUG_PRINT_EVERY_FRAME

// Print FPS every N frames.
#define FPS_REPORT_INTERVAL  100

// --- Node identity ---
// Which line this camera sits on. Used in serial output and radio messages.
// 1 = near back line, 2 = near hog line, 3 = far hog line, 4 = far back line
#define NODE_ID  1
