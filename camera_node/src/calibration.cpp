#include "calibration.h"
#include "camera.h"

// Compute the average brightness of a vertical strip [x_start, x_end] across
// all rows [ROI_Y_START, ROI_Y_END] in a grayscale frame.
static uint8_t avg_brightness_in_roi(const uint8_t* buf, int width,
                                     int x_start, int x_end) {
    uint32_t sum = 0;
    uint32_t count = 0;
    for (int y = ROI_Y_START; y <= ROI_Y_END; y++) {
        for (int x = x_start; x <= x_end; x++) {
            sum += buf[y * width + x];
            count++;
        }
    }
    return (count > 0) ? (uint8_t)(sum / count) : 128;
}

// Build a horizontal brightness profile: for each column x, average the
// brightness across all rows. This produces a 1D signal of length `width`.
static void build_column_profile(const uint8_t* buf, int width, int height,
                                 float* profile) {
    for (int x = 0; x < width; x++) {
        uint32_t sum = 0;
        for (int y = 0; y < height; y++) {
            sum += buf[y * width + x];
        }
        profile[x] = (float)sum / height;
    }
}

// Scan the horizontal brightness profile for a dark stripe (the painted line).
// Returns the center column of the detected line, or -1 if not found.
//
// Algorithm:
// 1. Compute a local average using a sliding window (21 columns wide)
// 2. Find regions where brightness dips below (local_avg - LINE_DETECT_MIN_DIP)
// 3. Among those regions, find the widest one within expected width range
// 4. Return its center
static int detect_painted_line(const float* profile, int width) {
    const int window = 21;  // Sliding window for local average
    const int half_w = window / 2;

    int best_center = -1;
    int best_width  = 0;

    // Scan for dark dips
    int dip_start = -1;
    for (int x = half_w; x < width - half_w; x++) {
        // Local average (excluding center region to avoid self-influence)
        float local_sum = 0;
        int   local_count = 0;
        for (int dx = -half_w; dx <= half_w; dx++) {
            if (abs(dx) > LINE_DETECT_MAX_WIDTH / 2) {
                local_sum += profile[x + dx];
                local_count++;
            }
        }
        float local_avg = (local_count > 0) ? local_sum / local_count : 128.0f;

        bool is_dark = (local_avg - profile[x]) >= LINE_DETECT_MIN_DIP;

        if (is_dark && dip_start < 0) {
            dip_start = x;
        } else if (!is_dark && dip_start >= 0) {
            int dip_width = x - dip_start;
            if (dip_width >= LINE_DETECT_MIN_WIDTH &&
                dip_width <= LINE_DETECT_MAX_WIDTH &&
                dip_width > best_width) {
                best_width  = dip_width;
                best_center = dip_start + dip_width / 2;
            }
            dip_start = -1;
        }
    }

    // Handle dip that extends to edge
    if (dip_start >= 0) {
        int dip_width = (width - half_w) - dip_start;
        if (dip_width >= LINE_DETECT_MIN_WIDTH &&
            dip_width <= LINE_DETECT_MAX_WIDTH &&
            dip_width > best_width) {
            best_width  = dip_width;
            best_center = dip_start + dip_width / 2;
        }
    }

    return best_center;
}

