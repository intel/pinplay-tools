# DrDebug: Determinstic Rplay based Debugging with Pin
  
## Pre-requisites
Intel SDE  >= 9.44  :  ( search for "Intel SDE") 

export SDE_BUILD_KIT="path to the  local copy of the SDE Kit"

Assumption: using “bash”:  put “.” in PATH 

## Build required tools
```console
install.drdebug.sh
```
# Example : run DrDebug on PinPoints/Test/drproduct-st

## build test case and create a pinball
```console
cd ../PinPoints/Test
#Build dotproduct-st
make clean; make

INPUT=1
PROGRAM=dotproduct-st
COMMAND="./dotproduct-st"
SDE_ARCH="-skl" # AMX registers introduced in Sapphire Rapids (-spr) not handled by 'pinball2elf' yet.
#Whole Program Logging and replay using the default sde tool
$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py --pin_options -skl --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --delete --mode st --log_options="-log:fat -log:mp_mode 0 -log:mp_atomic 0" --replay_options="-replay:strace" -l -r
```
## Use the pinball with gdb_replay
### tested with sde-external-9.44.0-2024-08-22-lin, gdb 13.2, and Python 3.9.6
```console
pinball=`ls whole_program.1/*.address | sed '/.address/s///'`
$SDE_BUILD_KIT/pinplay-scripts/gdb_replay $pinball dotproduct-st
```
