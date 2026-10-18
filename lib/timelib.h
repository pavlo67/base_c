#ifndef TIMING_H
#define TIMING_H

#include <chrono>
#include <cstdint>
#include <string>

#include "defines.h"

// Shared monotonic application clock; independent of wall-clock corrections.
using Clock = std::chrono::steady_clock;

// base types & functions ---------------------------------------------------------

typedef uint64_t duration;
typedef uint64_t moment;

const duration NANOSECOND  = 1;
const duration MICROSECOND = 1e3;
const duration MILLISECOND = 1e6;
const duration SECOND      = 1e9;
const duration MINUTE      = SECOND * 60;

std::string formatTimeCustom(time_t t);
moment      now();
uint64_t    nowMs();
uint64_t    monotonicNowMs();
uint32_t    internal32Ms();

// Timing -------------------------------------------------------------------------

typedef struct {
    uint32_t cnt;
    duration avg;
    duration min;
    duration max;
} timing_stat;

#if TIMING_LOG_STAT_MAX > 0
    typedef struct {
        uint32_t frameN;
        duration value;
    } timing_stat_value;
#endif

class TimingStat {
public:

    explicit    TimingStat(uint afterCnt = 0);
    void        add(moment started_at, uint32_t frameN = 0);
    timing_stat get();
    void        show(const std::string& label, bool showFPS = false, bool shouldShowValues = true, FILE* fLog = nullptr);

private:
    uint     afterCnt_;
    uint32_t cnt_;
    duration sum_;
    duration min_;
    duration max_;
    #if TIMING_LOG_STAT_MAX > 0
        void              addMinValue(uint32_t frameN, duration value);
        void              addMaxValue(uint32_t frameN, duration value);
        void              showValues(const std::string& label, const char* title, const timing_stat_value* values, uint32_t valuesCnt, FILE* fLog);
        uint32_t          cntLogMax_;
        uint32_t          valuesMinCnt_;
        uint32_t          valuesMaxCnt_;
        timing_stat_value valuesMin_[TIMING_LOG_STAT_MAX];
        timing_stat_value valuesMax_[TIMING_LOG_STAT_MAX];
    #endif
};



#endif //TIMING_H
