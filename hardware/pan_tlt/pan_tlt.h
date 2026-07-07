#ifndef BASE_CPP_PAN_TILT_H
#define BASE_CPP_PAN_TILT_H

#include "lib/info.h"

#include "config/config.h"

class PanTlt : public Config {
public:

    virtual void zero();
    virtual void set (int x, int y, Info& info);
    virtual void move(int x, int y, Info& info);

    static void probe();
    static bool test();
};


#endif //BASE_CPP_PAN_TILT_H
