#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
# uses $SDE_BUILD_KIT/intel64/sde-pinball-sysstate.so
# Build instructions:
# cd $PINBALL2ELF/pintools/PinballSYSState
#  make clean; make 
PROGRAM=dotproduct-st
INPUT=1

wpb=`ls whole_program.$INPUT/*.address | sed '/.address/s///'`
wpbname=`basename $wpb`
ddir="$wpbname.Data"
pdir="$wpbname.pp"
for e in `find $pdir  -name '*.elfie'`
do
  echo "running $e under 'perf stat -e instructions:u'"
  edir=`dirname $e`
  enm=`basename $e`
  pushd $edir
  time perf stat -e instructions:u $enm
  popd
done
