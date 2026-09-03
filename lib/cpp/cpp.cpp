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

bool fieldType(const std::string& jsonType, std::string& cppType, size_t& size) {
    static const std::map<std::string, std::pair<std::string, size_t>> types = {
        {"uint8_t", {"UINT8", 1}}, {"uint16_t", {"UINT16", 2}}, {"uint32_t", {"UINT32", 4}}, {"uint64_t", {"UINT64", 8}},
        {"int8_t", {"INT8", 1}}, {"int16_t", {"INT16", 2}}, {"int32_t", {"INT32", 4}}, {"int64_t", {"INT64", 8}},
        {"float", {"FLOAT32", 4}}, {"double", {"FLOAT64", 8}},
        {"char[8]", {"CHAR8BYTE", 8}}, {"char[16]", {"CHAR16BYTE", 16}}, {"char[24]", {"CHAR24BYTE", 24}},
        {"char[32]", {"CHAR32BYTE", 32}}, {"char[40]", {"CHAR40BYTE", 40}}, {"char[48]", {"CHAR488YTE", 48}},
        {"char[56]", {"CHAR56BYTE", 56}}, {"char[64]", {"CHAR64BYTE", 64}}, {"char[96]", {"CHAR96BYTE", 96}},
        {"char[128]", {"CHAR128BYTE", 128}}, {"char[160]", {"CHAR160BYTE", 160}}, {"char[192]", {"CHAR192BYTE", 192}},
        {"char[224]", {"CHAR224BYTE", 224}},
    };
    const auto it = types.find(jsonType);
    if (it == types.end()) return false;
    cppType = it->second.first;
    size = it->second.second;
    return true;
}

