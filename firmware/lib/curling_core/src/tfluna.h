#pragma once

#include <cstddef>
#include <cstdint>

namespace curling {
namespace tfluna {

// One measurement from a TF-Luna in UART mode.
struct Reading {
    uint16_t distance_cm;
    uint16_t amplitude;  // signal strength; below kMinAmplitude the distance is unreliable
    int16_t temp_centi_c;  // chip temperature in hundredths of a degree C
};

// The datasheet says distances are unreliable when amplitude is below 100
// or saturated at 0xFFFF.
constexpr uint16_t kMinAmplitude = 100;
constexpr uint16_t kSaturatedAmplitude = 0xFFFF;

inline bool is_reliable(const Reading& r) {
    return r.amplitude >= kMinAmplitude && r.amplitude != kSaturatedAmplitude;
}

// Incremental parser for the 9-byte data frame:
//   0x59 0x59 Dist_L Dist_H Amp_L Amp_H Temp_L Temp_H Checksum
// where Checksum is the low byte of the sum of the first 8 bytes.
class FrameParser {
public:
    // Feeds one byte. Returns true when it completes a valid frame, which is
    // then available from reading().
    bool feed(uint8_t byte);

    const Reading& reading() const { return reading_; }

    uint32_t frames() const { return frames_; }
    uint32_t checksum_errors() const { return checksum_errors_; }

private:
    uint8_t buf_[9] = {};
    size_t len_ = 0;
    Reading reading_ = {};
    uint32_t frames_ = 0;
    uint32_t checksum_errors_ = 0;
};

// Command builders. Each writes a complete command (header, length, id,
// payload, checksum) into out and returns its length, or 0 if out is too
// small or the argument is invalid.

// Output frame rate in Hz. The sensor supports 500/n Hz for integer n, so
// valid rates include 1, 10, 100, 125, 250. 0 switches to trigger mode.
size_t build_set_frame_rate(uint16_t hz, uint8_t* out, size_t out_len);

// Stores the current settings in the sensor's flash.
size_t build_save_settings(uint8_t* out, size_t out_len);

}  // namespace tfluna
}  // namespace curling
