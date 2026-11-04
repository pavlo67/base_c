#ifndef PARSELIB_H
#define PARSELIB_H

#include <cstdint>
#include <string>

bool parseUnsigned(const std::string& text, uint64_t& value);
bool parseFiniteFloat(std::string_view text, float& value);



#endif // PARSELIB_H
