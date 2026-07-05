#include <ctime>

#include "time.h"

#include <chrono>

// !!! std::time_t now = std::time(nullptr)

std::string formatTimeCustom(std::time_t t) {
    char timeString[std::size("yyyy-mm-ddThh:mm:ss")];
    std::strftime(std::data(timeString), std::size(timeString),"%FT%T", std::localtime(&t));
    return timeString;
}

moment now() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

uint64_t nowMs() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

Timing::Timing(uint afterCnt, uint resetEachCnt) {
    afterCnt_     = afterCnt;
    resetEachCnt_ = resetEachCnt;
    cnt_    = 0;
    sum_    = 0;
    min_    = 0;
    max_    = 0;
};

void Timing::add(moment started_at) {
    duration once = now() - started_at;

    cnt_++;
    sum_ += once;

    // TODO!!! process resetEachCnt_

    if (cnt_ == afterCnt_) {
        afterCnt_ = 0;
        cnt_      = 0;
        sum_      = 0;
        min_      = 0;
        max_      = 0;

    } else if (cnt_ == 1) {
        min_ = max_ = once;

    } else if (once < min_) {
        min_ = once;

    } else if (once > max_) {
        max_ = once;

    }
}

timing_stat Timing::get() {
    return {cnt_, (cnt_ ? sum_ / duration(cnt_) : 0), min_, max_};
}

std::string SHOW_TITLE = "timing/%s: cnt = %6d, avg = %8llu us, max = %8llu us, min = %8llu us";

void Timing::show(const std::string& label, bool showFPS, FILE* fLog) {
    timing_stat s = get();
    std::string title = (SHOW_TITLE + (showFPS ? ", fps = " + std::to_string(double(SECOND) / double(s.avg)) : "")) + "\n";

    printf(title.c_str(), label.c_str(), s.cnt, s.avg / MICROSECOND, s.max / MICROSECOND, s.min / MICROSECOND);
    if (fLog) {
        fprintf(fLog, title.c_str(), label.c_str(), s.cnt, s.avg / MICROSECOND, s.max / MICROSECOND, s.min / MICROSECOND);
    }
}

