import numpy as np
import matplotlib.pyplot as plt
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

cm = plt.cm.rainbow(np.linspace(0, 1, len(prof.unique_tag)))
colmap = {tag: c for tag, c in zip(prof.unique_tag, cm)}

dur = [stop-start for start, stop in prof.clock]
col = None
lab = None

if len(prof.unique_tag) > 1:
    col = [colmap[tag] for tag in prof.tag]
    labeled = set()
    lab = []
    for tag in prof.tag:
        if tag in labeled:
            lab.append("")
        else:
            lab.append(tag)
            labeled.add(tag)

plt.bar(np.arange(len(dur)), dur, color=col, label=lab)

if len(prof.unique_tag) > 1:
    plt.legend()

plt.ylabel("Execution time (s)")
plt.xlabel("Kernel invocation")

plt.savefig("exec_" + base)
