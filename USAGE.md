# NMO User Manual

## Heterogeneous Memory Setup

NMO targets systems with heterogeneous memory, where each memory type is mapped to one NUMA node. There is also support for emulating a heterogeneous memory system on a dual-socket homogeneous memory system.

### Heterogeneous Memory Emulation

To emulate a heterogeneous memory system, the target process is restricted to use only the processor on NUMA node 0. Thus the memory in NUMA node 0 (the local socket) acts as a faster tier and the memory in NUMA node 1 (the remote socket) acts as a slower memory tier. This is achieved using the `numactl` command.

```
$ numactl -N 0 ./myapp
```

To emulate various configurations of heterogeneous memory, NMO offers a tool to limit the size of an emulated memory tier. The tool `setup_waste` will fill up the target NUMA node with locked memory pages until it reaches the requested free space. In this way, it is possible to emulate for instance a fast tier in NUMA node 0 with only 1024 MB memory:

```
$ sudo bin/setup_waste 1024
```

The pages remain locked until the `setup_waste` process is killed.

Depending on the amount of memory, the tool can take a while to allocate and lock all of the required pages. For automation purposes, it is useful to be able to wait for the tool to complete the setup before executing the next step. The same goes for killing the tool to tear down the setup again. The tool accepts a second, optional argument, which is the name of a file to write to to signal that the setup is finished. The following is a useful pattern in bash scripts.

```bash
set -m

FIFO=waste_fifo

read <$FIFO & read_pid=$!
sudo bin/setup_waste XXX $FIFO & waste_pid=$!
wait $read_pid

# Conduct heterogeneous memory experiment here
# ...

sudo kill $waste_pid
wait $waste_pid

rm -f $FIFO
```

By default the tool targets to fill NUMA node 0. However, it also offers a third parameter which may be used to specify the targeted NUMA node.
For example, to instead target NUMA node 1, the following command may be used.

```
$ sudo bin/setup_waste 1024 /dev/null 1
```

## Instrumentation

NMO supports two different methods of instrumentation. The simplest is run-time instrumentation, while compile-time instrumentation is required to enable some of the advanced features.

Regardless of the instrumentation method, NMO must be enabled by defining the environment variable `NMO_ENABLE`. Without it, NMO instrumentation will remain disabled and the application is executed without any profiling overhead.

```
$ NMO_ENABLE=1 ./myapp
```

### Run-Time Instrumentation

Run-time instrumentation relies on preloading the NMO library using the dynamic linker. This is achieved by using the environment variable named `LD_PRELOAD`.

```
$ LD_PRELOAD=/path/to/nmo/lib/libnmo.so ./myapp
```

With run-time instrumentation, any binary can be instrumented without re-compilation or access to source code. However, advanced NMO features such as kernel tagging and memory address tagging are not available.

### Compile-Time Instrumentation

Compile-time instrumentation relies on linking the NMO library into a binary at compile-time. This is achieved by using adding `-lnmo` to the compile command.

```
$ gcc -o myapp -lnmo myapp.c
```

Using the compile-time instrumentation enables advanced features of NMO such as kernel tagging and memory address tagging. These enable users to insert simple tracing calls into their code which help to correlate the profiling results with their source code.

#### Kernel Tagging

Kernel tagging allows users to tag a certain time interval using a custom label. It allows users to correlate both counter values and samples to certain computational kernels/sections in the source code. The timestamp of the kernel start and end is saved in the info file.

To use kernel tagging, NMO offers the following API, where `nmo_start()` indicates the start of a kernel and `nmo_stop()` indicates the end of a kernel.

```c
#include <nmo.h>

nmo_start("my_kernel");
nmo_stop();
```

#### Memory Address Tagging

Memory address tagging allows users to tag address ranges during execution using a custom label. It
helps to correlate the collected memory reference samples with objects in the program source code. The tags are saved in
the info file.

The following snippet illustrates the address tagging API. The basic functionality is offered by `nmo_tag_addr()` which takes a label, a start address, and an end address. Additionally, the function `nmo_tag_from_maps()` allows tagging the memory address range of a memory-mapped file or one of the special process memory regions.

