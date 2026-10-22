#include "crossing_detector.h"

#include <algorithm>
#include <iterator>

namespace curling {

CrossingDetector::CrossingDetector(const Config& config) : config_(config) {}

void CrossingDetector::recalibrate() {
    state_ = State::kCalibrating;
    std::fill(std::begin(calibration_hist_), std::end(calibration_hist_), 0);
    calibration_seen_ = 0;
    calibration_reliable_ = 0;
    run_ = 0;
}

uint16_t CrossingDetector::threshold_cm() const {
    if (!calibrated()) return 0;
    const int below_baseline = static_cast<int>(baseline_cm_) - config_.baseline_margin_cm;
    const int limit = std::min<int>(config_.sheet_limit_cm, below_baseline);
    return static_cast<uint16_t>(std::max(limit, 0));
}

bool CrossingDetector::on_line(uint16_t distance_cm, bool reliable) const {
    return reliable && distance_cm < threshold_cm();
}

CrossingDetector::Event CrossingDetector::feed(int64_t t_us, uint16_t distance_cm,
                                               bool reliable) {
    switch (state_) {
        case State::kCalibrating: {
            ++calibration_seen_;
            if (reliable) {
                const size_t bin = std::min<size_t>(distance_cm, kCalibrationBins - 1);
                ++calibration_hist_[bin];
                ++calibration_reliable_;
            }
            if (calibration_seen_ < config_.calibration_samples) return Event::kNone;
            if (calibration_reliable_ == 0) {
                // Nothing reliable in range (e.g. open space): only the sheet
                // limit applies.
                baseline_cm_ = config_.sheet_limit_cm + config_.baseline_margin_cm;
            } else {
                uint32_t count = 0;
                size_t bin = 0;
                for (; bin < kCalibrationBins; ++bin) {
                    count += calibration_hist_[bin];
                    if (count * 2 >= calibration_reliable_) break;
                }
                baseline_cm_ = static_cast<uint16_t>(bin);
            }
            baseline_q8_ = static_cast<int32_t>(baseline_cm_) * 256;
            state_ = State::kIdle;
            return Event::kNone;
        }

        case State::kIdle:
            if (on_line(distance_cm, reliable)) {
                begin_candidate(t_us, distance_cm);
                if (config_.confirm_samples <= 1) {
                    state_ = State::kActive;
                    return Event::kStarted;
                }
                state_ = State::kPending;
            } else {
                update_baseline(distance_cm, reliable);
            }
            return Event::kNone;

        case State::kPending:
            if (!on_line(distance_cm, reliable)) {
                // Too short to be a rock: a stray reading.
                state_ = State::kIdle;
                return Event::kNone;
            }
            record(t_us, distance_cm);
            if (++run_ < config_.confirm_samples) return Event::kNone;
            state_ = State::kActive;
            run_ = 0;
            return Event::kStarted;

        case State::kActive:
            if (on_line(distance_cm, reliable)) {
                run_ = 0;
                record(t_us, distance_cm);
                if (t_us - crossing_.start_us > config_.max_duration_us) return finish(true);
                return Event::kNone;
            }
            if (++run_ < config_.clear_samples) return Event::kNone;
            return finish(false);
    }
    return Event::kNone;
}

void CrossingDetector::begin_candidate(int64_t t_us, uint16_t distance_cm) {
    crossing_ = {};
    crossing_.start_us = t_us;
    crossing_.min_cm = distance_cm;
    recorded_len_ = 0;
    run_ = 0;
    record(t_us, distance_cm);
    run_ = 1;
}

void CrossingDetector::record(int64_t t_us, uint16_t distance_cm) {
    crossing_.end_us = t_us;
    ++crossing_.samples;
    crossing_.min_cm = std::min(crossing_.min_cm, distance_cm);
    if (recorded_len_ < kMaxRecorded) recorded_[recorded_len_++] = distance_cm;
}

CrossingDetector::Event CrossingDetector::finish(bool timed_out) {
    std::nth_element(recorded_, recorded_ + recorded_len_ / 2, recorded_ + recorded_len_);
    crossing_.median_cm = recorded_[recorded_len_ / 2];
    crossing_.timed_out = timed_out;
    run_ = 0;
    if (timed_out) {
        recalibrate();
    } else {
        state_ = State::kIdle;
    }
    return Event::kEnded;
}

void CrossingDetector::update_baseline(uint16_t distance_cm, bool reliable) {
    // Only readings near the current baseline move it, so a rock just
    // outside the margin can't drag it down. Time constant ~256 samples (~1 s).
    if (!reliable) return;
    const int diff = static_cast<int>(distance_cm) - baseline_cm_;
    if (diff < -static_cast<int>(config_.baseline_margin_cm) || diff > 50) return;
    baseline_q8_ += (static_cast<int32_t>(distance_cm) * 256 - baseline_q8_) / 256;
    baseline_cm_ = static_cast<uint16_t>((baseline_q8_ + 128) / 256);
}

}  // namespace curling
