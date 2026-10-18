#include <ctime>

#include "timelib.h"

std::string formatTimeCustom(std::time_t t) {
    char timeString[std::size("yyyy-mm-ddThh:mm:ss")];
    std::strftime(std::data(timeString), std::size(timeString),"%FT%T", std::localtime(&t));
    return timeString;
}

static const uint64_t startedAtMs = monotonicNowMs();

moment now() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) [[unlikely]] {
        return 0;
    }
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

uint64_t nowMs() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) [[unlikely]] {
        return 0;
    }
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

uint64_t monotonicNowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()).count());
}

uint32_t internal32Ms() {
    return monotonicNowMs() - startedAtMs;
}

TimingStat::TimingStat(uint afterCnt) {
    afterCnt_ = afterCnt;
    cnt_      = 0;
    sum_      = 0;
    min_      = 0;
    max_      = 0;
    #if TIMING_LOG_STAT_MAX > 0
        cntLogMax_    = TIMING_LOG_STAT_MAX;
        valuesMinCnt_ = 0;
        valuesMaxCnt_ = 0;
    #endif
};

void TimingStat::add(moment started_at, uint32_t frameN) {
    duration once = now() - started_at;

    cnt_++;
    sum_ += once;

    #if TIMING_LOG_STAT_MAX > 0
        addMinValue(frameN, once);
        addMaxValue(frameN, once);
    #endif

    if (cnt_ == afterCnt_) {
        afterCnt_ = 0;
        cnt_      = 0;
        sum_      = 0;
        min_      = 0;
        max_      = 0;
        #if TIMING_LOG_STAT_MAX > 0
            valuesMinCnt_ = 0;
            valuesMaxCnt_ = 0;
        #endif

    } else if (cnt_ == 1) {
        min_ = max_ = once;

    } else if (once < min_) {
        min_ = once;

    } else if (once > max_) {
        max_ = once;

    }
}

timing_stat TimingStat::get() {
    return {cnt_, (cnt_ ? sum_ / duration(cnt_) : 0), min_, max_};
}

std::string SHOW_TITLE = "timing/%s: cnt = %6d, avg = %8llu us, max = %8llu us, min = %8llu us";

void TimingStat::show(const std::string& label, bool showFPS, bool shouldShowValues, FILE* fLog) {
    timing_stat s = get();
    std::string title = (SHOW_TITLE + (showFPS ? ", fps = " + std::to_string(double(SECOND) / double(s.avg)) : "")) + "\n";

    printf(title.c_str(), label.c_str(), s.cnt, s.avg / MICROSECOND, s.max / MICROSECOND, s.min / MICROSECOND);
    if (fLog) {
        fprintf(fLog, title.c_str(), label.c_str(), s.cnt, s.avg / MICROSECOND, s.max / MICROSECOND, s.min / MICROSECOND);
    }
    #if TIMING_LOG_STAT_MAX > 0
        if (shouldShowValues) {
            showValues(label, "minimum values", valuesMin_, valuesMinCnt_, fLog);
            showValues(label, "maximum values", valuesMax_, valuesMaxCnt_, fLog);
        }
    #endif
}

#if TIMING_LOG_STAT_MAX > 0
    void TimingStat::addMinValue(uint32_t frameN, duration value) {
        uint32_t idx = 0;
        while (idx < valuesMinCnt_ && valuesMin_[idx].value <= value) {
            idx++;
        }
        if (idx == cntLogMax_) {
            return;
        }
        const uint32_t valuesCnt = valuesMinCnt_ < cntLogMax_ ? valuesMinCnt_ : cntLogMax_ - 1;
        for (uint32_t i = valuesCnt; i > idx; i--) {
            valuesMin_[i] = valuesMin_[i - 1];
        }
        valuesMin_[idx] = {frameN, value};
        if (valuesMinCnt_ < cntLogMax_) {
            valuesMinCnt_++;
        }
    }

    void TimingStat::addMaxValue(uint32_t frameN, duration value) {
        uint32_t idx = 0;
        while (idx < valuesMaxCnt_ && valuesMax_[idx].value >= value) {
            idx++;
        }
        if (idx == cntLogMax_) {
            return;
        }
        const uint32_t valuesCnt = valuesMaxCnt_ < cntLogMax_ ? valuesMaxCnt_ : cntLogMax_ - 1;
        for (uint32_t i = valuesCnt; i > idx; i--) {
            valuesMax_[i] = valuesMax_[i - 1];
        }
        valuesMax_[idx] = {frameN, value};
        if (valuesMaxCnt_ < cntLogMax_) {
            valuesMaxCnt_++;
        }
    }

    void TimingStat::showValues(const std::string& label, const char* title, const timing_stat_value* values, uint32_t valuesCnt, FILE* fLog) {
        printf("timing/%s: %s\n", label.c_str(), title);
        if (fLog) {
            fprintf(fLog, "timing/%s: %s\n", label.c_str(), title);
        }
        for (uint32_t i = 0; i < valuesCnt; i++) {
            printf("  #%u: %llu us\n", values[i].frameN, static_cast<unsigned long long>(values[i].value / MICROSECOND));
            if (fLog) {
                fprintf(fLog, "  #%u: %llu us\n", values[i].frameN, static_cast<unsigned long long>(values[i].value / MICROSECOND));
            }
        }
    }
#endif

