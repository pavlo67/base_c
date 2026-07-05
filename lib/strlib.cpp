#include <unistd.h>

#include "strlib.h"
#include <cstdarg>
#include <cstdio>

std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> res;
    while (!s.empty()) {
        // TODO!!! str.find_first_not_of(delimiter);

        size_t pos = s.find(delimiter);
        // printf("%lu / '%s' / %s\n", pos, delimiter.c_str(), s.c_str());
        if (pos >= s.length() || pos == std::string::npos) {
            res.push_back(s);
            break;
        }

        res.push_back(s.substr(0,pos));
        s = s.substr(pos+1);
    }
    return res;
}


void trim(std::string& str) {
    size_t first = str.find_first_not_of(WHITESPACE);
    if (std::string::npos == first) {
        str.clear(); // String is all whitespace
        return;
    }
    str.erase(0, first);

    size_t last = str.find_last_not_of(WHITESPACE);
    if (std::string::npos != last) {
        str.erase(last + 1);
    }
}

std::string tail(std::string const& src, size_t const length) {
    if (length >= src.size()) { return src; }
    return src.substr(src.size() - length);
}


void logger(FILE* fLog, const char* format, ...) {
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args); // Clean up

    if (fLog) {
        va_start(args, format);
        vfprintf(fLog, format, args);
        va_end(args); // Clean up
    }
}