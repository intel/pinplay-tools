#!/bin/bash
export PATH=`pwd`/scripts:$PATH
SLICESIZE=10000
WARMUPFACTOR=1
INPUT=cversion

echo "program.input.rid, weight, exp_W+R_length, act_W+R_length, slicesize, filtered_R_length, unfiltered_R_length"
    pbdir=whole_program.$INPUT
    if [ ! -e  $pbdir ];
    then
     exit 
    fi
    pb=`ls $pbdir/*.address | sed '/.address/s///'`
    ppdir=`basename $pb`".pp"
    datadir=`basename $pb`".Data"
    if [ ! -e  $ppdir ];
    then
      exit
    fi
    if ! test "$( find $ppdir -name "*.address" -print -quit)"
    then
      exit
    fi
    for r in `ls $ppdir/*.address` 
    do
      rpb=`echo $r | sed '/.address/s///'`
      rpbname=`basename $rpb`
#lbm-s.1_15218_globalr7_warmupendPC0x000401de9_warmupendPCCount3132761_warmuplength600006476_endPC0x000401de9_endPCCount93391_length30001957_multiplier18-997_007_0-01390.address
       rid=`echo $rpbname | awk -F "_" '{print $3}' | sed '/globalr/s///'`
       rec=`grep "RegionId = $rid " $datadir/*.csv`
# # RegionId = 3 Slice = 46 Icount = 1380151514 Length = 30000138 Weight = 0.05195 Multiplier = 4.000 ClusterSlicecount = 4 ClusterIcount = 120000223
      slice=`echo  $rec | awk '{print $7}'`
      weight=`echo  $rec | awk '{print $16}'`
      flag=`echo "$slice > $WARMUPFACTOR" | bc`
      wf=$slice
      if [ $flag -eq 1 ];
      then
        wf=$WARMUPFACTOR
      fi
      act=`pinballicount.sh $rpb dum`
      exp=`echo "$wf*$SLICESIZE + $SLICESIZE" | bc`
      notfiltered="NA"
      if [ -e $rpb.event_icount.0.txt ];
      then
        hasnotfiltered=`grep -c notfiltered $rpb.event_icount.0.txt`
        scount=`grep global $rpb.event_icount.0.txt | awk '
             /Start/ {print $NF}'`
        ecount=`grep global $rpb.event_icount.0.txt | awk '
             /End/ {print $NF}'`
        if [ -z "$ecount" ];
        then
          ecount=`grep global $rpb.event_icount.0.txt | awk '
             /Fini/ {print $NF}'`
        fi
        if [ $hasnotfiltered -gt 0 ];
        then
#Sim-Start global_notfiltered_icount: 147372 global_icount: 172507
          notfiltered_scount=`grep global $rpb.event_icount.0.txt | awk '
             /Start/ {print $3}'`
          notfiltered_ecount=`grep global $rpb.event_icount.0.txt | awk '
             /End/ {print $3}'`
          if [ -z "$notfiltered_ecount" ];
          then
            notfiltered_ecount=`grep global $rpb.event_icount.0.txt | awk '
             /Fini/ {print $3}'`
          fi
        fi
        if [ -z "$scount" ]  || [ -z "$ecount" ];
        then
          rlength="NF"
          notfiltered_rlength="NF"
        else
          rlength=`echo "$ecount-$scount" | bc`
          notfiltered_rlength="NA"
          if [ $hasnotfiltered -gt 0 ];
          then
            notfiltered_rlength=`echo "$notfiltered_ecount-$notfiltered_scount" | bc`
          fi
        fi
      fi
      echo "$INPUT.$rid, $weight, $exp, $act, $SLICESIZE, $notfiltered_rlength, $rlength"
   done
