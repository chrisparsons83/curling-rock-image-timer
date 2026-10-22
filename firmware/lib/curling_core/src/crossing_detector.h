#pragma once

#include <cstddef>
#include <cstdint>

namespace curling {

// Turns a stream of distance samples from a LiDAR aimed along a line into
// crossing events. A crossing starts at the first sample that sees something
// nearer than the empty-line baseline (the leading edge) and ends once the
// line has been clear for a few samples.
class CrossingDetector {
public:
    struct Config {
        // Anything at or beyond this distance is ignored, e.g. a rock on the
        // adjacent sheet. Set it to the sheet width.
        uint16_t sheet_limit_cm = 450;
        // How much nearer than the empty-line baseline a reading must be to
        // count as something on the line.
        uint16_t baseline_margin_cm = 15;
        // Consecutive in-line samples needed to confirm a crossing. The start
        // time is still the first of them.
        uint8_t confirm_samples = 2;
        // Consecutive clear samples that end a crossing.
        uint8_t clear_samples = 3;
        // Samples used to learn the baseline at startup, before any detection.
        uint16_t calibration_samples = 250;
        // A crossing longer than this is treated as something parked in the
        // beam: it ends with timed_out set and the baseline is relearned.
        int64_t max_duration_us = 5000000;
    };

    struct Crossing {
        int64_t start_us;  // first in-line sample (leading edge)
        int64_t end_us;    // last in-line sample
        uint32_t samples;  // in-line samples counted
        uint16_t min_cm;
        uint16_t median_cm;
        bool timed_out;
    };

    enum class Event { kNone, kStarted, kEnded };

    CrossingDetector() : CrossingDetector(Config{}) {}
    explicit CrossingDetector(const Config& config);

    // Feeds one sample. reliable is false when the sensor says the distance
    // can't be trusted (weak or saturated signal); such samples count as a
    // clear line. Returns kStarted when a crossing is confirmed and kEnded
    // when it finishes. Read crossing() right after either: on kStarted it
    // holds the start time, on kEnded the full record.
    Event feed(int64_t t_us, uint16_t distance_cm, bool reliable);

    const Crossing& crossing() const { return crossing_; }
    bool calibrated() const { return state_ != State::kCalibrating; }
    bool active() const { return state_ == State::kActive; }
    // Empty-line distance, or 0 while calibrating.
    uint16_t baseline_cm() const { return calibrated() ? baseline_cm_ : 0; }
    // Readings nearer than this count as something on the line.
    uint16_t threshold_cm() const;

    // Starts learning the baseline again, e.g. after moving the unit.
    void recalibrate();

private:
    static constexpr size_t kMaxRecorded = 256;
    enum class State { kCalibrating, kIdle, kPending, kActive };

    bool on_line(uint16_t distance_cm, bool reliable) const;
    void begin_candidate(int64_t t_us, uint16_t distance_cm);
    void record(int64_t t_us, uint16_t distance_cm);
    Event finish(bool timed_out);
    void update_baseline(uint16_t distance_cm, bool reliable);

    Config config_;
    State state_ = State::kCalibrating;

    // Calibration: reliable readings are histogrammed in 1 cm bins and the
    // baseline set to their median, so a few stray readings can't skew it.
    // The TF-Luna's range tops out at 800 cm.
    static constexpr size_t kCalibrationBins = 801;
    uint16_t calibration_hist_[kCalibrationBins] = {};
    uint16_t calibration_seen_ = 0;
    uint16_t calibration_reliable_ = 0;

    // Baseline in 1/256 cm, tracked with a slow moving average while idle.
    int32_t baseline_q8_ = 0;
    uint16_t baseline_cm_ = 0;

    Crossing crossing_ = {};
    uint8_t run_ = 0;  // consecutive in-line (pending) or clear (active) samples
    uint16_t recorded_[kMaxRecorded] = {};
    size_t recorded_len_ = 0;
};

}  // namespace curling
