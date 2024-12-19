#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
# uses $SDE_BUILD_KIT/intel64/sde-pinball-sysstate.so
# Build instructions:
# cd $PINBALL2ELF/pintools/PinballSYSState
#  make clean; make 
PROGRAM=dotproduct
INPUT=1

if [ -z $PIN_ROOT ];
then
  echo "Set PIN_ROOT to point to the latest Pin kit"
  exit 1
fi

if [ -z $PINBALL2ELF ];
then
  echo "Set PINBALL2ELF to point to  the local pinball2elf sources cloned from https://github.com/intel/pinball2elf"
  exit 1
fi

if [ ! -e $PINBALL2ELF/src/pinball2elf ];
then
  echo " $PINBALL2ELF/src/pinball2elf is missing"
  exit 1
fi

wpb=`ls whole_program.$INPUT/*.address | sed '/.address/s///'`
wpbname=`basename $wpb`
ddir="$wpbname.Data"
pdir="$wpbname.pp"
for rpb in `ls $pdir/*.address`
do
  rpbname=`echo $rpb | sed '/.address/s///'`
  echo "running pinball2elf.sim.sh for $rpbname"
  $PINBALL2ELF/scripts/pinball2elf.sim.sh $rpbname foo 
done
