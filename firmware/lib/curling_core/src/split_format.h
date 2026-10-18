#pragma once

#include <cstddef>
#include <cstdint>

namespace curling {

// Formats a split given in microseconds as seconds with two decimals,
// rounded to the nearest hundredth, e.g. 3'705'000 -> "3.71".
// Writes at most buf_len bytes including the terminator and returns the
// number of characters written (excluding the terminator), or 0 if the
// buffer is too small.
size_t format_split(uint32_t micros, char* buf, size_t buf_len);

}  // namespace curling
