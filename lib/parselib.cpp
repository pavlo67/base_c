#include "parselib.h"

#include <charconv>
#include <cmath>

bool parseUnsigned(const std::string& text, uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    return result.ec == std::errc() && result.ptr == last;
}

// Strict signed decimal/scientific notation; finite representable float only.
constexpr const char* ON_PARSE_FINITE_FLOAT = "[parseFiniteFloat()]";
bool parseFiniteFloat(std::string_view text, float& value) {
    const bool explicitPlus = !text.empty() && text.front() == '+';
    if (explicitPlus) { text.remove_prefix(1); }
    float candidate = 0;
    if (text.empty() || text.front() == '+' || (explicitPlus && text.front() == '-')) {
        printf("%s ERROR: expected a signed number\n", ON_PARSE_FINITE_FLOAT);
        return false;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), candidate);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(candidate)) {
        printf("%s ERROR: expected a finite decimal number\n", ON_PARSE_FINITE_FLOAT);
        return false;
    }
    value = candidate;
    return true;
}
