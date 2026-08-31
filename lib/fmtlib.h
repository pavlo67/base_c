#ifndef BASE_FMTLIB_H
#define BASE_FMTLIB_H

#include <type_traits>

template <typename T>
requires std::is_enum_v<T>
constexpr auto enum2int(T e) {
    return static_cast<std::underlying_type_t<T>>(e);
}

#endif //BASE_FMTLIB_H
