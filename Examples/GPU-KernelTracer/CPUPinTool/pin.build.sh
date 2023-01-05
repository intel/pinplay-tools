#!/bin/bash
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
if [ -z $PIN_ROOT ];
then
  echo "Set PIN_ROOT to point to the latest Pin kit (from pintool.intel.com)."
  exit 1
fi
if [ -z $GTPIN_KIT ];
then
  echo "Set GTPIN_KIT to point to the 'Profilers' directory from the latest GT-Pin kit ( see from https://www.intel.com/content/www/us/en/developer/articles/tool/gtpin.html )."
  exit 1
fi
make clean; make
cd ../GTPinTool/build
./clean.sh
./build.sh
