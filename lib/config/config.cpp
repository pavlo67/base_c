#include "config.h"

#include <utility>

Config::Config(const std::string& filePath) {
    try {
        root_ = YAML::LoadFile(filePath);
        loaded_ = true;
    } catch (const std::exception& e) {
        printf("[Config::Config()] ERROR: %s: %s\n", filePath.c_str(), e.what());
        root_ = {};
        loaded_ = false;
    }
}

Config::Config(YAML::Node root) : root_(std::move(root)), loaded_(root_.IsDefined()) {}


