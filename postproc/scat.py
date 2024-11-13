import numpy as np
import matplotlib.pyplot as plt
import sys
import prof_reader

base = sys.argv[1]
sample_index = None
if len(sys.argv) > 2:
    sample_index = int(sys.argv[2])
prof = prof_reader.read(base, sample_index=sample_index)

#import matplotlib
#matplotlib.rc("xtick", labelsize=9)

# Make one plot per region
f, axs = plt.subplots(len(prof.region_start), 1, sharex=True, squeeze=False, height_ratios=list(reversed(prof.region_size)), figsize=(16,16))

axs = axs.flatten()
axs = np.flip(axs)
#f.subplots_adjust(bottom=0.225)

#axs[0].set_ylabel("Number of sampled L2 misses")

f.supxlabel("Time (s)")
f.supylabel("Virtual address prefix")

colors = ["orange", "green", "blue", "red", "purple"]
tag_idx = {}

labeled_tags = set()
tag_color = {}
unique_tags = list(set(prof.tag))
unique_tags.sort()
color = plt.cm.rainbow(np.linspace(0, 1, len(unique_tags)))

for tag, c in zip(unique_tags, color):
    tag_color[tag] = c

for i, (reg_start, reg_size) in enumerate(zip(prof.region_start, prof.region_size)):
    interval = [reg_start, reg_start+reg_size]
    l = list(map(lambda n: ("%012x"%np.uint64(n))[:5], interval))

    axs[i].set_ylim(reg_start, reg_start+reg_size)
    axs[i].set_yticks(ticks=interval, labels=l)

    axs[i].scatter(prof.time, prof.addr, marker=".", s=1)

    for j, (tag, tag_start, tag_end) in enumerate(prof.addr_tags):
        try:
            axs[i].axhspan(tag_start, tag_end, facecolor=colors[tag_idx[tag]], alpha=0.2)
        except KeyError:
            tag_idx[tag] = len(tag_idx)
            axs[i].axhspan(tag_start, tag_end, facecolor=colors[tag_idx[tag]], alpha=0.2, label=tag)

    if len(prof.unique_tag) > 1:
        for j, (start, stop) in enumerate(prof.clock):
            tag = prof.tag[j]
            if tag in labeled_tags:
                axs[i].axvspan(start, stop, alpha=0.2, color=tag_color[tag])
            else:
                axs[i].axvspan(start, stop, alpha=0.2, color=tag_color[tag], label=tag)
                labeled_tags.add(tag)

if len(prof.unique_tag) > 1 or len(prof.addr_tags) > 0:
    f.legend()

for i in range(len(prof.region_start)-1):
    axs[i].spines['top'].set_visible(False)
    axs[i+1].spines['bottom'].set_visible(False)
    #axs[i+1].set_yticks([])
    #axs[i+1].set_yticks([], minor=True)
    #plt.setp(axs[i+1].get_yticks(), visible=False)
    #axs[i+1].tick_params(left=False,right=False,minor=True)

plt.savefig("scat_" + base)
#plt.show()
