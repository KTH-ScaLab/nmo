import numpy as np
import sys

a = np.loadtxt(sys.argv[1], delimiter=",", skiprows=1)
b = np.loadtxt(sys.argv[2], delimiter=",", skiprows=1)

print("time:",a[:,0]/b[:,0])

bwa = a[:,1]+a[:,2]
bwb = b[:,1]+b[:,2]

print("bw:",bwb/bwa)
