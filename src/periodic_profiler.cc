#include <string.h>
#include "periodic_profiler.hh"
#include "clock.hh"

// Use output of bin/list_events to find event names on current architecture

#if defined(__x86_64__)
static const char *EVENT_NAME[] = {
    "OFFCORE_RESPONSE_0:L3_MISS_LOCAL",
    "OFFCORE_RESPONSE_1:L3_MISS_MISS_REMOTE_HOP1_DRAM",
    //"OFFCORE_RESPONSE_0:DMND_DATA_RD",
    //"OFFCORE_RESPONSE_1:PF_L2_DATA_RD",
    //"MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM",
    //"MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM",

    "FP_ARITH_INST_RETIRED.SCALAR_SINGLE",
    "FP_ARITH_INST_RETIRED.SCALAR_DOUBLE",

    "FP_ARITH_INST_RETIRED.128B_PACKED_DOUBLE",
    "FP_ARITH_INST_RETIRED.128B_PACKED_SINGLE",
    "FP_ARITH_INST_RETIRED.256B_PACKED_DOUBLE",
    "FP_ARITH_INST_RETIRED.256B_PACKED_SINGLE",
    "FP_ARITH_INST_RETIRED.512B_PACKED_DOUBLE",
    "FP_ARITH_INST_RETIRED.512B_PACKED_SINGLE",

    //"CPU_CLK_UNHALTED",
    //"CYCLE_ACTIVITY:STALLS_L3_MISS",
};

// #FLOP_Count = ( 1 * ( FP_ARITH_INST_RETIRED.SCALAR_SINGLE + FP_ARITH_INST_RETIRED.SCALAR_DOUBLE ) + 2 * FP_ARITH_INST_RETIRED.128B_PACKED_DOUBLE + 4 * ( FP_ARITH_INST_RETIRED.128B_PACKED_SINGLE + FP_ARITH_INST_RETIRED.256B_PACKED_DOUBLE ) + 8 * ( FP_ARITH_INST_RETIRED.256B_PACKED_SINGLE + FP_ARITH_INST_RETIRED.512B_PACKED_DOUBLE ) + 16 * FP_ARITH_INST_RETIRED.512B_PACKED_SINGLE )

// Group leader (see man perf_event_open group_fd)
// This is to ensure counters are comparable, eg to compute ratio
const bool EVENT_LEADER[] = {
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
    true,
};

const int NUM_EVENTS = 10;
#elif defined(__aarch64__)
static const char *EVENT_NAME[] = {
    "REMOTE_ACCESS", // Access on a different socket
    "BUS_ACCESS_RD", // Read access on bus, from load/store
                     // unit to DynamIQ Shared Unit (DSU) 
    "INST_RETIRED",  // Instruction executed
    //"MEM_READ_ACCESS", // "MEM_ACCESS_RD" in ARM PMU manual,
                         // counts access in memory and all
                         // cache levels without prefetch
    //"LL_CACHE_MISS_RD", // Read miss in last level cache
};

const bool EVENT_LEADER[] = {
    true,
    true,
    true,
};

const int NUM_EVENTS = 3;
#else
#error unsupported architecture
#endif

static_assert(NUM_EVENTS == sizeof(EVENT_NAME)/sizeof(char *));
static_assert(NUM_EVENTS == sizeof(EVENT_LEADER)/sizeof(bool));

const event_spec null_spec = {0, NULL, NULL};

const event_spec rl_counters = {NUM_EVENTS, EVENT_NAME, EVENT_LEADER};

#if defined(__x86_64__)
static const char *SAMPLE_EVENTS[] = {"MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM", "MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM"};
const event_spec mem_samplers = {2, SAMPLE_EVENTS, EVENT_LEADER};
#endif
//Issues with ARM SPE:
// -Non-SPE events rely on PERF_SAMPLE_ADDR being present in a raw sample event, which has the
//  description in perf_event_open man page that "Records an address, if applicable". Unfortunately,
//  this field always returns 0 for regular hardware counters on ARM, so sampling of non-SPE events on
//  ARM is advised against. If both SPE and non-SPE events are sampled, it can cause mmap to fail for
//  the aux buffer required for SPE.
//
// -Can not do "ARM_SPE:LOAD" and then "ARM_SPE:STORE" separately. perf_event_open
//  fails with device busy
//
// -For sampled events, the info file should display the hardware counters full count of sampled events,
//  however ARM_SPE:LOADSTORE, etc will have zeros since the event is not backed by a real hardware counter
#if defined(__aarch64__)
static const char *SAMPLE_EVENTS[] = {"ARM_SPE:LOADSTORE"};
const event_spec mem_samplers = {1, SAMPLE_EVENTS, EVENT_LEADER};
#endif

static const char *PF_EVENTS[] = {
    "OFFCORE_RESPONSE_0:PF_L2_DATA_RD",
    "OFFCORE_RESPONSE_0:PF_L2_RFO",
    "L2_LINES_IN",
    "L2_LINES_OUT:USELESS_HWPF",
};
const event_spec pf_counters = {4, PF_EVENTS, EVENT_LEADER};

static event_spec get_counters(int sample_period, perp_mode mode)
{
    switch (mode) {
        case PERP_ROOFLINE: return rl_counters;
        case PERP_NONE:     return null_spec;
        case PERP_PREFETCH: return pf_counters;
        default:            return null_spec;
    }
}

static event_spec get_samplers(int sample_period, perp_mode mode)
{
    switch (mode) {
        case PERP_ROOFLINE: return sample_period ? mem_samplers : null_spec;
        default:            return null_spec;
    }
}

PeriodicProfiler::PeriodicProfiler(const char *profile_name, int sample_period, perp_mode mode,
    int ringbufsize, int auxbufsize)
    : _mon(get_counters(sample_period, mode), get_samplers(sample_period, mode), sample_period,
        ringbufsize, auxbufsize, profile_name)
    , _tag("init")
{

    _stop_thread = false;
    _thread = std::thread(&PeriodicProfiler::run_thread, this);
    
    _mon.start(_tag.data());
    _mon.set_nonstop();
}

PeriodicProfiler::~PeriodicProfiler()
{
    _stop_thread = true;
    _thread.join();
}

void PeriodicProfiler::run_thread()
{
    const auto period = std::chrono::seconds(1);

    auto start = std::chrono::steady_clock::now();
    auto next = start + period;

    while (!_stop_thread) {
        std::this_thread::sleep_until(next);

        _mon.stop();
        _mon.start(_tag.data());

        next += period;
    }

    _mon.stop();
}

void PeriodicProfiler::kernel_start(const char *tag)
{
    char pt[strlen(tag)+2];
    sprintf(pt, "+%s", tag);
    _mon.mark_phase(pt);

    _tag.assign(tag);
}

void PeriodicProfiler::kernel_stop()
{
    char pt[_tag.size()+2];
    sprintf(pt, "-%s", _tag.c_str());
    _mon.mark_phase(pt);

    _tag.assign("init");
}