```c
#include <nmo.h>

nmo_tag_addr("vertices", vertex_buffer, vertex_buffer+n);
nmo_tag_from_maps("mmapped_file", "/full/path/to/file");
nmo_tag_from_maps("heap", "[heap]");
```

Special memory regions in Linux include the following:
- `[heap]`: The heap where smaller memory allocations by `malloc()` etc are located.
- `[stack]`: The stack (of the main thread) where return addresses and local variables are stored.

For a full reference, search for `/proc/[pid]/maps` in `man proc`.

## Profile Collection

NMO offers several profiling modes for capturing different metrics from the target application.
The captured data is saved into two files: `nmo.info` and `nmo.sample`. The info
file contains counter values and other metadata in text format. The sample file
contains raw memory reference samples, consisting of a timestamp and a virtual memory address. Working set size results are written to the file `nmo.rss.csv`.

All configuration options are provided using environment variables. The following table explains all the options available in NMO.

| Option          | Description               | Default |
|-----------------|---------------------------|---------|
| `NMO_ENABLE`    | Enable profile collection | off     |
| `NMO_NAME`      | Base name of output files | "nmo"   |
| `NMO_MODE`      | Profile collection mode   | none    |
| `NMO_PERIOD`    | Sampling period           | 0       |
| `NMO_TRACK_RSS` | Capture working set size  | off     |

Note that most profiling modes of NMO use the perf_event system in Linux. In some configurations, it is only available to the root user. The sysctl `kernel.perf_event_paranoid` can be used to control the access to perf_event for normal users. This value should be at most 1 to allow regular users to collect profiles using NMO.

### Extended Roofline Profiling

```
$ NMO_MODE=perp ./myapp
```

In the extended roofline profiling mode, NMO tracks counters related to memory references to different memory tiers. This data allows users to determine the number of memory references to the fast and slow memory tiers separately, to understand how different memory tiers are utilized in a heterogeneous memory system.

> remote access ratio = remote memory references / total memory references

Additionally, counters related to floating-point operations (flops) are captured, allowing users to compute the arithmetic intensity of different regions of the code.

> arithmetic intensity = flops / bytes loaded from memory

### Prefetcher Profiling

```
$ NMO_MODE=pf ./myapp
```

In the prefetcher profiling mode, NMO tracks counters related to the hardware prefetcher. The prefetcher loads data from memory into the cache before it has been explicitly requested by the processor. If the workload is predictable, prefetching can greatly improve the performance of memory-bound codes. However, if the workload is unpredictable, prefetching could cause unnecessary loads that go unused, reducing the performance.

> prefetching accuracy = (prefetched data - unused prefetched data) / prefetched data


> prefetching coverage = (prefetched data - unused prefetched data) / (loaded data - unused prefetched data)

### Dummy Profiling

```
$ NMO_MODE=noperp ./myapp
```

In the dummy profiling mode, no performance counters are tracked. However, all the usual meta-data is still captured and saved in the info file. For instance, kernel tagging may be used to provide timing information without any profiler overhead.

### Working Set Profiling

```
$ NMO_TRACK_RSS=1 ./myapp
```

In the working set profiling mode, NMO tracks the total memory usage of the target application, i.e. the Resident Set Size (RSS). The working set is tracked separately for each memory tier, which allows users to understand how much memory capacity is used from different memory resources. For instance, it is possible to compute the remote capacity ratio indicating how much of the total memory usage is located in a remote memory tier.

> remote capacity ratio = remote tier working set size / total working set size

### Memory Reference Sampling

```
$ NMO_MODE=perp NMO_PERIOD=10000 ./myapp
```

Memory reference sampling is a further extension of the extended roofline profiling mode. In this mode, NMO samples the virtual memory address of a subset of memory loads by using hardware-assisted event-based sampling. The period determines the sampling frequency. A period of 10,000 means that 1/10,000 of the total memory loads will be sampled. The period should be adjusted to limit the sampling overhead while ensuring good coverage.

