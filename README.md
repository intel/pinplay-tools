Pin Record Replay Tools

A collection of C/C++ programs and Python scripts to be used in conjunction with Intel Software Development Emulator (Intel SDE, available externally separately). The purpose is to use record/replay functionality in SDE for program analysis.

.
├── dotproduct-omp
├── Examples
├── GlobalLoopPoint
└── pinplay-scripts

dotproduct-omp: Test OpenMP program sources, makefile, shell-scripts to run looppoint toolchain.

Examples:Test program sources and  makefile for building simple SDE+PinPlay tools

GlobalLoopPoint: sources for tools doing global profiling of multi-threaded programs to find representative simulation regions.
See the HPCA-2022 paper:
  LoopPoint: Checkpoint-driven Sampled Simulation for Multi-threaded Applications
Alen Sabu (National University of Singapore), Harish Patil, Wim Heirman (Intel), Trevor E. Carlson (National University of Singapore)

pinplay-scripts: Python scripts to automate simulation region selection for SDE-based single threaded and multi-threaded programs.
