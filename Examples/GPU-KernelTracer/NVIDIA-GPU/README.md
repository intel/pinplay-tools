# Overview
NVBit is NVDIA's GT-Pin-like instrumentation framework.
It uses the LD_PRELOAD trick to get into target application.
Luckily, that trick continues to work with a Pin-tool run of the target application so CPU-Pin + NVBit combination just works out of the box.

For CPU-Pin + NVBit combination, we continue to use LD_PRELOAD and in addition the CPU-Pin tool explicitly dlopens an NVBit tool shared library.

NVBitHandler.H looks for a function named ""NVBitShimRegisterCallbacks" in the NVBit tool library specied and uses it to register callbacks on kernel events.

# Build/Use
The setup works with x86 CPUs (uses Pin) and NVDA GPUs (using NVBit) only.

## Pre-requisites:
1.  Pin or SDE kit
   - For Pin build:
    - Set ***PIN_ROOT*** to point to the latest [Pin kit](https://pintool.intel.com)
  ***OR***
   - For SDE build:
    - Set ***SDE_BUILD_KIT*** to point to an [SDE kit (9.14 or later)](https://www.intel.com/content/www/us/en/developer/articles/tool/software-development-emulator.html)
    - Set ***MBUILD*** to mbuild sources cloned from https://github.com/intelxed/mbuild.git

2. Set ***NVBIT_KIT*** to point to the 'NVBit-X.Y.Z' directory from the latest [NVBit kit](https://github.com/NVlabs/NVBit/releases)

## How to build:
```
cd CPUPinTool
./pin.build.sh # Pin-based build : both Probe and JIT mode supported
```
  ***OR***
```
./sde.build.sh # SDE-based build : only JIT mode supported
```

## How to run:
set ***PINPLAYTOOLS*** __<path to pinplay-tools >__
### WARNING: do not use SDE-built tool with Pin or vice versa

### Pin-based run

```
LD_PRELOAD=$PINPLAYTOOLS/Examples/GPU-KernelTracer/NVIDIA-GPU/NVBitTool/NVBitTool.so $PIN_ROOT/pin -t $PINPLAYTOOLS/Examples/GPU-KernelTracer/NVIDIA-GPU/CPUPinTool/obj-intel64/xpu-pin-nvbit-handler.so -nvbittool $PINPLAYTOOLS/Examples/GPU-KernelTracer/NVIDIA-GPU/NVBitTool/NVBitTool.so -- hellocuda
```

### SDE-based run
```
$SDE_BUILD_KIT/sde64 -env LD_PRELOAD $PINPLAYTOOLS/Examples/GPU-KernelTracer/NVIDIA-GPU/NVBitTool/NVBitTool.so -t64 $PINPLAYTOOLS/Examples/GPU-KernelTracer/NVIDIA-GPU/CPUPinTool/obj-intel64/xpu-pin-nvbit-handler.so -nvbittool $PINPLAYTOOLS/Examples/GPU-KernelTracer/NVIDIA-GPU/NVBitTool/NVBitTool.so -- hellocuda
```
###  Combining mpirun with XPU-Analysis
The suggested command line is this:
```
  mpirun <mpirun args> pin/sde tool-invocation -- application <args>
```
Howerver, use of LD_PRELOAD in the command above is tricky. Here's one way to get mpirun+LD_PRELOAD+pin/sde combination to work.
Create a script apprun.sh:
```
#!/bin/bash
LD_PRELOAD=<....> application <args>
```
If using SDE:
```
  mpirun <mpirun args> sde64 -t64 path-to-sde_tool -- apprun.sh
```
If using Pin:
```
  mpirun <mpirun args> pin -follow_execv -t path-to-pin_tool -- apprun.sh
```
Note the use of '-follow_execv' in the Pin case (the sde driver sets that flag by default when it invokes the underlying pin driver).
