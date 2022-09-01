#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
export PATH=$PIN_ROOT/extras/pinplay/scripts/:$PATH
export PATH=`pwd`/scripts:$PATH
export OMP_NUM_THREADS=8
#export SPINFLAGS="-spinloop:start_SSC 0x4376 -spinloop:end_SSC 0x4377 "
export FILTERFLAGS="-filter_exclude_rtn kmp_wait_template -filter_exclude_rtn  kmp_wait_4"
export SUFFIX=""
INPUT=cversion
if [ -z $PIN_ROOT ];
then
  echo "PIN_ROOT not set"
  exit
fi
if [ ! -e $PIN_ROOT/extras/dcfg/bin/intel64/global_event_icounter.so ];
then
  echo "$PIN_ROOT/extras/dcfg/bin/intel64/global_event_icounter.so not available"
  exit
fi
    pbdir=whole_program.$INPUT
    if [ ! -e  $pbdir ];
    then
      echo "No  $pbdir"
      exit
    fi
    pb=`ls $pbdir/*.address | sed '/.address/s///'`
    ppdir=`basename $pb`".pp"
    datadir=`basename $pb`".Data"
    dcfg="$pb.dcfg.json.bz2"
    tcount=`ls "$pb"*result | wc -l`
    OMP_NUM_THREADS=$tcount
    echo "#!/bin/bash" > run.pccount_event_icount.$INPUT.sh
    echo "export PIN_ROOT=$PIN_ROOT" >> run.pccount_event_icount.$INPUT.sh
    echo "export PATH=\$PIN_ROOT/extras/pinplay/scripts/:\$PATH" >> run.pccount_event_icount.$INPUT.sh
      for rpb in $ppdir/*.address
      do
        prefix=`echo $rpb | sed '/.address/s///'`
        tcount=`ls $prefix.*result | wc -l`
        rpbname=`basename $prefix`
#lbm-s.1_5027_globalr6_warmupendPC0x153bba2c49e7_warmupendPCCount379734_warmuplength209442439_endPC0x153bba2c49e7_endPCCount158861_length30289434_multiplier309-476_006_0-08024
        region=`echo $rpbname | awk -F"_" '{print $3}'`
        inpno=`echo $rpbname | awk -F"_" '{print $1}' | awk -F "." '{print $2}'`
        wPC=`echo $rpbname | awk -F"_" '{print $4}' | sed '/warmupendPC/s///'`
        wcnt=`echo $rpbname | awk -F"_" '{print $5}' | sed '/warmupendPCCount/s///'`
        sPC=`echo $rpbname | awk -F"_" '{print $7}' | sed '/endPC/s///'`
        scnt=`echo $rpbname | awk -F"_" '{print $8}' | sed '/endPCCount/s///'`
        if [ $wcnt != "0" ];
        then
          WATCHLIST="-watch_addr $sPC"
          if [ "$wPC" != "$sPC" ];
          then
            WATCHLIST="-watch_addr $wPC -watch_addr $sPC"
          fi
          echo "\$PIN_ROOT/pin -t \$PIN_ROOT/extras/dcfg/bin/intel64/global_event_icounter.so -thread_count $tcount  -prefix $ppdir/$rpbname$SUFFIX $FILTERFLAGS   -pinplay:control start:address:$wPC:count$wcnt:global,stop:address:$sPC:count$scnt:global $WATCHLIST -exit_on_stop -replay -xyzzy -replay:deadlock_timeout 0 -replay:addr_trans -replay:basename $ppdir/$rpbname -- \$PIN_ROOT/extras/pinplay/bin/intel64/nullapp" >> run.pccount_event_icount.$INPUT.sh 
        else
          echo "\$PIN_ROOT/pin -t \$PIN_ROOT/extras/dcfg/bin/intel64/global_event_icounter.so -thread_count $tcount  -prefix $ppdir/$rpbname$SUFFIX $FILTERFLAGS   -pinplay:control start:icount:1:global,stop:address:$sPC:count$scnt:global -watch_addr $sPC  -exit_on_stop -replay -xyzzy -replay:deadlock_timeout 0 -replay:addr_trans -replay:basename $ppdir/$rpbname -- \$PIN_ROOT/extras/pinplay/bin/intel64/nullapp" >> run.pccount_event_icount.$INPUT.sh 
        fi
      done
    chmod +x run.pccount_event_icount.$INPUT.sh
    echo " run.pccount_event_icount.$INPUT.sh created"
