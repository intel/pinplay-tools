#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause

if [[  -z $SDE_BUILD_KIT ]]; then
    echo "SDE_BUILD_KIT not defined"
    exit 1
fi

cd ./Profiler/DCFG
make clean
make TARGET=ia32
make TARGET=intel64
cd -

cd Drivers/FlowControlLoopPoint/
make clean
make TARGET=ia32 
make build TARGET=ia32 
make clean TARGET=ia32 
make TARGET=intel64 
make build TARGET=intel64 
make clean TARGET=intel64 
cd -

find . -name "obj-i*" | xargs rm -r
