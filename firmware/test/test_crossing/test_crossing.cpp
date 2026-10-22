#include <unity.h>

#include <vector>

#include "crossing_detector.h"

using curling::CrossingDetector;
using Event = CrossingDetector::Event;

void setUp() {}
void tearDown() {}

namespace {

constexpr int64_t kPeriodUs = 4115;  // ~243 Hz, as measured on the bench

struct Sample {
    uint16_t cm;
    bool reliable;
};

// Feeds samples at a fixed rate and collects what the detector reports.
struct Harness {
    CrossingDetector det;
    int64_t t_us = 0;
    std::vector<int64_t> starts;
    std::vector<CrossingDetector::Crossing> ended;

    explicit Harness(const CrossingDetector::Config& c = {}) : det(c) {}

    void feed(uint16_t cm, int n = 1, bool reliable = true) {
        for (int i = 0; i < n; ++i) {
            const Event e = det.feed(t_us, cm, reliable);
            if (e == Event::kStarted) starts.push_back(det.crossing().start_us);
            if (e == Event::kEnded) ended.push_back(det.crossing());
            t_us += kPeriodUs;
        }
    }

    // Empty sheet with the far board at baseline_cm, long enough to calibrate.
    void calibrate(uint16_t baseline_cm = 460) { feed(baseline_cm, 250); }
};

}  // namespace

void test_calibrates_to_far_board() {
    Harness h;
    TEST_ASSERT_FALSE(h.det.calibrated());
    h.calibrate(460);
    TEST_ASSERT_TRUE(h.det.calibrated());
    TEST_ASSERT_EQUAL_UINT16(460, h.det.baseline_cm());
    // The far board minus the margin is nearer than the sheet limit.
    TEST_ASSERT_EQUAL_UINT16(445, h.det.threshold_cm());
}

void test_no_detection_while_calibrating() {
    Harness h;
    h.feed(460, 100);
    h.feed(200, 30);
    h.feed(460, 120);
    TEST_ASSERT_EQUAL(0, h.starts.size());
}

void test_detects_rock_with_leading_edge_time() {
    Harness h;
    h.calibrate();
    h.feed(460, 10);
    const int64_t first_rock_sample = h.t_us;
    h.feed(210, 3);
    h.feed(200, 20);
    h.feed(215, 3);
    h.feed(460, 10);

    TEST_ASSERT_EQUAL(1, h.starts.size());
    TEST_ASSERT_EQUAL_INT64(first_rock_sample, h.starts[0]);
    TEST_ASSERT_EQUAL(1, h.ended.size());
    const auto& c = h.ended[0];
    TEST_ASSERT_EQUAL_INT64(first_rock_sample, c.start_us);
    TEST_ASSERT_EQUAL_INT64(first_rock_sample + 25 * kPeriodUs, c.end_us);
    TEST_ASSERT_EQUAL_UINT32(26, c.samples);
    TEST_ASSERT_EQUAL_UINT16(200, c.min_cm);
    TEST_ASSERT_EQUAL_UINT16(200, c.median_cm);
    TEST_ASSERT_FALSE(c.timed_out);
}

void test_ignores_single_sample_spike() {
    Harness h;
    h.calibrate();
    h.feed(150, 1);
    h.feed(460, 20);
    TEST_ASSERT_EQUAL(0, h.starts.size());
    TEST_ASSERT_EQUAL(0, h.ended.size());
}

void test_ignores_adjacent_sheet() {
    // Far board missing (e.g. open end), so the baseline sits beyond the
    // sheet: a rock at 6 m on the next sheet must not trigger.
    Harness h;
    h.calibrate(800);
    h.feed(600, 40);
    h.feed(800, 20);
    TEST_ASSERT_EQUAL(0, h.starts.size());
    TEST_ASSERT_EQUAL_UINT16(450, h.det.threshold_cm());
}

