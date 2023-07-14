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
    cp -r  ReducedInstLib $SDE_BUILD_KIT//pinkit/source/tools/InstLib
fi

if [[  -z $PINBALL2ELF ]]; then
    echo "PINBALL2ELF not defined"
    echo "Point to a clone of https://github.com/intel/pinball2elf"
    exit 1
fi

echo "For older SDE ( sde-external-9.14 or older) set 'export CFLAGS=-DOLDSDE'"
echo "*********************"
sleep 1
cd ./EventCounter
make clean TARGET=ia32
make clean TARGET=intel64
make build TARGET=ia32 
make build TARGET=intel64
make clean TARGET=ia32
make clean TARGET=intel64
cd -

cd ./Profiler/DCFG
make clean TARGET=ia32 
make clean TARGET=intel64 
# for older SDE ( sde-external-9.14 or older) use "export CFLAGS=-DOLDSDE"
make TARGET=ia32 CFLAGS=$CFLAGS
make clean TARGET=ia32 
make TARGET=intel64 CFLAGS=$CFLAGS
make clean TARGET=intel64 
cd -

cd Drivers/BarrierPoint/
make clean TARGET=ia32 
make build TARGET=ia32 CFLAGS=$CFLAGS
make clean TARGET=ia32
make clean TARGET=intel64 
make build TARGET=intel64 CFLAGS=$CFLAGS
make clean TARGET=intel64
cd -

cd Drivers/FlowControlLoopPoint/
make clean TARGET=ia32 
make build TARGET=ia32  CFLAGS=$CFLAGS
make clean TARGET=ia32 
make clean TARGET=intel64 
make build TARGET=intel64  CFLAGS=$CFLAGS
make clean TARGET=intel64 
cd -

find . -name "obj-i*" | xargs rm -r
