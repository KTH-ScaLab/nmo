import numpy as np
import matplotlib.pyplot as plt
import sys
import prof_reader

base = sys.argv[1]
sample_index = None
#if len(sys.argv) > 2:
    #sample_index = int(sys.argv[2])
mib = float(sys.argv[2])
prof = prof_reader.read(base, sample_index=sample_index)

N = 2*1024*1024
#N = 4096

pages = np.empty_like(prof.addr)
for i, a in enumerate(prof.addr):
    pages[i] = prof.addr[i] & np.uint64(~(N-1))
pages.sort()

total_access = len(pages)

page_counts = []
i = 0
while i < len(pages):
    span = 1
    while i < len(pages)-span and pages[i] == pages[i+span]:
        span += 1
    page_counts.append(span)
    i += span

#print([hex(p) for p in pages[:50]])
#for c in page_counts: print(c)

page_counts.sort(reverse=True)
#wss = len(page_counts)
wss = int(mib*1024**2/N)

cs = np.cumsum(page_counts)
for i in range(len(cs)):
    cs[i] = (100.0 * cs[i]) /float(cs[-1])

if len(cs)<wss:
    cs = np.pad(cs, (0, wss-len(cs)), mode="edge")
else:
    print((wss-len(cs))/wss)
    wss=len(cs)

np.savetxt(f"cumul_2M_{base}.txt", cs, fmt="%03.2f")
sys.exit(0)

plt.rcParams["figure.figsize"] = (4,3)

plt.grid(ls=":")
plt.xlabel("% Memory Footprint")
plt.ylabel("% Accesses")
plt.plot(np.linspace(0, 100, num=wss), cs)

plt.savefig(f"cumul_2M_{base}.pdf", bbox_inches="tight")
#plt.savefig(f"cumul_{base}.pdf", bbox_inches="tight")

print(f"samples {total_access}")