CalibrationResult calibration_run() {
    CalibrationResult result = {};
    result.success = false;
    result.line_x  = -1;

    Serial.println("Calibration: capturing empty ice frames...");

    // Accumulate column brightness profile across multiple frames
    float profile[FRAME_WIDTH] = {};
    int   frames_captured = 0;

    for (int i = 0; i < CALIBRATION_FRAME_COUNT; i++) {
        camera_fb_t* fb = camera_capture();
        if (!fb) {
            Serial.printf("Calibration: frame %d capture failed\n", i);
            continue;
        }

        if (fb->width != FRAME_WIDTH || fb->height != FRAME_HEIGHT) {
            Serial.printf("Calibration: unexpected frame size %dx%d\n",
                          fb->width, fb->height);
            esp_camera_fb_return(fb);
            continue;
        }

        // Accumulate per-column brightness
        float frame_profile[FRAME_WIDTH];
        build_column_profile(fb->buf, fb->width, fb->height, frame_profile);
        for (int x = 0; x < FRAME_WIDTH; x++) {
            profile[x] += frame_profile[x];
        }

        frames_captured++;
        esp_camera_fb_return(fb);
    }

    if (frames_captured < 5) {
        Serial.println("Calibration: too few frames captured, aborting");
        return result;
    }

    // Average the accumulated profile
    for (int x = 0; x < FRAME_WIDTH; x++) {
        profile[x] /= frames_captured;
    }

    // Try to detect the painted line
    int line_center = detect_painted_line(profile, FRAME_WIDTH);

    int roi_x_start, roi_x_end;
    if (line_center >= 0) {
        // Center ROI on the detected line with margin
        roi_x_start = line_center - LINE_DETECT_ROI_MARGIN;
        roi_x_end   = line_center + LINE_DETECT_ROI_MARGIN;

        // Clamp to frame bounds
        if (roi_x_start < 0) roi_x_start = 0;
        if (roi_x_end >= FRAME_WIDTH) roi_x_end = FRAME_WIDTH - 1;

        Serial.printf("Calibration: painted line detected at column %d\n",
                       line_center);
        Serial.printf("Calibration: ROI set to columns %d-%d\n",
                       roi_x_start, roi_x_end);
    } else {
        // Fall back to default center strip
        roi_x_start = ROI_DEFAULT_X_START;
        roi_x_end   = ROI_DEFAULT_X_END;

        Serial.println("Calibration: no painted line detected, using default ROI");
        Serial.printf("Calibration: ROI set to columns %d-%d (default)\n",
                       roi_x_start, roi_x_end);
    }

    // Compute baseline brightness within the ROI using a fresh batch of frames
    uint32_t baseline_sum = 0;
    int baseline_count = 0;
    for (int i = 0; i < 10; i++) {
        camera_fb_t* fb = camera_capture();
        if (!fb) continue;

        baseline_sum += avg_brightness_in_roi(fb->buf, fb->width,
                                               roi_x_start, roi_x_end);
        baseline_count++;
        esp_camera_fb_return(fb);
    }

    if (baseline_count == 0) {
        Serial.println("Calibration: failed to capture baseline frames");
        return result;
    }

    result.success     = true;
    result.baseline    = (uint8_t)(baseline_sum / baseline_count);
    result.roi_x_start = roi_x_start;
    result.roi_x_end   = roi_x_end;
    result.line_x      = line_center;

    Serial.printf("Calibration complete: baseline=%d, ROI=[%d,%d]",
                   result.baseline, roi_x_start, roi_x_end);
    if (line_center >= 0) {
        Serial.printf(", line_col=%d", line_center);
    }
    Serial.println();

    // Print the brightness profile for debugging / visualization
    Serial.println("Column brightness profile (avg across rows):");
    for (int x = 0; x < FRAME_WIDTH; x += 4) {
        int bar_len = (int)(profile[x] / 8);  // Scale 0-255 to 0-31
        Serial.printf("%3d|", x);
        for (int b = 0; b < bar_len; b++) Serial.print('#');
        if (x >= roi_x_start && x <= roi_x_end) Serial.print(" <ROI");
        if (x == line_center) Serial.print(" <LINE");
        Serial.println();
    }

    return result;
}

uint8_t calibration_refresh_baseline(int roi_x_start, int roi_x_end) {
    uint32_t sum = 0;
    int count = 0;

    for (int i = 0; i < 10; i++) {
        camera_fb_t* fb = camera_capture();
        if (!fb) continue;

        sum += avg_brightness_in_roi(fb->buf, fb->width,
                                      roi_x_start, roi_x_end);
        count++;
        esp_camera_fb_return(fb);
    }

    return (count > 0) ? (uint8_t)(sum / count) : 128;
}
