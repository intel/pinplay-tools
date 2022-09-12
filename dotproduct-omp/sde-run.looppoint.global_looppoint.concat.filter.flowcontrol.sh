#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
export OMP_NUM_THREADS=8
SLICESIZE=80000000
WARMUP_FACTOR=1
MAXK=5
PROGRAM=dotproduct
INPUT=1
COMMAND="./dotproduct-omp"
PCCOUNT="--pccount_regions"
WARMUP="--warmup_factor $WARMUP_FACTOR" 
GLOBAL="--global_regions"
FLOWCONTROL="-flowcontrol:verbose 1 -flowcontrol:quantum 1000000"
PAR=3 # how many regions to process in parallel. Should have PAR*OMP_NUM_THREADS
  # cores available on the test machine

export FILTERFLAGS="-filter_exclude_lib libgomp.so.1 -filter_exclude_lib libiomp5.so"

echo "Using $COMMAND"
echo "Using $FILTERFLAGS"

if [ -z $SDE_BUILD_KIT ];
then
  echo "Set SDE_BUILD_KIT to point to the latest (internal)SDE kit"
  exit
fi

if [ ! -e $SDE_BUILD_KIT/pinplay-scripts ];
then
  echo "$SDE_BUILD_KIT/pinplay-scripts does not exist"
  cp -r ../pinplay-scripts $SDE_BUILD_KIT
fi

if [ ! -e $SDE_BUILD_KIT/pinplay-scripts/PinPointsHome/Linux/bin/simpoint ];
then
  echo "$SDE_BUILD_KIT/pinplay-scripts//PinPointsHome/Linux/bin/simpoint does not exist"
  echo " Attempting to build it ..."
  pushd $SDE_BUILD_KIT/pinplay-scripts//PinPointsHome/Linux/bin/
  make clean; make
  popd
  if [ ! -e $SDE_BUILD_KIT/pinplay-scripts/PinPointsHome/Linux/bin/simpoint ];
  then
    echo "$SDE_BUILD_KIT/pinplay-scripts//PinPointsHome/Linux/bin/simpoint does not exist"
    echo "See $SDE_BUILD_KIT/pinplay-scripts/README.simpoint"
    exit 1
  fi
fi

if [ ! -e $SDE_BUILD_KIT/intel64/sde-global-looppoint.so ];
then
  echo " $SDE_BUILD_KIT/intel64/sde-global-looppoint.so is missing"
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
#$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py $GLOBAL --pintool="sde-global-looppoint.so" --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --delete --mode mt --log_options="$FLOWCONTROL -start_address main -log:fat -dcfg -log:mp_mode 0 -log:mp_atomic 0" --replay_options="$FLOWCONTROL -replay:strace" -l -r 
#Removed $FLOWCONTROL as it at times causes hangs during whole-program logging
$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py $GLOBAL --pintool="sde-global-looppoint.so" --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --delete --mode mt --log_options="-start_address main -log:fat -dcfg -log:mp_mode 0 -log:mp_atomic 0" --replay_options="-replay:strace" -l -r 

#Profiling for LoopPoint
DCFG=`ls whole_program.$INPUT/*dcfg*.bz2`
#$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py $GLOBAL $PCCOUNT --pintool="sde-global-looppoint.so" --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --mode mt -S $SLICESIZE -b --replay_options "$FLOWCONTROL -global_profile -emit_vectors 0 $FILTERFLAGS -looppoint:global_profile -looppoint:dcfg-file $DCFG -looppoint:main_image_only 0 -looppoint:loop_info $PROGRAM.$INPUT.loop_info.txt"
#Removed $FLOWCONTROL as it at times causes hangs during whole-program logging
$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py $GLOBAL $PCCOUNT --pintool="sde-global-looppoint.so" --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" --mode mt -S $SLICESIZE -b --replay_options "-global_profile -emit_vectors 0 $FILTERFLAGS -looppoint:global_profile -looppoint:dcfg-file $DCFG -looppoint:main_image_only 0 -looppoint:loop_info $PROGRAM.$INPUT.loop_info.txt"

