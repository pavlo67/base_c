#ifndef BASE_CPP_PANTILTSTEPPER_H
#define BASE_CPP_PANTILTSTEPPER_H

#include "hardware/pan_tlt/pan_tlt.h"

class PanTltStepper : public PanTlt {
public:

    PanTltStepper();
    void zero() final;
    void set (int pan, int tlt, Info& info) final;
    void move(int pan, int tlt, Info& info) final;

    void load(const Json::Value& jv, Info& info) final;
    void save(Json::Value& jv)                   final;
    void show()                                  final;

private:
    int pan_ = 0;
    int tlt_ = 0;

};




#endif //BASE_CPP_PANTILTSTEPPER_H
