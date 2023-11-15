#!/usr/bin/env python3

import numpy as np
import sys

for line in sys.stdin:
    if not line.startswith("phases="):
        continue

    total = {}

    _, ls = line.split("=")
    ll = ls.split(",")

    for i in range(len(ll)//2):
        stag, stime = ll[2*i].split(":")
        etag, etime = ll[2*i+1].split(":")

        assert stag.startswith("+")
        assert etag.startswith("-")
        assert stag[1:] == etag[1:]

        tag = stag[1:]
        total[tag] = total.get(tag, 0) + 1e-9*(int(etime)-int(stime))
        #print(1e-9*(int(etime)-int(stime)))

    for tag in total:
        print(tag, total[tag], sep="\t")
