#!/usr/bin/env python3

import numpy as np
import sys

rss = np.loadtxt(sys.argv[1], delimiter=",", skiprows=1)

loc = rss[:,1]
rem = rss[:,2]
tot = loc + rem
mi = np.argmax(tot)
m = tot[mi]

mib = lambda b: round(b/1024/1024)

print("Max =", mib(m), "MiB", f"({mib(loc[mi])}+{mib(rem[mi])})")
print("Pool% =", round(100*rem[mi]/tot[mi]))
print()

print("25% =", mib(0.25*m), "MiB")
print("50% =", mib(0.50*m), "MiB")
print("75% =", mib(0.75*m), "MiB")
