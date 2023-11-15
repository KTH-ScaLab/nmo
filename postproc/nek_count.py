import numpy as np
import sys
import prof_reader

prof = [prof_reader.read(base) for base in sys.argv[1:]]
ncall = prof[0].num_calls

local_bytes = np.zeros(ncall)
remote_bytes = np.zeros(ncall)
fp = np.zeros(ncall)
dur = np.zeros(ncall)


for i in range(ncall):
    dur[i] = prof[0].clock[i][1] - prof[0].clock[i][0]

    local_bytes[i] = sum(64 * p.counter["OFFCORE_RESPONSE_0:L3_MISS_LOCAL"][i] for p in prof)
    remote_bytes[i] = sum(64 * p.counter["OFFCORE_RESPONSE_1:L3_MISS_MISS_REMOTE_HOP1_DRAM"][i] for p in prof)

    fp_single = sum(p.counter["FP_ARITH_INST_RETIRED.SCALAR_SINGLE"][i] for p in prof)
    fp_double = sum(p.counter["FP_ARITH_INST_RETIRED.SCALAR_DOUBLE"][i] for p in prof)

    fp[i] = fp_single + fp_double


for tag in prof[0].unique_tag:
    print(f"duration,bw_local_gb,bw_remote_gb,gflops,arith_intensity,cycles,dram_stall   #{tag}")
    for i in range(ncall):
        if prof[0].tag[i] != tag:
            continue
        giga_per_sec = 1 / (1e9 * dur[i])

        local_bw = local_bytes[i] * giga_per_sec
        remote_bw = remote_bytes[i] * giga_per_sec

        flops = fp[i] * giga_per_sec

        ai = fp[i] / (local_bytes[i] + remote_bytes[i])

        #cycles = prof.counter["CPU_CLK_UNHALTED"][i]
        #stall = prof.counter["CYCLE_ACTIVITY:STALLS_L3_MISS"][i]
        cycles = 1
        stall = 0

        print(dur[i], local_bw, remote_bw, flops, ai, cycles, stall/cycles, sep=",")

    print()
    print()
