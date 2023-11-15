import prof_reader
from sys import argv

prefix = argv[1]

# Use rank 0 for MPI app
if prefix.startswith("nekrs_"):
    prefix += "-0"

prof = prof_reader.read(prefix)

start = prof.clock_start_raw[0]
stop = prof.clock_stop_raw[-1]

with open(f"{argv[1]}.gl") as f:
    prev_line = None

    for line in f:
        clock = int(line.split()[0])
        if clock < start:
            continue
        if clock > stop:
            break
        if prev_line is not None:
            print(prev_line, end="")
        prev_line = line
    else:
        print(prev_line, end="")
