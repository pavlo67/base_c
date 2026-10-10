#ifndef BASE_CONFIG_SECTION_H
#define BASE_CONFIG_SECTION_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "config.h"

using config_values_t = std::map<std::string, std::string>;

class ComponentState {
public:
    virtual ~ComponentState() = default;

    virtual bool load(const Config& cfg) = 0;
    virtual config_values_t get() const = 0;
};

using config_sections_t = std::vector<std::pair<std::string, ComponentState*>>;

#endif // BASE_CONFIG_SECTION_H
