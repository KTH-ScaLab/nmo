#!/bin/bash

set -m
set -x

rm -f tmp_fifo

set -e

mkfifo tmp_fifo

read < tmp_fifo & read_pid=$!
numactl -N0 -m1 ./bw $((1024*1024*1024)) $1 -1 > tmp_fifo & bw=$!
wait $read_pid

sudo pcm -silent -nc -nsys -csv=tmp.csv & pcm=$!
sleep 4

kill $bw
sudo kill $pcm
wait

#awk 'BEGIN{FS=","}{print $58,$59,$60,$64,$65,$66,$70,$71,$72,$76,$77,$78}' < tmp.csv
#awk 'BEGIN{FS=","}{print $55,$56,$57,$61,$62,$63,$67,$68,$69,$73,$74,$75}' < tmp.csv
#skt0datain+skt0trafficout
#awk 'BEGIN{FS=","}{din=$55+$56+$57; tout=$67+$68+$69; print(din+tout)}' < tmp.csv | datamash max 1
#skt0traffic+skt1traffic
awk 'BEGIN{FS=","}{s0=$67+$68+$69; s1=$73+$74+$75; print(s0+s1)}' < tmp.csv | datamash max 1
sudo mv tmp.csv upi_${OMP_NUM_THREADS}_$1.csv

rm -f tmp_fifo
