import numpy as np
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

local_bytes = np.zeros(prof.num_calls)
remote_bytes = np.zeros(prof.num_calls)
fp = np.zeros(prof.num_calls)
time = np.zeros(prof.num_calls)

fp_width = [
    ("FP_ARITH_INST_RETIRED.SCALAR_SINGLE", 1),
    ("FP_ARITH_INST_RETIRED.SCALAR_DOUBLE", 1),

    ("FP_ARITH_INST_RETIRED.128B_PACKED_DOUBLE", 2),
    ("FP_ARITH_INST_RETIRED.128B_PACKED_SINGLE", 4),
    ("FP_ARITH_INST_RETIRED.256B_PACKED_DOUBLE", 4),
    ("FP_ARITH_INST_RETIRED.256B_PACKED_SINGLE", 8),
    ("FP_ARITH_INST_RETIRED.512B_PACKED_DOUBLE", 8),
    ("FP_ARITH_INST_RETIRED.512B_PACKED_SINGLE", 16),
]

for i in range(prof.num_calls):
    time[i] = prof.clock[i][1]

    # local_bytes[i] = 64 * prof.counter["OFFCORE_RESPONSE_0:DMND_DATA_RD"][i]
    # remote_bytes[i] = 64 * prof.counter["OFFCORE_RESPONSE_1:PF_L2_DATA_RD"][i]

    try:
        local_bytes[i] = 64 * prof.counter["OFFCORE_RESPONSE_0:L3_MISS_LOCAL"][i]
        remote_bytes[i] = 64 * prof.counter["OFFCORE_RESPONSE_1:L3_MISS_MISS_REMOTE_HOP1_DRAM"][i]
    except KeyError:
        print("warning: using demand load counters", file=sys.stderr)
        local_bytes[i] = 64 * prof.counter["MEM_LOAD_UOPS_L3_MISS_RETIRED:LOCAL_DRAM"][i]
        remote_bytes[i] = 64 * prof.counter["MEM_LOAD_UOPS_L3_MISS_RETIRED:REMOTE_DRAM"][i]

    fp[i] = 0
    for ev, w in fp_width:
        fp[i] += prof.counter[ev][i] * w

    print(time[i], fp[i], local_bytes[i], remote_bytes[i], sep="\t")

