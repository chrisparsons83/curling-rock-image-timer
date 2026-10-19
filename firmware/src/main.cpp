// Bench firmware for issue #14: stream TF-Luna readings over USB serial.
//
// Every second it prints a summary line (frame rate, latest reading, worst
// gap between frames, error counts). Send 'r' over the serial monitor to
// toggle raw CSV output of every frame, or 'c' to clear the counters.

#include <Arduino.h>
#include <esp_timer.h>

#include "tfluna.h"

namespace {

// UART pins wired to the TF-Luna. The sensor's TXD goes to kLidarRxPin.
#if CONFIG_IDF_TARGET_ESP32S3
constexpr int kLidarRxPin = 18;
constexpr int kLidarTxPin = 17;
#else  // ESP32-C3 Super Mini
constexpr int kLidarRxPin = 20;
constexpr int kLidarTxPin = 21;
#endif
constexpr uint32_t kLidarBaud = 115200;
constexpr uint16_t kFrameRateHz = 250;

HardwareSerial& lidar = Serial1;
curling::tfluna::FrameParser parser;

bool raw_mode = false;

// Counters for the current one-second window, plus totals since the last clear.
uint32_t window_frames = 0;
uint32_t window_bytes = 0;
uint32_t window_unreliable = 0;
int64_t window_max_gap_us = 0;
int64_t window_start_us = 0;
int64_t last_frame_us = 0;
uint32_t total_frames = 0;
uint32_t checksum_errors_at_clear = 0;
int64_t cleared_at_us = 0;

void send_command(const uint8_t* cmd, size_t len) {
    if (len > 0) lidar.write(cmd, len);
}

void configure_lidar() {
    uint8_t cmd[8];
    // Set on every boot rather than saved to the sensor's flash, so a fresh
    // sensor works without a separate setup step.
    send_command(cmd, curling::tfluna::build_set_frame_rate(kFrameRateHz, cmd, sizeof(cmd)));
    delay(50);
    while (lidar.available()) lidar.read();  // drop the command replies
}

void reset_window(int64_t now_us) {
    window_frames = 0;
    window_bytes = 0;
    window_unreliable = 0;
    window_max_gap_us = 0;
    window_start_us = now_us;
}

void clear_counters(int64_t now_us) {
    reset_window(now_us);
    last_frame_us = 0;
    total_frames = 0;
    checksum_errors_at_clear = parser.checksum_errors();
    cleared_at_us = now_us;
}

void handle_console() {
    while (Serial.available()) {
        switch (Serial.read()) {
            case 'r':
                raw_mode = !raw_mode;
                if (raw_mode) Serial.println("t_us,distance_cm,amplitude,temp_c");
                break;
            case 'c':
                clear_counters(esp_timer_get_time());
                Serial.println("# counters cleared");
                break;
        }
    }
}

void on_frame(const curling::tfluna::Reading& r, int64_t t_us) {
    if (last_frame_us != 0 && t_us - last_frame_us > window_max_gap_us) {
        window_max_gap_us = t_us - last_frame_us;
    }
    last_frame_us = t_us;
    ++window_frames;
    ++total_frames;
    if (!curling::tfluna::is_reliable(r)) ++window_unreliable;

    if (raw_mode) {
        Serial.printf("%lld,%u,%u,%.2f\n", static_cast<long long>(t_us), r.distance_cm,
                      r.amplitude, r.temp_centi_c / 100.0);
    }
}

// Ends the one-second window, printing its summary unless raw output is on.
void end_window(int64_t now_us) {
    if (raw_mode) {
        reset_window(now_us);
        return;
    }
    const auto& r = parser.reading();
    const double secs = (now_us - window_start_us) / 1e6;
    Serial.printf(
        "# bytes=%lu  rate=%.1f Hz  dist=%u cm  amp=%u  temp=%.1f C  max_gap=%.1f ms  unreliable=%lu  "
        "total=%lu  checksum_errors=%lu  since_clear=%.0f s\n",
        static_cast<unsigned long>(window_bytes), window_frames / secs, r.distance_cm, r.amplitude, r.temp_centi_c / 100.0,
        window_max_gap_us / 1000.0, static_cast<unsigned long>(window_unreliable),
        static_cast<unsigned long>(total_frames),
        static_cast<unsigned long>(parser.checksum_errors() - checksum_errors_at_clear),
        (now_us - cleared_at_us) / 1e6);
    reset_window(now_us);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    lidar.begin(kLidarBaud, SERIAL_8N1, kLidarRxPin, kLidarTxPin);
    // Hand bytes to the driver as soon as a frame's worth arrives, or after a
    // one-symbol pause, instead of waiting for the FIFO to fill. This keeps
    // the arrival timestamps close to when each frame finished.
    lidar.setRxFIFOFull(9);
    lidar.setRxTimeout(1);
    delay(500);
    configure_lidar();
    clear_counters(esp_timer_get_time());
    Serial.printf("# TF-Luna bench: RX=GPIO%d TX=GPIO%d, %u Hz. 'r' toggles raw CSV, 'c' clears counters.\n",
                  kLidarRxPin, kLidarTxPin, kFrameRateHz);
}

void loop() {
    handle_console();

    while (lidar.available()) {
        const uint8_t byte = static_cast<uint8_t>(lidar.read());
        ++window_bytes;
        if (parser.feed(byte)) on_frame(parser.reading(), esp_timer_get_time());
    }

    const int64_t now_us = esp_timer_get_time();
    if (now_us - window_start_us >= 1000000) end_window(now_us);
}
