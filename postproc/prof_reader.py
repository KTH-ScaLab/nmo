import numpy as np
from collections import defaultdict
from types import SimpleNamespace
import hashlib
import ranges
import multiprocessing
import functools

def hist_count(addr, width):

    rh = defaultdict(int)
    for x in addr:
        k = x - (x%width)
        rh[k] += 1

    return rh

def make_regions(addr, rw):
    rh = defaultdict(int)

    # Count access by base region
    multiprocessing.set_start_method('fork')
    chunks = multiprocessing.cpu_count()
    addr_split=np.array_split(addr,chunks)

    pool = multiprocessing.Pool(processes=chunks)
    rh_split = pool.map(functools.partial(hist_count, width=rw), addr_split)
    pool.close()
    pool.join()

    for rhs in rh_split:
        for key in rhs:
            rh[key] += rhs[key]

    # Remove if <1% accesses
    d = []
    for k in rh:
        if rh[k]/len(addr) < 0.01:
            d.append(k)

    for k in d:
        del rh[k]

    rh = list(sorted(rh))

    # Join adjacent regions
    i = 0
    start = []
    size = []
    while i < len(rh):
        start.append(rh[i])
        size.append(rw)
        j = 1
        while i+j < len(rh) and rh[i+j]-j*rw == start[-1]:
            size[-1] += rw
            j += 1
        i += j

    return start, size

def read(base, sample_index=None, region_width=1024*1024*1024):
    prof = SimpleNamespace()

    prof.addr_tags = []
    prof.raw_counter = {}
    prof.counter_active = {}

    clock_start = []
    clock_stop = []
    sample_md5 = None

    raw_phase = []

    clock_base = 0

    counters_are_cumulative = True

    base = base.removesuffix(".info")

    sample_name = f"{base}.sample"

    for line in open(base+".info", "r"):
        key, val = line.strip().split("=", maxsplit=1)

        if False and key == "num_samples":
            s = val.split(",")
            for i in range(kindex):
                off += int(s[i])
            num = int(s[kindex])
            print(sum(map(int,s)))
        elif key == "addr_tags":
            for el in val.split(","):
                tag, start, end = el.split(":")
                prof.addr_tags.append((tag, int(start, 16), int(end, 16)))
        elif key == "phases":
            for el in val.split(","):
                tag, time = el.split(":")
                raw_phase.append((tag, int(time)))
        elif key == "clock_res":
            prof.clock_res = float(val)
        elif key =="l1_cache_line_size":
            prof.l1_cache_line_size = int(val)
        elif key == "sample_md5":
            md5s = val.strip().split(",")
            if sample_index is None and len(md5s) == 1:
                sample_md5 = md5s[0]
            else:
                try:
                    sample_md5 = md5s[sample_index]
                    sample_name = f"{base}.sample{sample_index}"
                except:
                    #raise ValueError(f"Invalid sample index ({sample_index}) for multi-event sample run")
                    pass
        elif key == "clock_start":
            s = val.split(",")
            clock_base = int(s[0])
            for el in s:
                clock_start.append(int(el))
        elif key == "clock_stop":
            for el in val.split(","):
                clock_stop.append(int(el))
        elif key == "tag":
            prof.tag = val.split(",")
            prof.num_calls = len(prof.tag)
        elif key.isupper():
            prof.raw_counter[key] = np.array(list(map(float, val.split(","))))
        elif key.startswith("active_"):
            counter = key.removeprefix("active_")
            prof.counter_active[counter] = np.array(list(map(float, val.split(","))))
        elif key == "counters_are_not_cumulative":
            counters_are_cumulative = False

    if counters_are_cumulative:
        for key in prof.counter:
            # Invert cumulative sum
            prof.raw_counter[key] = np.diff(prof.raw_counter[key], prepend=0)

    # Default active to 1, for samplers active is missing since they are pinned.
    prof.counter = {key: prof.raw_counter[key] / prof.counter_active.get(key, 1) for key in prof.raw_counter}

    #prof.addr_tags.sort(key=lambda at: at.start)
    prof.addr_tags = list(set(prof.addr_tags))
    i = 0
    n = len(prof.addr_tags)
    while i < n-1:
        t0, s0, e0 = prof.addr_tags[i]
        t1, s1, e1 = prof.addr_tags[i+1]
        if t0 == t1 and e0 == s1:
            prof.addr_tags[i] = (t0, s0, e1)
            del prof.addr_tags[i+1]
            n -= 1
        i += 1

    prof.addr_ranges = ranges.RangeDict()
    for i, (t, s, e) in enumerate(prof.addr_tags):
        prof.addr_ranges[ranges.Range(s, e)] = i
    
    if clock_base == 0:
        clock_base = np.amin(t)

    clock_txfm = lambda x: prof.clock_res * (x - clock_base)

    prof.clock = list(zip(map(clock_txfm, clock_start), map(clock_txfm, clock_stop)))
    prof.clock_start_raw = clock_start
    prof.clock_stop_raw = clock_stop

    prof.unique_tag = list(sorted(set(prof.tag)))
    prof.unique_phase = list(sorted(set(tag[1:] for tag, time in raw_phase)))

    # prof.phase_ranges = ranges.RangeDict()
    # for i in range(len(raw_phase)//2):
    #     stag, stime = raw_phase[2*i]
    #     etag, etime = raw_phase[2*i+1]

    #     assert stag.startswith("+")
    #     assert etag.startswith("-")
    #     assert stag[1:] == etag[1:]

    #     tag_id = prof.unique_phase.index(stag[1:])
    #     prof.phase_ranges[ranges.Range(stime, etime)] = tag_id

    EMPTY_MD5 = "d41d8cd98f00b204e9800998ecf8427e"
    UNINIT_MD5 = "b0e641c998cc3eae6fa2f8726d98cddd"
    if sample_md5 not in (None, EMPTY_MD5, UNINIT_MD5):
        f = np.fromfile(sample_name, dtype=np.uint64)

        if sample_md5 is not None:
            m = hashlib.md5()
            m.update(f.tobytes())
            h = m.hexdigest()
            if h != sample_md5:
                #raise RuntimeError(f"sample_md5={sample_md5}, but actual md5={h}")
                pass

        raw_time = f[::2]

        prof.time = clock_txfm(raw_time)

        prof.addr = f[1::2]

        prof.region_width = np.uint64(region_width)
        prof.region_start, prof.region_size = make_regions(prof.addr, prof.region_width)
        prof.num_regions = len(prof.region_start)

        # prof.sample_phase = np.empty(len(prof.time), dtype=int)
        # for i in range(len(prof.time)):
        #     prof.sample_phase[i] = prof.phase_ranges.get(raw_time[i], -1)

    return prof
