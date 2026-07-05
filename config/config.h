#ifndef BASE_CPP_CONFIG_H
#define BASE_CPP_CONFIG_H

#include "lib/info.h"

#include <json/value.h>

class Config {
public:
    virtual ~Config() = default;

    virtual void load(const Json::Value& jv, Info& info) = 0;
    virtual void save(Json::Value& jv)                   = 0;
    virtual void show()                                  = 0;
};

#endif //BASE_CPP_CONFIG_H
