#ifndef BASE_CPP_H
#define BASE_CPP_H

#include <string>

std::string escapeCpp(const std::string& value);
bool fieldType(const std::string& jsonType, std::string& cppType, size_t& size);

#endif // BASE_CPP_H
