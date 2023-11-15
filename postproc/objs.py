import numpy as np
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

counts = {}

print("object\tsize\t-", end="")
print(f"\tinit", end="")
for p in prof.unique_phase:
    print(f"\t{p}", end="")
print()
print("---------------------------------------------")

for i in range(len(prof.time)):
    obj = prof.addr_ranges.get(prof.addr[i], -1)
    k = (obj, prof.sample_phase[i])
    counts[k] = counts.get(k, 0) + 1

for oi in list(range(len(prof.addr_tags))) + [-1]:
    if oi == -1:
        oname = "other"
        size = ""
    else:
        at = prof.addr_tags[oi]
        oname = at[0]
        size = at[2]-at[1]

    print(oname, size, "", sep="\t", end="")
    for p in [-1] + list(range(len(prof.unique_phase))):
        c = counts.get((oi,p), 0)
        print(f"\t{c}", end="")
    print()

print()
print("total samples", len(prof.time))
