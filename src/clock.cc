#include <time.h>
#include "clock.hh"
#include "nmo_exception.hh"

unsigned long nano_clock()
{
    timespec time, res;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &time) < 0)
        throw NmoException("failed clock_gettime");

    if (clock_getres(CLOCK_MONOTONIC_RAW, &res) < 0 || res.tv_sec)
        throw NmoException("clock_getres failure (CLOCK_MONOTONIC_RAW)");

    return time.tv_sec * res.tv_nsec*1000000000 + time.tv_nsec * res.tv_nsec;
}
