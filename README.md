# NMO: Non-homogeneous Memory Observatory

NMO is a memory-centric profiling tool suite for heterogeneous memory.

## Usage

NMO can be built using Make. It depends on libpfm, libnuma and openssl.

    make -j

Documentation of NMO is available in the [NMO User Manual](USAGE.md).

### Supported architectures

- Intel Skylake-X
- ARM64

### Arm Setup

NMO use SPE to sample data in the ARM architecture, so we need to load SPE into perf tool when profiling virtual address of memory. Disable kernel page table isolation for the target. To ensure that kernel page table isolation is disabled, boot the machine with the command-line argument ```kpti=off```. Use the following command to check if SPE loaded successfully.

```
$ perf list | grep "arm_spe"
  arm_spe_0//             [Kernel PMU event]
```

## Publications

> J. Wahlgren, G. Schieffer, M. Gokhale, and I. B. Peng, “A Quantitative Approach
for Adopting Disaggregated Memory in HPC Systems,” in _Proceedings of the
International Conference for High Performance Computing, Networking, Storage
and Analysis (SC)_, 2023.

> S. Miksits, R. Shi, M. Gokhale, J. Wahlgren, G. Schieffer, and I. B. Peng, “Multi-level Memory-Centric Profiling on ARM Processors with ARM SPE,” in _International Workshop on Memory System, Management and Optimization (MEMO)_, 2024.

## License

NMO is licensed under the [GNU General Public License version 3](LICENSE).

Copyright 2023-2024 Jacob Wahlgren, Ivy B. Peng, Samuel Miksits, Ruimin Shi.

## Funding

The development of NMO is supported by the European Commission under the Horizon
project OpenCUBE (101092984) and the Swedish Research Council (no. 2022.03062).
