#include "cpp.h"

#include <map>

std::string escapeCpp(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

bool fieldType(const std::string& jsonType, size_t& size) {
    static const std::map<std::string, size_t> types = {
        {"uint8", 1},
        {"uint16", 2},
        {"uint32", 4},
        {"uint64", 8},
        {"int8", 1},
        {"int16", 2},
        {"int32", 4},
        {"int64", 8},
        {"float32", 4},
        {"float64", 8},
        {"char8byte",   8},
        {"char16byte",  16},
        {"char24byte",  24},
        {"char32byte",  32},
        {"char40byte",  40},
        {"char48byte",  48},
        {"char56byte",  56},
        {"char64byte",  64},
        {"char96byte",  96},
        {"char128byte", 128},
        {"char160byte", 160},
        {"char192byte", 192},
        {"char224byte", 224},
        {"char240byte", 240}
    };
    const auto it = types.find(jsonType);
    if (it == types.end()) return false;
    size = it->second;
    return true;
}

