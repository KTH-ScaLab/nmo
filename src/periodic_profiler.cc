#include <string.h>
#include "periodic_profiler.hh"
#include "clock.hh"

// Use output of tests/list_events to find event names on current architecture
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

static_assert(NUM_EVENTS == sizeof(EVENT_NAME)/sizeof(char *));
static_assert(NUM_EVENTS == sizeof(EVENT_LEADER)/sizeof(bool));

const event_spec null_spec    = {0, NULL, NULL};

const event_spec rl_counters = {NUM_EVENTS, EVENT_NAME, EVENT_LEADER};
const event_spec fp_counters = {NUM_EVENTS-2, EVENT_NAME+2, EVENT_LEADER+2};

//const event_spec mem_samplers = {2, EVENT_NAME, EVENT_LEADER};
static const char *SAMPLE_EVENTS[] = {"MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM", "MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM"};
const event_spec mem_samplers = {2, SAMPLE_EVENTS, EVENT_LEADER};

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

PeriodicProfiler::PeriodicProfiler(const char *profile_name, int sample_period, perp_mode mode)
    : _mon(get_counters(sample_period, mode), get_samplers(sample_period, mode), sample_period, profile_name)
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
