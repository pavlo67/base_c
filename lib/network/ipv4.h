#pragma once

#include <string>
#include <string_view>
#include <vector>

bool isIPv4Address(std::string_view address);
bool loadLocalIPv4Addresses(std::vector<std::string>& addresses);
