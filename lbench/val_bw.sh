#!/bin/bash

set -m
set -x

rm -f tmp_fifo

set -e

mkfifo tmp_fifo

read < tmp_fifo & read_pid=$!
numactl -N0 -m0 ./bw $((1024*1024*1024)) $1 -1 > tmp_fifo & bw=$!
wait $read_pid

sudo pcm-memory -silent -csv=tmp.csv & pcm=$!
sleep 4

kill $bw
sudo kill $pcm
wait

awk 'BEGIN{FS=","}{x=$27+$28;if(x>m)m=x}END{print m}' < tmp.csv
sudo mv tmp.csv bw_${OMP_NUM_THREADS}_$1.csv

rm -f tmp_fifo
