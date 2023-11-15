import numpy as np
import matplotlib.pyplot as plt
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

labels = [tag for tag, _, _ in prof.addr_tags] + ["other"]
times = [[] for i in range(len(prof.addr_tags) + 1)]

for i in range(len(prof.addr)):
    for j in range(len(prof.addr_tags)):
        _, start, end = prof.addr_tags[j]
        if prof.addr[i] >= start and prof.addr[i] < end:
            times[j].append(prof.time[i])
            break
    else:
        times[-1].append(prof.time[i])

if len(times[-1]) < 0.01*len(prof.time) or len(times) == 1:
    labels[-1] = ""

if len(prof.unique_tag) > 1:
    labeled_tags = set()
    tag_color = {}
    unique_tags = list(set(prof.tag))
    unique_tags.sort()
    color = plt.cm.rainbow(np.linspace(0, 1, len(unique_tags)))

    for tag, c in zip(unique_tags, color):
        tag_color[tag] = c

    for j, (start, stop) in enumerate(prof.clock):
        tag = prof.tag[j]
        if tag in labeled_tags:
            plt.axvspan(start, stop, alpha=0.15, color=tag_color[tag])
        else:
            plt.axvspan(start, stop, alpha=0.15, color=tag_color[tag], label=tag)
            labeled_tags.add(tag)

    plt.legend()

plt.hist(times, label=labels, bins=1000, stacked=True)

plt.xlabel("Time (s)")
plt.ylabel("Number of samples")
plt.grid(alpha=0.25)


plt.savefig("time_" + base)
