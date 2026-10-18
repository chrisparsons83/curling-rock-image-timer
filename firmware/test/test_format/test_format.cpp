#include <unity.h>

#include <cstring>

#include "split_format.h"

using curling::format_split;

void setUp() {}
void tearDown() {}

static void expect_split(uint32_t micros, const char* expected) {
    char buf[16];
    TEST_ASSERT_EQUAL_size_t(std::strlen(expected), format_split(micros, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_rounds_to_nearest_hundredth() {
    expect_split(3'705'000, "3.71");
    expect_split(3'704'999, "3.70");
    expect_split(14'200'000, "14.20");
}

void test_zero_and_small() {
    expect_split(0, "0.00");
    expect_split(4'999, "0.00");
    expect_split(5'000, "0.01");
}

void test_buffer_too_small() {
    char buf[4];
    TEST_ASSERT_EQUAL_size_t(0, format_split(14'200'000, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rounds_to_nearest_hundredth);
    RUN_TEST(test_zero_and_small);
    RUN_TEST(test_buffer_too_small);
    return UNITY_END();
}
