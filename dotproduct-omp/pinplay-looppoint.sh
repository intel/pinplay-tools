#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
export OMP_NUM_THREADS=8
SLICESIZE=80000000
WARMUP_FACTOR=0
MAXK=5
PROGRAM=dotproduct
INPUT=1
COMMAND="./dotproduct-omp"
PCCOUNT="--pccount_regions"
WARMUP="--warmup_factor $WARMUP_FACTOR" 
GLOBAL="--global_regions"

if [ -z $PIN_ROOT ];
then
  echo "Set PIN_ROOT to point to the latest (internal)SDE kit"
  exit
fi

if [ ! -e $PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so ];
then
  echo " $PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so is missing"
  exit 1
fi

if [ ! -e $PIN_ROOT/extras/pinplay/scripts ];
then
  echo "$PIN_ROOT/extras/pinplay/scripts does not exist"
  exit 1
fi

sch="active"
if [ $# -eq 1 ];
then
 sch=$1
fi

if [ ! -e $sch.env.sh ];
then
  echo "./$sch.env.sh does not exist; using ./active.env.sh"
  sch="active"
fi
echo "source ./$sch.env.sh"
source ./$sch.env.sh

#Whole Program Logging and replay
$PIN_ROOT/extras/pinplay/scripts/pinpoints.py $GLOBAL --pintool="$PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so" --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --delete --mode mt --log_options="-log:start_address main -log:fat -dcfg -log:mp_mode 0 -log:mp_atomic 0" --replay_options="-replay:strace" -l -r 

#Profiling for LoopPoint
DCFG=`ls whole_program.$INPUT/*dcfg*.bz2`
$PIN_ROOT/extras/pinplay/scripts/pinpoints.py $GLOBAL $PCCOUNT --pintool="$PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so"  --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --mode mt -S $SLICESIZE -b --replay_options "-global_profile -emit_vectors 0 -looppoint:global_profile -looppoint:dcfg-file $DCFG -looppoint:main_image_only 0 -looppoint:loop_info $PROGRAM.$INPUT.loop_info.txt"

#Simpoint
$PIN_ROOT/extras/pinplay/scripts/pinpoints.py $GLOBAL $PCCOUNT  --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" $PCCOUNT -S $SLICESIZE $WARMUP --maxk=$MAXK --append_status -s 

#Region pinball generation and replay
$PIN_ROOT/extras/pinplay/scripts/pinpoints.py $GLOBAL $PCCOUNT  --pintool="$PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so"  --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND"  $PCCOUNT $WARMUP -S $SLICESIZE --mode mt --coop_pinball --append_status --log_options="-dcfg -dcfg:read_dcfg 1 -log:controller_log -log:controller_olog sde.looppoint.controller.txt -log:fat " --replay_options="-replay:strace" -p -R 
