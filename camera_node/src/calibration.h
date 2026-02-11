#pragma once

#include <esp_camera.h>
#include "config.h"

// Result of the calibration process.
struct CalibrationResult {
    bool    success;
    uint8_t baseline;    // Average brightness of ROI on empty ice (0-255)
    int     roi_x_start; // Tripwire ROI left column
    int     roi_x_end;   // Tripwire ROI right column
    int     line_x;      // Detected painted line center column (-1 if not found)
};

// Run the full calibration sequence:
// 1. Capture CALIBRATION_FRAME_COUNT frames of empty ice
// 2. Build a horizontal brightness profile (average brightness per column)
// 3. Auto-detect the painted line (dark stripe on bright ice)
// 4. Position the ROI centered on the line (or use defaults)
// 5. Compute baseline brightness within the ROI
//
// This takes ~1-2 seconds. Call it during setup and whenever recalibration
// is needed (button press, radio command, or periodic refresh).
CalibrationResult calibration_run();

// Lightweight baseline refresh: average ROI brightness over a few frames
// without re-detecting the line. Use this for periodic drift correction.
uint8_t calibration_refresh_baseline(int roi_x_start, int roi_x_end);
