#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <string_view>

// Strict signed decimal/scientific notation; finite representable float only.
inline constexpr const char* ON_PARSE_FINITE_FLOAT = "[parseFiniteFloat()]";
inline bool parseFiniteFloat(std::string_view text, float& value) {
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
