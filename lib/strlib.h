#ifndef STRLIB_H
#define STRLIB_H

#include <cstdint>
#include <vector>
#include <string>

const std::string WHITESPACE = " \n\r\t\f\v ";

std::vector<std::string> split(std::string s, const std::string& delimiter);
std::string tail(std::string const& src, size_t const length);
void trim(std::string& str);

void logger(FILE* fLog, const char* format, ...);

#endif //STRLIB_H
