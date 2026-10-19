#include <unity.h>

#include <vector>

#include "tfluna.h"

using namespace curling::tfluna;

void setUp() {}
void tearDown() {}

// Builds a valid data frame for the given raw field values.
static std::vector<uint8_t> frame(uint16_t dist, uint16_t amp, uint16_t raw_temp) {
    std::vector<uint8_t> f = {0x59, 0x59,
                              static_cast<uint8_t>(dist), static_cast<uint8_t>(dist >> 8),
                              static_cast<uint8_t>(amp), static_cast<uint8_t>(amp >> 8),
                              static_cast<uint8_t>(raw_temp), static_cast<uint8_t>(raw_temp >> 8)};
    uint8_t sum = 0;
    for (uint8_t b : f) sum = static_cast<uint8_t>(sum + b);
    f.push_back(sum);
    return f;
}

static int feed_all(FrameParser& p, const std::vector<uint8_t>& bytes) {
    int complete = 0;
    for (uint8_t b : bytes) complete += p.feed(b) ? 1 : 0;
    return complete;
}

void test_parses_frame() {
    FrameParser p;
    // 452 cm, amplitude 1200, raw temp 2280: 2280 / 8 - 256 = 29.0 °C
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, frame(452, 1200, 2280)));
    TEST_ASSERT_EQUAL_UINT16(452, p.reading().distance_cm);
    TEST_ASSERT_EQUAL_UINT16(1200, p.reading().amplitude);
    TEST_ASSERT_EQUAL_INT16(2900, p.reading().temp_centi_c);
    TEST_ASSERT_EQUAL_UINT32(1, p.frames());
    TEST_ASSERT_TRUE(is_reliable(p.reading()));
}

void test_resyncs_after_garbage() {
    FrameParser p;
    std::vector<uint8_t> bytes = {0x00, 0x59, 0x12, 0xFF};
    auto f = frame(300, 500, 2048);
    bytes.insert(bytes.end(), f.begin(), f.end());
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes));
    TEST_ASSERT_EQUAL_UINT16(300, p.reading().distance_cm);
    TEST_ASSERT_EQUAL_INT16(0, p.reading().temp_centi_c);
}

void test_counts_checksum_errors_and_recovers() {
    FrameParser p;
    auto bad = frame(100, 500, 2048);
    bad[8] ^= 0xFF;
    auto good = frame(200, 500, 2048);
    std::vector<uint8_t> bytes = bad;
    bytes.insert(bytes.end(), good.begin(), good.end());
    TEST_ASSERT_EQUAL_INT(1, feed_all(p, bytes));
    TEST_ASSERT_EQUAL_UINT32(1, p.checksum_errors());
    TEST_ASSERT_EQUAL_UINT16(200, p.reading().distance_cm);
}

void test_stream_of_frames() {
    FrameParser p;
    std::vector<uint8_t> bytes;
    for (uint16_t d = 0; d < 250; ++d) {
        auto f = frame(d, 1000, 2048);
        bytes.insert(bytes.end(), f.begin(), f.end());
    }
    TEST_ASSERT_EQUAL_INT(250, feed_all(p, bytes));
    TEST_ASSERT_EQUAL_UINT32(0, p.checksum_errors());
}

void test_reliability() {
    TEST_ASSERT_FALSE(is_reliable({450, 99, 0}));
    TEST_ASSERT_TRUE(is_reliable({450, 100, 0}));
    TEST_ASSERT_FALSE(is_reliable({450, 0xFFFF, 0}));
}

void test_set_frame_rate_command() {
    uint8_t out[8];
    TEST_ASSERT_EQUAL_size_t(6, build_set_frame_rate(250, out, sizeof(out)));
    const uint8_t expected[] = {0x5A, 0x06, 0x03, 0xFA, 0x00, 0x5D};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 6);

    TEST_ASSERT_EQUAL_size_t(6, build_set_frame_rate(100, out, sizeof(out)));
    const uint8_t expected100[] = {0x5A, 0x06, 0x03, 0x64, 0x00, 0xC7};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected100, out, 6);
}

void test_set_frame_rate_rejects_bad_input() {
    uint8_t out[8];
    TEST_ASSERT_EQUAL_size_t(0, build_set_frame_rate(300, out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0, build_set_frame_rate(600, out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0, build_set_frame_rate(250, out, 5));
}

void test_save_settings_command() {
    uint8_t out[4];
    TEST_ASSERT_EQUAL_size_t(4, build_save_settings(out, sizeof(out)));
    const uint8_t expected[] = {0x5A, 0x04, 0x11, 0x6F};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 4);
}

void test_get_version_command() {
    uint8_t out[4];
    TEST_ASSERT_EQUAL_size_t(4, build_get_version(out, sizeof(out)));
    const uint8_t expected[] = {0x5A, 0x04, 0x01, 0x5F};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 4);
}

void test_output_enable_commands() {
    uint8_t out[5];
    TEST_ASSERT_EQUAL_size_t(5, build_set_output_enabled(true, out, sizeof(out)));
    const uint8_t on[] = {0x5A, 0x05, 0x07, 0x01, 0x67};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(on, out, 5);
    TEST_ASSERT_EQUAL_size_t(5, build_set_output_enabled(false, out, sizeof(out)));
    const uint8_t off[] = {0x5A, 0x05, 0x07, 0x00, 0x66};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(off, out, 5);
    TEST_ASSERT_EQUAL_size_t(0, build_set_output_enabled(true, out, 4));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parses_frame);
    RUN_TEST(test_resyncs_after_garbage);
    RUN_TEST(test_counts_checksum_errors_and_recovers);
    RUN_TEST(test_stream_of_frames);
    RUN_TEST(test_reliability);
    RUN_TEST(test_set_frame_rate_command);
    RUN_TEST(test_set_frame_rate_rejects_bad_input);
    RUN_TEST(test_save_settings_command);
    RUN_TEST(test_get_version_command);
    RUN_TEST(test_output_enable_commands);
    return UNITY_END();
}
