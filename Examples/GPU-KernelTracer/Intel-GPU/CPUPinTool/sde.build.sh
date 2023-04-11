#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
if [[  -z $SDE_BUILD_KIT ]]; then
    echo "SDE_BUILD_KIT not defined"
    exit 1
fi

if [ ! -e $SDE_BUILD_KIT/pinplay-scripts ];
then
  echo "$SDE_BUILD_KIT/pinplay-scripts does not exist"
  echo "git clone https://github.com/intel/pinplay-tools.git"
  echo "cp -r pinplay-tools/pinplay-scripts $SDE_BUILD_KIT/"
  exit 1
fi


if [[  -z $GTPIN_KIT ]]; then
    echo "GTPIN_KIT not defined : need to point to the GT-Pin 'Profilers' dir"
    exit 1
fi
if [[  -z $MBUILD ]]; then
    echo "MBUILD not defined"
    echo " mbuild clone https://github.com/intelxed/mbuild.git"
    echo " set MBUILD to point to 'mbuild' just cloned"
    exit 1
fi

rm -rf obj-intel64
export PYTHONPATH=$SDE_BUILD_KIT/pinplay-scripts:$MBUILD
./mfile.py --host-cpu x86-64 --debug

cd ../GTPinTool/build
./clean.sh
./build.sh
