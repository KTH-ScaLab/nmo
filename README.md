# NMO: Non-homogeneous Memory Observatory

NMO is a memory-centric profiling tool suite for heterogeneous memory.

## Usage

NMO can be built using Make. It depends on PFM and libnuma.

    make -j

This prototype version is tested on the Intel Skylake-X architecture.

Documentation of NMO is available in the [NMO User Manual](USAGE.md).

## Publications

> J. Wahlgren, G. Schieffer, M. Gokhale, and I. B. Peng, “A Quantitative Approach
for Adopting Disaggregated Memory in HPC Systems,” in _Proceedings of the
International Conference for High Performance Computing, Networking, Storage
and Analysis (SC23)_, 2023.

## License

NMO is licensed under the [GNU General Public License version 3](LICENSE).

Copyright 2023 Jacob Wahlgren and Ivy B. Peng.

## Funding

The development of NMO is supported by the European Commission under the Horizon
project OpenCUBE (101092984) and the Swedish Research Council (no. 2022.03062).
