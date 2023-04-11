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
set ***LIT2ELF*** __<path to  applications.tracing.lit.lit2elf>__
### WARNING: do not use SDE-built tool with Pin or vice versa

### Pin-based run

```
LD_PRELOAD=$LIT2ELF/Tools/XPU-Pin/NvidiaGPU/NVbitTool/NVbitTool.so $PIN_ROOT/pin -t $LIT2ELF/Tools/XPU-Pin/NvidiaGPU/CPUPinTool/obj-intel64/xpu-pin-nvbit-handler.so -nvbittool LIT2ELF/Tools/XPU-Pin/NvidiaGPU/NVbitTool/NVbitTool.so -- hellocuda
```

### SDE-based run
```
$SDE_BUILD_KIT/sde64 -env LD_PRELOAD $LIT2ELF/Tools/XPU-Pin/NvidiaGPU/NVbitTool/NVbitTool.so -t64 $LIT2ELF/Tools/XPU-Pin/NvidiaGPU/CPUPinTool/obj-intel64/xpu-pin-nvbit-handler.so -nvbittool $LIT2ELF/Tools/XPU-Pin/NvidiaGPU/NVbitTool/NVbitTool.so -- hellocuda
```

