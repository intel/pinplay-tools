# PinPoints:  Instructions for running Pin+SimPoint toolchain with the following steps:
  1. Generate whole-program pinball (wpb)
  2. Replay wpb and create basic block vectors (bbvs)
  3. Use SimPoint to cluster bbvs and find representative regions 
  4. Replay wpb and use representative region specification to create region pinballs (rpb's)
  5. Replay rpbs,

#  See the latest Tutorial 
[SDE-PinPoints](Documents/SDE-PinPoints-2024.pdf)

  
## Pre-requisites
Pin kit >= 3.31 : http://pintool.intel.com  ( search for "Intel Pintool")

export PIN_ROOT="path to the local copy of the Pin kit "

Intel SDE  >= 9.44  :  ( search for "Intel SDE") 

export SDE_BUILD_KIT="path to the  local copy of the SDE Kit"

Pinball2elf repo: https://github.com/intel/pinball2elf 

export PINBALL2ELF="path to the  local clone of the pinball2elf repo"

Assumption: using “bash”:  put “.” in PATH 

## Build required tools
```console
sde-pin-build-PinPoints.sh
```
# Example : run PinPoints flow on 'dotproduct-st'

```console
cd Test
#edit sde-run.pinpoints.single-threaded.sh : change SLICESEIS/WARMUP_FACTOR/SDE_ARCH if needed
make clean; make

#Basic PinPoints : region SELECTION and region pinball generation
sde-run.pinpoints.single-threaded.sh

#Region VALIDATION
run.ROIPerf-validation.sh

#SIMULATOR STAGING
create.sniper_region_simulation_scripts.sh
```
