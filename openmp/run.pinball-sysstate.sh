#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
# uses $SDE_BUILD_KIT/intel64/sde-pinball-sysstate.so
# Build instructions:
# cd $PINBALL2ELF/pintools/PinballSYSState
#  make clean; make 
PROGRAM=dotproduct-st
INPUT=1
COMMAND="./dotproduct-st"
SDE_ARCH="-skl" # AMX registers introduced in Sapphire Rapids (-spr) not handled by 'pinball2elf' yet.

if [ -z $SDE_BUILD_KIT ];
then
  echo "Set SDE_BUILD_KIT to point to the latest (internal)SDE kit"
  exit 1
fi

if [ ! -e $SDE_BUILD_KIT/intel64/sde-pinball-sysstate.so ];
then
  echo " $SDE_BUILD_KIT/intel64/sde-pinball-sysstate.so is missing"
  echo "   See build instructions above"
  exit 1
fi

wpb=`ls whole_program.$INPUT/*.address | sed '/.address/s///'`
wpbname=`basename $wpb`
ddir="$wpbname.Data"
pdir="$wpbname.pp"
for rpb in `ls $pdir/*.address`
do
  rpbname=`echo $rpb | sed '/.address/s///'`
  echo "running SYSSTATE for $rpbname"
  $SDE_BUILD_KIT/sde64 $SDE_ARCH -t64 $SDE_BUILD_KIT/intel64/sde-pinball-sysstate.so -replay -replay:addr_trans -replay:basename $rpbname -sysstate:out $rpb -- $SDE_BUILD_KIT/intel64/nullapp 
done