If the period is set to short, the kernel may throttle the sample rate. In Linux, the max sample rate is controlled by the file `/proc/sys/kernel/perf_event_max_sample_rate`, or equivalently the sysctl `kernel.perf_event_max_sample_rate`. If the sample rate is throttled, a message is printed to the kernel ring buffer, which may be viewed using the `dmesg` command. In this case, the max sample rate may be set prohibitively low, and the user may which to increase it again by writing a new value to the control file. An example of this kernel message is shown below. Note that the NMO sample period counts the number of samples per total number of events, while the kernel sample rate counts the number of events per second.

```
perf: interrupt took too long (3147 > 3128), lowering kernel.perf_event_max_sample_rate to 63000
```

### Custom Profiling

The NMO source code is highly configurable, and users can define their own custom profiling modes by modifying the file `src/periodic_profiler.cc`. Here, any performance event counters may be enabled by adding them to the event list.

## Post-Processing

NMO provides a large suite of Python scripts to post-process the profiling results, generating CSV files or plots. These scripts are located in the `postproc/` directory.

- `sperp.py` provides a summary of extended roofline profiling data per kernel tag.
- `cumul.py` generates the cumulative distribution of memory accesses in the memory footprint of an application.
- `scat.py` visualizes sampled memory references as a scatter plot.
- `rss_sum.py` provides a summary of working set profiling data.
- `phase_time.py` computes the time spent in different tagged kernels.
- Etcetera...

The script `prof_reader.py` contains all the logic for parsing info and sample files. It can be used as a library for users who want to develop their own Python-based post-processing tools in NMO.

## File Formats

While NMO offers many tools for post-processing profiling results, the output files user simple formats which may be processed using custom tools. This section describes those formats so that users may develop their own post-processing tools on top of NMO.

### Info file

The info file follows a simple key-value pair structure. Each line defines one pair, first the name of the key, then the symbol `=`, and then the value. Some keys support multiple values, which are separated by the symbol `,`.

```
key1=value
key2=value1,value2,value
```

Upper-case key names are reserved for hardware counter values. 

Info file parsing is handled by the script `postproc/prof_reader.py`, which may serve as a full reference to the keys used in NMO.

### Sample file

The sample file follows a simple binary format. The file can be seen as an array of records. Each record describes one sample. It consists of first a 64-bit timestamp value and second a 64-bit virtual memory address. The resolution of the timestamp value is described by the `clock_res` key in the info file, given in seconds.

### RSS CSV file

The RSS file stores the memory usage of the target process at one second intervals. It follows the Comma Separated Values format. The first column is a timestamp using the same format as in the sample file. The second and third columns show the memory usage divided into the fast/local tier and the slow/remote tier respectively. The unit is MB.

## LBench – Memory Interference Tools

NMO additionally includes tools to study the performance of applications under memory interference. Interference can be caused by other users in a multi-tenancy or disaggregated memory scenario. LBench is built upon the `bw` tool, which generates memory traffic with a configurable intensity. LBench can be executed in continuous mode to generate background memory traffic, or in constant mode as a probe to measure the current memory interference level.

### Generating Memory Traffic

```
$ lbench/bw array_bytes nflops trials
```

The `bw` command allocates an array of size `array_bytes`. For every array element, which is of type double, i.e. 8 bytes, the tool performs `nflops` floating-point operations. The process is repeated `trials` times. The array size can be adjusted to target a level in the cache hierarchy, or the main memory. A recommended starting point is 1 GB (1073741824 bytes). By increasing the number of floating-point operations, the memory pressure is reduced as the time between memory references is increased, and vice versa. The trials parameter can be set to -1 to run continuously until the process is killed.
The `bw` command is parallelized using OpenMP, and the number of threads can be controlled with the `OMP_NUM_THREADS` environment variable.

### Background Traffic – Continuous Mode

The continuous mode is enabled by setting the trials parameter to -1. The traffic generator is running continuously in the background while performing some experiments. An example of using the continuous mode is shown in the script `lbench/upi.sh`.

### Interference Probe – Constant Mode

The constant mode is enabled by setting the trials parameter to a positive value. The `bw` command is executed as a probe to measure the current interference state of the system. When the tool is executed, it will output the time it took to finish the specified number of iterations. By comparing the time to a baseline in an idle system, it is possible to determine the amount of interference at a certain point in time. The slowdown in the probe is called the interference coefficient (IC).

> interference coefficient = probe time / probe time on idle system
