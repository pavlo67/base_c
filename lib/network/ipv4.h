#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

bool isIPv4Address(std::string_view address);
std::string ipV4HostToString(uint32_t address);
bool loadLocalIPv4Addresses(std::vector<std::string>& addresses);
