// Display bring-up for issue #36: drive a 64x32 HUB75 panel from the hog-unit
// ESP32-S3 while the TF-Luna and crossing detector keep running.
//
// The panel shows the time between the last two crossings in large digits
// (swipe a hand through the beam twice) and the live distance in small text.
// Every second the serial monitor prints the frame rate and the worst gap
// between LiDAR frames, which shows whether the panel's DMA refresh delays
// the sensor. Keys: 'b' relearns the baseline, '+'/'-' change brightness.

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <esp_timer.h>

#include "crossing_detector.h"
#include "split_format.h"
#include "tfluna.h"

namespace {

// Hog-unit pin map (see docs/bench/display.md). GPIO17/18 stay on the
// TF-Luna, and GPIO9-14 and 21 are left free for the nRF24 radio.
constexpr int kLidarRxPin = 18;
constexpr int kLidarTxPin = 17;
constexpr uint32_t kLidarBaud = 115200;

constexpr int kR1 = 4, kG1 = 5, kB1 = 6;
constexpr int kR2 = 7, kG2 = 15, kB2 = 16;
constexpr int kA = 1, kB = 2, kC = 42, kD = 41, kE = -1;  // 1/16 scan: no E line
constexpr int kClk = 40, kLat = 39, kOe = 38;
constexpr int kPanelWidth = 64;
constexpr int kPanelHeight = 32;

HardwareSerial& lidar = Serial1;
curling::tfluna::FrameParser parser;
curling::CrossingDetector detector;
MatrixPanel_I2S_DMA* panel = nullptr;
uint8_t brightness = 40;  // of 255; keeps current low on a USB supply

int64_t last_start_us = 0;
uint32_t last_split_us = 0;
bool have_split = false;
bool redraw = true;
uint16_t shown_distance_cm = 0;

uint32_t window_frames = 0;
int64_t window_max_gap_us = 0;
int64_t window_start_us = 0;
int64_t last_frame_us = 0;

void configure_lidar() {
    uint8_t cmd[8];
    size_t len = curling::tfluna::build_set_output_enabled(true, cmd, sizeof(cmd));
    if (len > 0) lidar.write(cmd, len);
    delay(20);
    len = curling::tfluna::build_set_frame_rate(250, cmd, sizeof(cmd));
    if (len > 0) lidar.write(cmd, len);
}

void draw() {
    panel->clearScreen();
    panel->setTextWrap(false);

    // Top line: live distance, or the baseline state.
    panel->setTextSize(1);
    panel->setCursor(1, 1);
    panel->setTextColor(panel->color565(80, 80, 255));
    if (!detector.calibrated()) {
        panel->print("learning");
    } else {
        panel->printf("%u cm", shown_distance_cm);
    }

    // Big digits: the latest split, or a dash before there is one.
    char text[12] = "-.--";
    if (have_split && curling::format_split(last_split_us, text, sizeof(text)) == 0) {
        snprintf(text, sizeof(text), "--");
    }
    panel->setTextSize(2);
    const int width = static_cast<int>(strlen(text)) * 12 - 2;
    panel->setCursor((kPanelWidth - width) / 2, 14);
    panel->setTextColor(detector.active() ? panel->color565(255, 160, 0) : panel->color565(255, 255, 255));
    panel->print(text);
}

void on_frame(const curling::tfluna::Reading& r, int64_t t_us) {
    if (last_frame_us != 0 && t_us - last_frame_us > window_max_gap_us) {
        window_max_gap_us = t_us - last_frame_us;
    }
    last_frame_us = t_us;
    ++window_frames;

    const bool was_calibrated = detector.calibrated();
    const auto event = detector.feed(t_us, r.distance_cm, curling::tfluna::is_reliable(r));
    if (!was_calibrated && detector.calibrated()) {
        Serial.printf("# baseline=%u cm  threshold=%u cm\n", detector.baseline_cm(), detector.threshold_cm());
        redraw = true;
    }
    if (event == curling::CrossingDetector::Event::kStarted) {
        const int64_t start_us = detector.crossing().start_us;
        if (last_start_us != 0) {
            last_split_us = static_cast<uint32_t>(start_us - last_start_us);
            have_split = true;
            Serial.printf("# split %.3f s\n", last_split_us / 1e6);
        }
        last_start_us = start_us;
        redraw = true;
    } else if (event == curling::CrossingDetector::Event::kEnded) {
        redraw = true;
    }
}

void handle_console() {
    while (Serial.available()) {
        switch (Serial.read()) {
            case 'b':
                detector.recalibrate();
                Serial.println("# relearning baseline, keep the beam clear");
                redraw = true;
                break;
            case '+':
                brightness = brightness > 235 ? 255 : brightness + 20;
                panel->setBrightness8(brightness);
                Serial.printf("# brightness=%u\n", brightness);
                break;
            case '-':
                brightness = brightness < 20 ? 0 : brightness - 20;
                panel->setBrightness8(brightness);
                Serial.printf("# brightness=%u\n", brightness);
                break;
        }
    }
}

}  // namespace

void setup() {
    Serial.setTxBufferSize(2048);
    Serial.begin(921600);
    lidar.begin(kLidarBaud, SERIAL_8N1, kLidarRxPin, kLidarTxPin);
    lidar.setRxFIFOFull(9);
    lidar.setRxTimeout(1);

    HUB75_I2S_CFG::i2s_pins pins = {kR1, kG1, kB1, kR2, kG2, kB2, kA, kB, kC, kD, kE, kLat, kOe, kClk};
    HUB75_I2S_CFG config(kPanelWidth, kPanelHeight, 1, pins);
    panel = new MatrixPanel_I2S_DMA(config);
    if (!panel->begin()) {
        Serial.println("# panel failed to start");
    }
    panel->setBrightness8(brightness);

    delay(500);
    configure_lidar();
    Serial.println("# display test: keep the beam clear while the baseline is learned. Keys: b relearn, +/- brightness.");
    window_start_us = esp_timer_get_time();
}

void loop() {
    handle_console();

    while (lidar.available()) {
        if (parser.feed(static_cast<uint8_t>(lidar.read()))) on_frame(parser.reading(), esp_timer_get_time());
    }

    const int64_t now_us = esp_timer_get_time();
    if (now_us - window_start_us >= 1000000) {
        Serial.printf("# rate=%.1f Hz  max_gap=%.1f ms  dist=%u cm  checksum_errors=%lu\n",
                      window_frames / ((now_us - window_start_us) / 1e6), window_max_gap_us / 1000.0,
                      parser.reading().distance_cm, static_cast<unsigned long>(parser.checksum_errors()));
        window_frames = 0;
        window_max_gap_us = 0;
        window_start_us = now_us;
        // Refresh the live distance once a second rather than on every frame,
        // so drawing never competes with reading the sensor.
        if (parser.reading().distance_cm != shown_distance_cm) {
            shown_distance_cm = parser.reading().distance_cm;
            redraw = true;
        }
    }

    if (redraw) {
        redraw = false;
        draw();
    }
}
