#ifndef BASE_CONFIG_H
#define BASE_CONFIG_H

#include "yaml-cpp/yaml.h"

#include <string>
#include <atomic>
#include <memory>

class Config {
public:
    Config() = default;
    explicit Config(const std::string& filePath);
    explicit Config(YAML::Node root);

    YAML::Node get(const std::string& name) const {
        return root_[name];
    }

    bool loadedOk() const {
        return loaded_;
    }

private:
    YAML::Node root_;
    bool loaded_ = false;
};

// Publish immutable snapshots; readers retain their snapshot while loading a section.
using SharedConfig = std::atomic<std::shared_ptr<const Config>>;

#endif //BASE_CONFIG_H