wpb=`ls whole_program.$INPUT/*.address | sed '/.address/s///'`
wpbname=`basename $wpb`
datadir=$wpbname.Data
bbname=$datadir/$wpbname
echo "$SDE_BUILD_KIT/pinplay-scripts/make-balanced-concat-vectors.py $bbname $OMP_NUM_THREADS"
$SDE_BUILD_KIT/pinplay-scripts/make-balanced-concat-vectors.py $bbname $OMP_NUM_THREADS
cvfile=$datadir/$wpbname.global.cv
bbfile=$datadir/$wpbname.global.bb
bkpbbfile=$datadir/$wpbname.global.bb.bkp
ls $cvfile $bbfile
if [ ! -e $bkpbbfile ];
then
   cp $bbfile $bkpbbfile
   echo "$bkpbbfile" created
else
   echo "$bkpbbfile" exists
fi

echo "Replacing $bbfile with concatenated version"
cp $cvfile $bbfile

#Simpoint
$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py $GLOBAL $PCCOUNT  --pintool="sde-global-looppoint.so"  --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND" $PCCOUNT -S $SLICESIZE $WARMUP --maxk=$MAXK --append_status -s 

#Region pinball generation and replay
#$SDE_BUILD_KIT/pinplay-scripts/sde_pinpoints.py $GLOBAL $PCCOUNT  --pintool="sde-global-looppoint.so"  --program_name=$PROGRAM --input_name=$INPUT --command="$COMMAND"  $PCCOUNT $WARMUP -S $SLICESIZE --mode mt --coop_pinball --append_status --log_options="-dcfg -dcfg:read_dcfg 1 -log:fat -log:region_id -controller_log -controller_olog looppoint.controller.txt" --replay_options="$FLOWCONTROL -replay:strace" -p -R 
#Removed $FLOWCONTROL as it at times causes hangs during whole-program logging
# Create per-region CSV files
wpb=`ls whole_program.$INPUT/*.address | sed '/.address/s///'`
wpbname=`basename $wpb`
ddir="$wpbname.Data"
pdir="$wpbname.pp"
csvfile=`ls $ddir/*global.pinpoints.csv`
$SDE_BUILD_KIT/pinplay-scripts/split.pc-csvfile.py --csv_file $csvfile
echo "SDE_BUILD_KIT  = $SDE_BUILD_KIT" > Makefile.regions
for rcsv in `ls $ddir/*.CSV`
do
  rid=`echo $rcsv | awk -F "." '{print $(NF-1)}'`
  rpbname=$wpbname"_"$rid
  #echo $rcsv $rid $rpbname
  rstr="t"$rid
  echo $rstr":" >> Makefile.regions
  echo "	\${SDE_BUILD_KIT}/sde -p -xyzzy -p -reserve_memory -p $wpb.address   -t sde-global-looppoint.so -replay -xyzzy  -replay:deadlock_timeout 0  -replay:basename $wpb -replay:playout 0  -replay:strace  -dcfg -dcfg:read_dcfg 1 -log:fat -log -xyzzy -pcregions:in $rcsv -pcregions:merge_warmup -log:basename $pdir/$rpbname -log:compressed bzip2  -log:mt 1 -- \${SDE_BUILD_KIT}/intel64/nullapp" >> Makefile.regions
  astr=$astr" "$rstr
done
echo "all:" $astr >> Makefile.regions
echo "Makefile.regions created"
echo "Running with -j $PAR"
make -j $PAR -f Makefile.regions all
for rpb in `ls $pdir/*.address`
do
  rpbname=`echo $rpb | sed '/.address/s///'`
  echo "replaying $rpbname"
  $SDE_BUILD_KIT/pinplay-scripts/replay $rpbname
done
