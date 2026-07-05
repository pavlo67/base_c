#ifndef TIMING_H
#define TIMING_H

#include <cstdint>
#include <string>

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

// Timing -------------------------------------------------------------------------

typedef struct {
    uint32_t cnt;
    duration avg;
    duration min;
    duration max;
} timing_stat;

class Timing {
public:

    explicit    Timing(uint afterCnt = 0, uint resetEachCnt = 0);
    void        add(moment started_at);
    timing_stat get();
    void        show(const std::string& label, bool showFPS = false, FILE* fLog = nullptr);

private:
    uint     afterCnt_;
    uint     resetEachCnt_;
    uint32_t cnt_;
    duration sum_;
    duration min_;
    duration max_;
};



#endif //TIMING_H
