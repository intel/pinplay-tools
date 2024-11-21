#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

if [[  -z $SDE_BUILD_KIT ]]; then
    echo "SDE_BUILD_KIT not defined"
    exit 1
fi

if [[  ! -e $SDE_BUILD_KIT//pinkit/source/tools/InstLib ]]; then
    echo "$SDE_BUILD_KIT//pinkit/source/tools/InstLib does not exist"
    echo " ... trying to workaround.."
    cp -r  ../GlobalLoopPoint/ReducedInstLib $SDE_BUILD_KIT//pinkit/source/tools/InstLib
fi

if [[  -z $PIN_ROOT ]]; then
    echo "PIN_ROOT not defined"
    exit 1
fi

if [[  -z $PINBALL2ELF ]]; then
    echo "PINBALL2ELF not defined"
    echo "Point to a clone of https://github.com/intel/pinball2elf"
    exit 1
fi

# PinPoints phase 1: SELECTION: Tools for region selction and region pinball generation

cd $SDE_BUILD_KIT/pinkit/sde-example/example
make TARGET=intel64 clean;  make TARGET=intel64
cp obj-intel64/pcregions_control.so $SDE_BUILD_KIT/intel64
cd -

# PinPoints phase 2: VALIDATION:  Tools for region validation with ROIPERF
echo "For older SDE ( sde-external-9.14 or older) set 'export CFLAGS=-DOLDSDE'"
echo "*********************"
sleep 1
## EventCounter
cd ../GlobalLoopPoint/EventCounter
make clean TARGET=ia32
make clean TARGET=intel64
make build TARGET=ia32 
make build TARGET=intel64
make clean TARGET=ia32
make clean TARGET=intel64
cd -

## ROIPerf
### pinball2elf and perf libraries
cd $PINBALL2ELF/src
make clean; make all
cd -

cd $PINBALL2ELF/pintools/ROIProbe
make clean; make
cp obj-intel64/pcregions_control.so $SDE_BUILD_KIT/intel64
cd -

#PinPoints phase 3: SIMULATOR STAGING Tools for elfie generation
## pinball2elf 
cd $PINBALL2ELF/src
make clean; make all

cd $PINBALL2ELF/pintools/PinballSYSState
make clean; make
