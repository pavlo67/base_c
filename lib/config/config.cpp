#include "config.h"

const std::string ON_CONFIG = "[Config::Config()]";
Config::Config(const std::string& filePath) {
    try {
        root_ = YAML::LoadFile(filePath);
        loaded_ = true;
    } catch (const std::exception& e) {
        printf("%s ERROR: %s: %s\n", ON_CONFIG.c_str(), filePath.c_str(), e.what());
        root_ = {};
        loaded_ = false;
    }
}


