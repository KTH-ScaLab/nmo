import numpy as np
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

print("time", *[k for k in prof.counter], sep="\t")
for i in range(prof.num_calls):
    t = prof.clock[i][1]

    print(t, *[prof.counter[k][i] for k in prof.counter], sep="\t")

