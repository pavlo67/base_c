#include "parselib.h"

#include <charconv>

bool parseUnsigned(const std::string& text, uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    return result.ec == std::errc() && result.ptr == last;
}
