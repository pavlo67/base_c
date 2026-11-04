#ifndef STRLIB_H
#define STRLIB_H

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include <string>
#include <string_view>

const std::string WHITESPACE = " \n\r\t\f\v ";

std::vector<std::string> split(std::string s, const std::string& delimiter);
std::string              tail(std::string const& src, size_t const length);
void                     trim(std::string& str);


// using cv lib
// inline void              toLowerCase(std::string& str) {
//     std::transform(
//                    str.begin(), str.end(), str.begin(),
//                    [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
// }
// inline void              toUpperCase(std::string& str) {
//     std::transform(
//                    str.begin(), str.end(), str.begin(),
//                    [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
// }


std::string stringMapToMultilineString(const std::map<std::string, std::string>& values);








#endif //STRLIB_H
