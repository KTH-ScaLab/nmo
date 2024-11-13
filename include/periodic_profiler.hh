#ifndef PERIODIC_PROFILER_H
#define PERIODIC_PROFILER_H

#include <atomic>
#include <string>
#include "monitor.hh"

enum perp_mode {
    PERP_OFF = 0,
    PERP_ROOFLINE,
    PERP_NONE,
    PERP_PREFETCH,
};

class PeriodicProfiler
{
public:
    PeriodicProfiler(const char *profile_name, int sample_period, perp_mode mode,
        int ringbufsize, int auxbufsize);
    ~PeriodicProfiler();
    void kernel_start(const char *tag);
    void kernel_stop();

    Monitor _mon;
private:
    std::atomic_bool _stop_thread;
    std::string _tag;
    std::thread _thread;

    void run_thread();
};



#endif
