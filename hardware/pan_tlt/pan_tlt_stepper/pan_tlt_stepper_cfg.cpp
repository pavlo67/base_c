#include "pan_tlt_stepper.h"

#include "_base_defines.h"

#include <cstdio>

void PanTltStepper::load(const Json::Value& jv, Info& info) {
    if (!jv.isObject()) {
        info.setError("PanTltStepper config must be an object");
        return;
    }
    pan_ = jv.get("pan", 0).asInt();
    tlt_ = jv.get("tilt", 0).asInt();
    info.setOk();
}

void PanTltStepper::save(Json::Value& jv) {
    jv = Json::Value(Json::objectValue);
    jv["pan"] = pan_;
    jv["tilt"] = tlt_;
}

void PanTltStepper::show() {
    std::printf("PanTltStepper: initialized=%s, pan=%d, tilt=%d\n",
                initialized_ ? "true" : "false", pan_, tlt_);
}
