import numpy as np
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

total_bytes = np.zeros(prof.num_calls)
remote_bytes = np.zeros(prof.num_calls)
dur = np.zeros(prof.num_calls)
inst = np.zeros(prof.num_calls)
l1_cache_line_size = prof.l1_cache_line_size

for i in range(prof.num_calls):
    dur[i] = prof.clock[i][1] - prof.clock[i][0]

    total_bytes[i] = l1_cache_line_size * prof.counter["BUS_ACCESS_RD"][i]
    remote_bytes[i] = l1_cache_line_size * prof.counter["REMOTE_ACCESS"][i]
    inst[i] = prof.counter["INST_RETIRED"][i]

local_bytes = total_bytes - remote_bytes

print("duration,bw_local_gb,bw_remote_gb,byte_per_inst,tag")
for i in range(prof.num_calls):
    giga_per_sec = 1 / (1e9 * dur[i])

    local_bw = local_bytes[i] * giga_per_sec
    remote_bw = remote_bytes[i] * giga_per_sec

    tag = prof.tag[i]

    bpi = total_bytes[i] / inst[i]

    print(dur[i], local_bw, remote_bw, bpi, tag, sep=",")
