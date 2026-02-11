#include "detector.h"
#include <esp_timer.h>

void RockDetector::init(uint8_t baseline, int roi_x_start, int roi_x_end,
                        uint8_t threshold) {
    baseline_    = baseline;
    threshold_   = threshold;
    roi_x_start_ = roi_x_start;
    roi_x_end_   = roi_x_end;
    state_       = DetectorState::IDLE;
    frames_in_state_ = 0;
    history_idx_   = 0;
    history_count_ = 0;
    fps_start_us_  = esp_timer_get_time();
    fps_frame_count_ = 0;
    fps_ = 0.0f;
}

void RockDetector::set_baseline(uint8_t baseline) {
    baseline_ = baseline;
}

const char* RockDetector::state_name() const {
    switch (state_) {
        case DetectorState::IDLE:     return "IDLE";
        case DetectorState::ENTERING: return "ENTERING";
        case DetectorState::PRESENT:  return "PRESENT";
        case DetectorState::EXITING:  return "EXITING";
        default:                      return "UNKNOWN";
    }
}

RoiResult RockDetector::analyze_roi(const camera_fb_t* fb) {
    RoiResult result;
    result.timestamp_us = esp_timer_get_time();

    // Validate frame dimensions before pixel access
    if (fb->width != FRAME_WIDTH || fb->height != FRAME_HEIGHT) {
        result.brightness = baseline_;
        return result;
    }

    // Compute average brightness within the ROI strip
    uint32_t sum = 0;
    uint32_t count = 0;
    const uint8_t* buf = fb->buf;
    int width = fb->width;

    for (int y = ROI_Y_START; y <= ROI_Y_END; y++) {
        for (int x = roi_x_start_; x <= roi_x_end_; x++) {
            sum += buf[y * width + x];
            count++;
        }
    }

    result.brightness = (count > 0) ? (uint8_t)(sum / count) : baseline_;

    // Update FPS counter
    fps_frame_count_++;
    int64_t now = result.timestamp_us;
    int64_t elapsed = now - fps_start_us_;
    if (elapsed >= 1000000) {  // Every 1 second
        fps_ = (float)fps_frame_count_ * 1000000.0f / elapsed;
        fps_frame_count_ = 0;
        fps_start_us_ = now;
    }

    return result;
}

void RockDetector::push_history(const RoiResult& roi) {
    history_[history_idx_] = roi;
    history_idx_ = (history_idx_ + 1) % HISTORY_SIZE;
    if (history_count_ < HISTORY_SIZE) history_count_++;
}

bool RockDetector::is_rock_present(uint8_t brightness) const {
    // Rock is present when brightness drops significantly below baseline.
    // Guard against underflow: if brightness > baseline, no rock.
    if (brightness >= baseline_) return false;
    return (baseline_ - brightness) >= threshold_;
}

int64_t RockDetector::interpolate_crossing_time() const {
    // We want to find the moment the brightness crossed the midpoint
    // between baseline and the detected rock brightness.
    //
    // Strategy: look at the last few frames in history, find the transition
    // from "above midpoint" to "below midpoint", and linearly interpolate
    // between those two frames.
    //
    // The midpoint represents the rock's leading edge being at the center
    // of the ROI strip.

    if (history_count_ < 2) {
        // Not enough history — return the most recent timestamp
        int last = (history_idx_ - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        return history_[last].timestamp_us;
    }

    // Find the lowest brightness in recent history (the "rock" brightness)
    uint8_t min_brightness = 255;
    for (int i = 0; i < history_count_; i++) {
        if (history_[i].brightness < min_brightness) {
            min_brightness = history_[i].brightness;
        }
    }

    // Midpoint between baseline and rock brightness
    uint8_t midpoint = (baseline_ + min_brightness) / 2;

    // Walk backwards through history to find the transition frame pair
    // where brightness crosses the midpoint.
    for (int i = 1; i < history_count_; i++) {
        int idx_curr = (history_idx_ - i     + HISTORY_SIZE) % HISTORY_SIZE;
        int idx_prev = (history_idx_ - i - 1 + HISTORY_SIZE) % HISTORY_SIZE;

        uint8_t b_curr = history_[idx_curr].brightness;
        uint8_t b_prev = history_[idx_prev].brightness;

        // Look for the frame pair where brightness transitions through midpoint
        // (going from bright to dark, i.e., prev >= midpoint and curr < midpoint)
        if (b_prev >= midpoint && b_curr < midpoint) {
            int64_t t_prev = history_[idx_prev].timestamp_us;
            int64_t t_curr = history_[idx_curr].timestamp_us;

            // Linear interpolation: at what time did brightness = midpoint?
            float frac = (float)(b_prev - midpoint) / (float)(b_prev - b_curr);
            int64_t crossing = t_prev + (int64_t)(frac * (t_curr - t_prev));
            return crossing;
        }
    }

    // Fallback: if no clean transition found, return the first frame that
    // was below threshold.
    for (int i = history_count_ - 1; i >= 0; i--) {
        int idx = (history_idx_ - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        if (is_rock_present(history_[idx].brightness)) {
            return history_[idx].timestamp_us;
        }
    }

    // Last resort: return current time
    return esp_timer_get_time();
}

DetectionEvent RockDetector::update(const RoiResult& roi) {
    push_history(roi);

    DetectionEvent event = {};
    event.detected = false;

    bool rock_now = is_rock_present(roi.brightness);

    switch (state_) {
        case DetectorState::IDLE:
            if (rock_now) {
                state_ = DetectorState::ENTERING;
                frames_in_state_ = 1;
            }
            break;

        case DetectorState::ENTERING:
            if (rock_now) {
                frames_in_state_++;
                if (frames_in_state_ >= MIN_FRAMES_FOR_DETECTION) {
                    // Confirmed rock crossing — calculate interpolated time
                    state_ = DetectorState::PRESENT;
                    frames_in_state_ = 0;

                    event.detected = true;
                    event.crossing_time_us = interpolate_crossing_time();

                    // Confidence based on how sharp the transition was.
                    // A clean, fast drop = high confidence.
                    if (baseline_ > 0) {
                        int drop = (int)baseline_ - (int)roi.brightness;
                        event.confidence = (float)drop / (float)baseline_;
                        if (event.confidence < 0.0f) event.confidence = 0.0f;
                        if (event.confidence > 1.0f) event.confidence = 1.0f;
                    } else {
                        event.confidence = 0.0f;
                    }
                }
            } else {
                // False alarm — brightness recovered before confirmation
                state_ = DetectorState::IDLE;
                frames_in_state_ = 0;
            }
            break;

        case DetectorState::PRESENT:
            if (!rock_now) {
                state_ = DetectorState::EXITING;
                frames_in_state_ = 1;
            }
            break;

        case DetectorState::EXITING:
            if (!rock_now) {
                frames_in_state_++;
                if (frames_in_state_ >= MIN_FRAMES_CLEAR) {
                    state_ = DetectorState::IDLE;
                    frames_in_state_ = 0;
                    // Clear history so stale data from this detection
                    // doesn't corrupt the next interpolation
                    history_idx_ = 0;
                    history_count_ = 0;
                }
            } else {
                // Rock still partially in view
                state_ = DetectorState::PRESENT;
                frames_in_state_ = 0;
            }
            break;
    }

    return event;
}