void test_narrow_sheet_uses_baseline() {
    // Far board nearer than the configured sheet limit: the board itself
    // must not count as a crossing.
    Harness h;
    h.calibrate(420);
    h.feed(420, 100);
    TEST_ASSERT_EQUAL(0, h.starts.size());
    TEST_ASSERT_UINT16_WITHIN(5, 405, h.det.threshold_cm());
    h.feed(300, 10);
    h.feed(420, 10);
    TEST_ASSERT_EQUAL(1, h.ended.size());
}

void test_brief_dropout_inside_rock_is_one_crossing() {
    Harness h;
    h.calibrate();
    h.feed(200, 10);
    h.feed(460, 2);  // fewer than clear_samples
    h.feed(200, 10);
    h.feed(460, 10);
    TEST_ASSERT_EQUAL(1, h.starts.size());
    TEST_ASSERT_EQUAL(1, h.ended.size());
    TEST_ASSERT_EQUAL_UINT32(20, h.ended[0].samples);
}

void test_two_rocks_are_two_crossings() {
    Harness h;
    h.calibrate();
    h.feed(200, 10);
    h.feed(460, 10);
    h.feed(250, 10);
    h.feed(460, 10);
    TEST_ASSERT_EQUAL(2, h.ended.size());
    TEST_ASSERT_EQUAL_UINT16(200, h.ended[0].min_cm);
    TEST_ASSERT_EQUAL_UINT16(250, h.ended[1].min_cm);
}

void test_unreliable_samples_count_as_clear() {
    Harness h;
    h.calibrate();
    h.feed(200, 10, false);
    TEST_ASSERT_EQUAL(0, h.starts.size());
    h.feed(200, 10);
    h.feed(0, 5, false);
    TEST_ASSERT_EQUAL(1, h.ended.size());
}

void test_parked_object_times_out_and_becomes_baseline() {
    Harness h;
    h.calibrate();
    h.feed(150, 1300);  // ~5.3 s
    TEST_ASSERT_EQUAL(1, h.ended.size());
    TEST_ASSERT_TRUE(h.ended[0].timed_out);
    TEST_ASSERT_FALSE(h.det.calibrated());
    h.feed(150, 250);
    TEST_ASSERT_TRUE(h.det.calibrated());
    TEST_ASSERT_UINT16_WITHIN(5, 150, h.det.baseline_cm());
    h.feed(150, 100);
    TEST_ASSERT_EQUAL(1, h.starts.size());
}

void test_baseline_tracks_slow_drift_but_not_rocks() {
    Harness h;
    h.calibrate(460);
    h.feed(455, 2000);
    TEST_ASSERT_UINT16_WITHIN(1, 455, h.det.baseline_cm());
    h.feed(300, 3);  // a stray reading well inside the margin doesn't move it
    h.feed(455, 10);
    TEST_ASSERT_UINT16_WITHIN(1, 455, h.det.baseline_cm());
}

void test_noisy_calibration_uses_median() {
    Harness h;
    h.feed(460, 200);
    h.feed(100, 30);  // someone walks past during calibration
    h.feed(460, 20);
    TEST_ASSERT_UINT16_WITHIN(5, 460, h.det.baseline_cm());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_calibrates_to_far_board);
    RUN_TEST(test_no_detection_while_calibrating);
    RUN_TEST(test_detects_rock_with_leading_edge_time);
    RUN_TEST(test_ignores_single_sample_spike);
    RUN_TEST(test_ignores_adjacent_sheet);
    RUN_TEST(test_narrow_sheet_uses_baseline);
    RUN_TEST(test_brief_dropout_inside_rock_is_one_crossing);
    RUN_TEST(test_two_rocks_are_two_crossings);
    RUN_TEST(test_unreliable_samples_count_as_clear);
    RUN_TEST(test_parked_object_times_out_and_becomes_baseline);
    RUN_TEST(test_baseline_tracks_slow_drift_but_not_rocks);
    RUN_TEST(test_noisy_calibration_uses_median);
    return UNITY_END();
}
