Pin Record/Replay Tools

A collection of C/C++ programs and Python scripts to be used in conjunction with Intel Software Development Emulator (Intel SDE, available externally separately). The purpose is to use record/replay functionality in SDE for program analysis.


 ├── openmp
 ├── Examples
 ├── GlobalLoopPoint
 └── pinplay-scripts

openmp: Test OpenMP program sources, makefile, shell-scripts to run looppoint toolchain.

Examples:Test program sources and  makefile for building simple SDE+PinPlay tools

GlobalLoopPoint: sources for tools doing global profiling of multi-threaded programs to find representative simulation regions.

  See the HPCA-2022 paper:
  LoopPoint: Checkpoint-driven Sampled Simulation for Multi-threaded Applications
Alen Sabu (National University of Singapore), Harish Patil, Wim Heirman (Intel), Trevor E. Carlson (National University of Singapore)

pinplay-scripts: Python scripts to automate simulation region selection for SDE-based single threaded and multi-threaded programs.

Also see CGO-2021 ELFie paper: ELFies: Executable Region Checkpoints for Performance Analysis and Simulation 
  Harish Patil; Alexander Isaev; Wim Heirman; Alen Sabu; Ali Hajiabadi; Trevor E. Carlson


Coming soon: instructions for 'traditional' (single-threaded) PinPoints.

TIPS:
 1. If using PinPoints scripts, do not use "\~" to specify any paths, instead use "$HOME"
   e.g. the option “--sdehome=~/sde-external-9.33.0-2024-01-07-lin” should be “--sdehome=$HOME/sde-external-9.33.0-2024-01-07-lin”. 
