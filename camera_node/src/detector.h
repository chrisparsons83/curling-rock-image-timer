#pragma once

#include <cstdint>
#include <esp_camera.h>
#include "config.h"

// Detection states
enum class DetectorState : uint8_t {
    IDLE,       // No rock in view, monitoring baseline
    ENTERING,   // Brightness dropping — rock entering FOV
    PRESENT,    // Rock fully in ROI
    EXITING,    // Brightness recovering — rock leaving
};

// Result from analyzing one frame's ROI
struct RoiResult {
    uint8_t brightness;   // Average brightness of ROI (0-255)
    int64_t timestamp_us; // Frame capture time (esp_timer_get_time)
};

// Detection event emitted when a rock crossing is detected
struct DetectionEvent {
    bool    detected;          // True if a crossing was detected this frame
    int64_t crossing_time_us;  // Interpolated crossing time (microseconds)
    float   confidence;        // 0.0–1.0 based on how clean the signal was
};

class RockDetector {
public:
    // Initialize with calibration results
    void init(uint8_t baseline, int roi_x_start, int roi_x_end,
              uint8_t threshold = DETECTION_THRESHOLD_DEFAULT);

    // Update the baseline (for periodic recalibration)
    void set_baseline(uint8_t baseline);

    // Analyze a frame's ROI and return brightness + timestamp
    RoiResult analyze_roi(const camera_fb_t* fb);

    // Feed an ROI result into the state machine. Returns a detection event.
    DetectionEvent update(const RoiResult& roi);

    // Get current state (for debugging)
    DetectorState state() const { return state_; }
    const char*   state_name() const;

    // Get current FPS measurement
    float fps() const { return fps_; }

private:
    // Configuration
    uint8_t baseline_   = 128;
    uint8_t threshold_  = DETECTION_THRESHOLD_DEFAULT;
    int     roi_x_start_ = ROI_DEFAULT_X_START;
    int     roi_x_end_   = ROI_DEFAULT_X_END;

    // State machine
    DetectorState state_ = DetectorState::IDLE;
    int frames_in_state_ = 0;

    // History buffer for interpolation (circular, last 8 frames)
    static constexpr int HISTORY_SIZE = 8;
    RoiResult history_[HISTORY_SIZE] = {};
    int       history_idx_ = 0;
    int       history_count_ = 0;

    // FPS tracking
    int64_t fps_start_us_ = 0;
    int     fps_frame_count_ = 0;
    float   fps_ = 0.0f;

    void push_history(const RoiResult& roi);
    bool is_rock_present(uint8_t brightness) const;

    // Estimate the exact time the rock's leading edge crossed the tripwire
    // center, by fitting a line through the brightness transition.
    int64_t interpolate_crossing_time() const;
};
