import numpy as np
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

# for counter, vals in prof.counter.items():
#     print(counter)
#     print(vals)
#     mean = np.mean(vals)
#     std = np.std(vals)
#     print(mean)
#     print(std)
#     print(std/mean if mean != 0 else 0)
#     print()

# print(",".join(prof.counter))
# for i in range(prof.num_calls):
#     for vals in prof.counter.values():
#         print(vals[i], end=",")
#     print()

local_bytes = np.zeros(prof.num_calls)
remote_bytes = np.zeros(prof.num_calls)
fp = np.zeros(prof.num_calls)
dur = np.zeros(prof.num_calls)


for i in range(prof.num_calls):
    dur[i] = prof.clock[i][1] - prof.clock[i][0]

    local_bytes[i] = 64 * prof.counter["OFFCORE_RESPONSE_0:L3_MISS_LOCAL"][i]
    remote_bytes[i] = 64 * prof.counter["OFFCORE_RESPONSE_1:L3_MISS_MISS_REMOTE_HOP1_DRAM"][i]

    fp_single = prof.counter["FP_ARITH_INST_RETIRED.SCALAR_SINGLE"][i]
    fp_double = prof.counter["FP_ARITH_INST_RETIRED.SCALAR_DOUBLE"][i]

    fp[i] = fp_single + fp_double

# print("local_bytes remote_bytes")
# for i in range(prof.num_calls):
#     print(local_bytes[i], remote_bytes[i])

# print()

print("duration,bw_local_gb,bw_remote_gb,gflops,arith_intensity,cycles,dram_stall,tag")
for i in range(prof.num_calls):
    giga_per_sec = 1 / (1e9 * dur[i])

    local_bw = local_bytes[i] * giga_per_sec
    remote_bw = remote_bytes[i] * giga_per_sec

    flops = fp[i] * giga_per_sec

    ai = fp[i] / (local_bytes[i] + remote_bytes[i])

    cycles = prof.counter["CPU_CLK_UNHALTED"][i]
    stall = prof.counter["CYCLE_ACTIVITY:STALLS_L3_MISS"][i]

    tag = prof.tag[i]

    print(dur[i], local_bw, remote_bw, flops, ai, cycles, stall/cycles, tag, sep=",")
