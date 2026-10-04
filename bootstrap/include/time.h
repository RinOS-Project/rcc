#ifndef RCC_BOOTSTRAP_TIME_H
#define RCC_BOOTSTRAP_TIME_H

/* Minimal host-compatible time ABI used by the bootstrap preprocessor. */
typedef long time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t* timer);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);

#endif
