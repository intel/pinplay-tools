#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
if [ -z $PIN_ROOT ];
then
  echo "Set PIN_ROOT to point to the latest Pin kit (from pintool.intel.com)."
  exit 1
fi
if [ -z $CUDA_LIB ];
then
  echo "Put CUDE compiler/runtime in your environment: put the location on 'nvcc' in your PATH and set CUDA_LIB, CUDA_INC"
  #exit 1
fi
if [ -z $NVBIT_KIT ];
then
  echo "Set NVBIT_KIT to point to the latest NVBit kit from https://github.com/NVlabs/NVBit/releases"
  exit 1
fi
make clean; make
cd ../NVBitTool
make clean; make
