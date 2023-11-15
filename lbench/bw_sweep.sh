#!/bin/bash

for threads in 1 2 4 8 12 16 20 24; do
    echo "# threads=$threads"
    for flops in 1 2 4 8 16 32 64; do
        echo -n "$flops "
        OMP_NUM_THREADS=$threads ./val_bw.sh $flops
    done
    echo; echo
done
