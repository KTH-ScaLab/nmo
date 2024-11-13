import numpy as np
import matplotlib.pyplot as plt
import sys
import prof_reader

base = sys.argv[1]
prof = prof_reader.read(base)

import matplotlib
matplotlib.rc("xtick", labelsize=9)

# Calculate sample tag
prof.sample_tag = np.full(len(prof.time), -1)
i = 0
j = 0
while True:
    try:
        t = prof.time[i]
        start, stop = prof.clock[j]
    except IndexError:
        break
    if t < start:
        i += 1
    elif t < stop:
        prof.sample_tag[i] = prof.unique_tag.index(prof.tag[j])
        i += 1
    else:
        j += 1

utag_addr = [prof.addr[prof.sample_tag == i] for i in range(len(prof.unique_tag))]

# Make one histogram per region and tag
f, axs = plt.subplots(len(prof.unique_tag), prof.num_regions, sharey=True, squeeze=False, width_ratios=prof.region_size, figsize=(6.4, 1.5*3.2))
#f.subplots_adjust(bottom=0.225)
axs = axs.T

f.supxlabel("Virtual address prefix")
f.supylabel("Number of samples")

for j, ax in enumerate(axs[-1,:]):
    #ax.set_ylabel(prof.unique_tag[j], rotation=0, size='large')
    ax.yaxis.set_label_position("right")
    ax.set_ylabel(prof.unique_tag[j], rotation=90, size='large')

f.tight_layout()

colors = ["blue", "red", "orange", "green"]
tag_idx = {}

for i in range(prof.num_regions):
    reg_start = prof.region_start[i]
    reg_size = prof.region_size[i]
    reg_end = reg_start + reg_size
    reg_ext = (reg_start, reg_end)

    for j, addr in enumerate(utag_addr):
        axs[i][j].hist(addr, bins=int(50*reg_size/prof.region_width), range=reg_ext, log=False)
        axs[i][j].grid(axis="y")
        #l = list(map(lambda n: "%04x"%np.uint64(n//rw), reg_ext))
        l = list(map(lambda n: ("%012x"%np.uint64(n))[:5], reg_ext))
        axs[i][j].set_xticks(ticks=reg_ext, labels=l if j == len(utag_addr)-1 else ["",""], rotation=90)

    for j, (tag, tag_start, tag_end) in enumerate(prof.addr_tags):
        if tag_start > reg_end or tag_end < reg_start:
            continue

        try:
            for a in axs[i][:]: a.axvspan(tag_start, tag_end, facecolor=colors[tag_idx[tag]], alpha=0.2)
        except KeyError:
            tag_idx[tag] = len(tag_idx)
            for a in axs[i][:]: a.axvspan(tag_start, tag_end, facecolor=colors[tag_idx[tag]], alpha=0.2, label=tag)

for i in range(prof.num_regions-1):
    for a in axs[i][:]: a.spines['right'].set_visible(False)
    for a in axs[i+1][:]: a.spines['left'].set_visible(False)

f.legend()

plt.savefig("multihist_" + base)
#plt.show()
