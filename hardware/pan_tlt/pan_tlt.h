#ifndef BASE_CPP_PAN_TILT_H
#define BASE_CPP_PAN_TILT_H

#include "lib/info.h"
#include <json/value.h>

class PanTlt {
public:
    virtual ~PanTlt() = default;

    virtual void zero() = 0;
    virtual void set (int x, int y, Info& info) = 0;
    virtual void move(int x, int y, Info& info) = 0;
    virtual void load(const Json::Value& jv, Info& info) = 0;
    virtual void save(Json::Value& jv) = 0;
    virtual void show() = 0;

    static void probe();
    static bool test();
};


#endif //BASE_CPP_PAN_TILT_H
