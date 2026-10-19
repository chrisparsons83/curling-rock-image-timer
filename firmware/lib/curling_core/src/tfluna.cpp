#include "tfluna.h"

namespace curling {
namespace tfluna {

namespace {

constexpr uint8_t kFrameHeader = 0x59;
constexpr uint8_t kCommandHeader = 0x5A;
constexpr uint8_t kIdFrameRate = 0x03;
constexpr uint8_t kIdSaveSettings = 0x11;

uint8_t checksum(const uint8_t* bytes, size_t n) {
    uint8_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum = static_cast<uint8_t>(sum + bytes[i]);
    return sum;
}

}  // namespace

bool FrameParser::feed(uint8_t byte) {
    // Hunt for the two-byte header before collecting the rest of the frame.
    if (len_ < 2 && byte != kFrameHeader) {
        len_ = 0;
        return false;
    }
    buf_[len_++] = byte;
    if (len_ < sizeof(buf_)) return false;

    len_ = 0;
    if (checksum(buf_, 8) != buf_[8]) {
        ++checksum_errors_;
        return false;
    }
    reading_.distance_cm = static_cast<uint16_t>(buf_[2] | (buf_[3] << 8));
    reading_.amplitude = static_cast<uint16_t>(buf_[4] | (buf_[5] << 8));
    // Raw temperature is in units of 1/8 °C offset by 256 °C.
    const int32_t raw_temp = buf_[6] | (buf_[7] << 8);
    reading_.temp_centi_c = static_cast<int16_t>(raw_temp * 100 / 8 - 25600);
    ++frames_;
    return true;
}

size_t build_set_frame_rate(uint16_t hz, uint8_t* out, size_t out_len) {
    if (out_len < 6 || hz > 500 || (hz != 0 && 500 % hz != 0)) return 0;
    out[0] = kCommandHeader;
    out[1] = 6;
    out[2] = kIdFrameRate;
    out[3] = static_cast<uint8_t>(hz & 0xFF);
    out[4] = static_cast<uint8_t>(hz >> 8);
    out[5] = checksum(out, 5);
    return 6;
}

size_t build_save_settings(uint8_t* out, size_t out_len) {
    if (out_len < 4) return 0;
    out[0] = kCommandHeader;
    out[1] = 4;
    out[2] = kIdSaveSettings;
    out[3] = checksum(out, 3);
    return 4;
}

}  // namespace tfluna
}  // namespace curling
