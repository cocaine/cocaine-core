#ifndef YAPPI_COMMON_HPP
#define YAPPI_COMMON_HPP

#include <time.h>
#include <syslog.h>

#define clock_advance(tsa, tsb)                 \
    do {                                        \
        tsa.tv_sec += tsb.tv_sec;               \
        tsa.tv_nsec += tsb.tv_nsec;             \
        if (tsa.tv_nsec >= 1e+9) {              \
            tsa.tv_sec++;                       \
            tsa.tv_nsec -= 1e+9;                \
        }                                       \
    } while(0);

#define clock_parse(interval, ts)               \
    do {                                        \
        ts.tv_sec = interval / 1000;            \
        ts.tv_nsec = (interval % 1000) * 1e+6;  \
    } while(0);

#endif
