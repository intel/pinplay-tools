#!/bin/bash
if [[  -z $SDE_BUILD_KIT ]]; then
    echo "SDE_BUILD_KIT not defined"
    exit 1
fi

if [ ! -e $SDE_BUILD_KIT/pinplay-scripts ];
then
  echo "$SDE_BUILD_KIT/pinplay-scripts does not exist"
  echo "clone https://github.com/intel/pinplay-tools.git"
  echo " cp pinplay-tools/pinplay-scripts -scripts $SDE_BUILD_KIT"
fi

if [ -z $CUDA_LIB ];
then
  echo "Put CUDE compiler/runtime in your environment: put the location on 'nvcc' in your PATH and set CUDA_LIB, CUDA_INC"
  exit 1
fi
if [ -z $NVBIT_KIT ];
then
  echo "Set NVBIT_KIT to point to the latest NVBit kit from https://github.com/NVlabs/NVBit/releases"
  exit 1
fi
if [[  -z $MBUILD ]]; then
    echo "MBUILD not defined"
    echo " git clone https://github.com/intelxed/mbuild.git"
    echo " set MBUILD to point to 'mbuild' just cloned"
    exit 1
fi

export PYTHONPATH=$SDE_BUILD_KIT/pinplay-scripts:$MBUILD
./mfile.py --host-cpu x86-64 --debug
