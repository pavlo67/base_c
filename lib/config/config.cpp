#include "config.h"

Config::Config(const std::string& filePath) {
    try {
        root_ = YAML::LoadFile(filePath);
        loaded_ = true;
    } catch (const std::exception& e) {
        printf("ERROR on Config::Config(%s): %s\n", filePath.c_str(), e.what());
        root_ = {};
        loaded_ = false;
    }
}


