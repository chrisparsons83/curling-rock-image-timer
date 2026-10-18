#include "split_format.h"

#include <cstdio>

namespace curling {

size_t format_split(uint32_t micros, char* buf, size_t buf_len) {
    const uint32_t hundredths = (micros + 5000) / 10000;
    const int n = std::snprintf(buf, buf_len, "%lu.%02lu",
                                static_cast<unsigned long>(hundredths / 100),
                                static_cast<unsigned long>(hundredths % 100));
    if (n < 0 || static_cast<size_t>(n) >= buf_len) {
        if (buf_len > 0) buf[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(n);
}

}  // namespace curling
