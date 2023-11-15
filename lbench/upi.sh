#!/bin/bash

threads="$1"
int="$2"

case "$threads" in
    1)
        export OMP_NUM_THREADS=1
        case "$int" in
            0)  flop=-1
                ;;
            10) flop=21
                ;;
            20) flop=12
                ;;
            30)
                flop=4
                ;;
            *)
                echo "unsupported intensity"
                exit 1
                ;;
        esac
        ;;
    2)
        export OMP_NUM_THREADS=2
        case "$int" in
            0)  flop=-1
                ;;
            10) flop=30
                ;;
            20) flop=20
                ;;
            30)
                flop=14
                ;;
            40)
                flop=9
                ;;
            50)
                flop=4
                ;;
            *)
                echo "unsupported intensity"
                exit 1
                ;;
        esac
        ;;
    3)
        export OMP_NUM_THREADS=3
        case "$int" in
            0)  flop=-1
                ;;
            70)  flop=1
                ;;
        esac
        ;;
    *)
        echo "unsupported threads"
        exit 1
        ;;
esac

exec numactl -N0 -m1 $(dirname "$BASH_SOURCE")/bw $((1024*1024*1024)) $flop -1
