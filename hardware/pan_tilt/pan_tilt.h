#ifndef BASE_CPP_PAN_TILT_H
#define BASE_CPP_PAN_TILT_H

#include "lib/info.h"

#include "config/config.h"

class PanTilt : public Config {
public:

    void zero();
    void set (int x, int y, Info& info);
    void move(int x, int y, Info& info);

    void probe();
    bool test();
};


#endif //BASE_CPP_PAN_TILT_H
