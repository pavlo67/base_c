#ifndef BASE_CONFIG_H
#define BASE_CONFIG_H

#include "yaml-cpp/yaml.h"

#include <string>

class Config {
public:
    explicit Config(const std::string& filePath);

    const YAML::Node& get(const std::string& name) const {
        return root_[name];
    }

    bool loadedOk() const {
        return loaded_;
    }

private:
    YAML::Node root_;
    bool loaded_ = false;
};


#endif //BASE_CONFIG_H
