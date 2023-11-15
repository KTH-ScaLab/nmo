import numpy as np
import matplotlib.pyplot as plt
import sys
import prof_reader

base = sys.argv[1]
sample_index = None
if len(sys.argv) > 2:
    sample_index = int(sys.argv[2])
prof = prof_reader.read(base, sample_index=sample_index)

import matplotlib
matplotlib.rc("xtick", labelsize=9)

# Make one histogram per region
f, axs = plt.subplots(1, prof.num_regions, sharey=True, squeeze=False, width_ratios=prof.region_size, figsize=(6.4, 3.2))
axs = axs.flatten()
f.subplots_adjust(bottom=0.225)

axs[0].set_ylabel("Number of samples")
f.supxlabel("Virtual address prefix")

colors = ["blue", "red", "orange", "green", "magenta", "yellow", "teal"]
tag_idx = {}

for i in range(prof.num_regions):
    reg_start = prof.region_start[i]
    reg_size = prof.region_size[i]
    reg_end = reg_start + reg_size
    reg_ext = (reg_start, reg_end)

    #axs[i].hist(prof.addr, bins=int(50*reg_size/prof.region_width), range=reg_ext, log=False)
    axs[i].hist(prof.addr, bins=int(200*reg_size/prof.region_width), range=reg_ext, log=False)
    axs[i].grid(axis="y")
    #l = list(map(lambda n: "%04x"%np.uint64(n//rw), reg_ext))
    l = list(map(lambda n: ("%012x"%np.uint64(n))[:5], reg_ext))
    axs[i].set_xticks(ticks=reg_ext, labels=l, rotation=90)

    for j, (tag, tag_start, tag_end) in enumerate(prof.addr_tags):
        if tag_start > reg_end or tag_end < reg_start:
            continue

        try:
            axs[i].axvspan(tag_start, tag_end, facecolor=colors[tag_idx[tag]], alpha=0.2)
        except KeyError:
            tag_idx[tag] = len(tag_idx)
            axs[i].axvspan(tag_start, tag_end, facecolor=colors[tag_idx[tag]], alpha=0.2, label=tag)

for i in range(prof.num_regions-1):
    axs[i].spines['right'].set_visible(False)
    axs[i+1].spines['left'].set_visible(False)
    #axs[i+1].set_yticks([])
    #axs[i+1].set_yticks([], minor=True)
    #plt.setp(axs[i+1].get_yticks(), visible=False)
    #axs[i+1].tick_params(left=False,right=False,minor=True)

f.legend()

if sample_index is None:
    plt.savefig("hist_" + base)
else:
    plt.savefig("hist_" + base + "-" + str(sample_index))
#plt.show()
