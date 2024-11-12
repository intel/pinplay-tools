#!/bin/bash
ulimit -s unlimited

if [ -z $SDE_BUILD_KIT ];
then
  echo "Set SDE_BUILD_KIT to point to the latest (internal)SDE kit"
  exit
fi

if [ ! -e $SDE_BUILD_KIT/intel64/sde-global-event-icounter.so ];
then
  echo " $SDE_BUILD_KIT/intel64/sde-global-event-icounter.so is missing"
  exit 1
fi

SDE_ROOT=$SDE_BUILD_KIT

  i=1 
  b=dotproduct-st
    pgm=$b
    TCOUNT=$OMP_NUM_THREADS
    CORE=$TCOUNT"C"
    if [ ! -e  whole_program.$i ];
    then
      echo " whole_program.$i is missing"
      exit 1
    fi
    prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
    prefix=`basename $prefix`
    ddir=$prefix.Data
    if [ ! -e  whole_program.$i ];
    then
      exit
    fi
    prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
    wpb=$prefix
      #ripno=`grep "^rip" $wpb.global.log | awk '{print $2}'`
      #regfile=`echo $wpb*.0.result | sed '/result/s//reg.bz2/'`
      #rip=`bzcat $regfile| grep -e "^$ripno" | awk '{print $NF}' | head -1 | endian.sh`
    prefix=`basename $prefix`
    ppdir=$prefix.pp
    mkdir -p $ppdir
    ddir=$prefix.Data
    rsvcount=`find $ddir -name "*.CSV" | wc -l`
    if [ $rsvcount -eq 0 ];
    then
      echo "No per-regions CSV files found in $ddir"
      exit 1
    fi
      for rcsv in $ddir/*.CSV
      do
        rec=`grep "simulation" $rcsv` 
#cluster 9 from slice 423,global,10,0x4046b0,ft.C.x,0x46b0,5473634570,0x404890,ft.C.x,0x4890,4390787815,11874820,800000135,0.37778,611.963,simulation
#cluster 0 from slice 1,0,1,0x558f663242e8,dotproduct-st,0x12e8,2318635,0x558f663242a0,dotproduct-st,0x12a0,5729872,3288497,80000002,0.42857,3.000,simulation
        tid=`echo $rec | awk -F"," '{print $2}'`
        rid=`echo $rec | awk -F"," '{print $3}'`
        ridlong=`echo $rec | awk -F"," '{printf "%03d",$3}'`
        region="t"$tid"r"$rid
        rpbname=$prefix"_"$region
        startImage=`echo $rec | awk -F"," '{printf $5}'`
        endImage=`echo $rec | awk -F"," '{printf $9}'`
        startOffset=`echo $rec | awk -F"," '{printf $6}'`
        endOffset=`echo $rec | awk -F"," '{printf $10}'`
        startPC=`echo $rec | awk -F"," '{printf $4}'`
        endPC=`echo $rec | awk -F"," '{printf $8}'`
        startCount=`echo $rec | awk -F"," '{print $7}'`
        endCount=`echo $rec | awk -F"," '{print $11}'`
        haswarmup=`grep -c ",warmup" $rcsv`
        if [ $haswarmup -eq 1 ];
        then
          rec=`grep "warmup" $rcsv` 
          wstartImage=`echo $rec | awk -F"," '{printf $5}'`
          wstartOffset=`echo $rec | awk -F"," '{printf $6}'`
          wstartPC=`echo $rec | awk -F"," '{printf $4}'`
          wstartCount=`echo $rec | awk -F"," '{print $7}'`
        fi
        cfgfile=$b.$i.cfg
        #cmd=`grep command $cfgfile | sed '/command: /s///'`
        cmd="\$SDE_ROOT/intel64/nullapp"
        if [ $haswarmup -eq 1 ];
        then
COMMAND="\$SDE_ROOT/sde64 -t64 \$SDE_ROOT/intel64/sde-global-event-icounter.so -thread_count 1 -prefix $ppdir/$rpbname -controller_log -controller_olog pcevents.controller.$pgm.$i.$rid.txt -watch_addr $startPC -watch_addr $endPC -xyzzy    -warmup -control start:address:$wstartPC:count$wstartCount -control start:address:$startPC:count$startCount -control stop:address:$endPC:count$endCount -exit_on_stop -replay -replay:basename $wpb -replay:addr_trans  -- $cmd"
        else
COMMAND="\$SDE_ROOT/sde64 -t64 \$SDE_ROOT/intel64/sde-global-event-icounter.so -thread_count 1 -prefix $ppdir/$rpbname -controller_log -controller_olog pcevents.controller.$pgm.$i.$rid.txt -watch_addr $startPC -watch_addr $endPC -xyzzy  -control start:address:$startPC:count$startCount -control stop:address:$endPC:count$endCount -replay -replay:basename $wpb -replay:addr_trans  -- $cmd"
        fi
        echo "#!/bin/bash" > run.sde-eventcount.$pgm.$i.$rid.sh
        echo "export SDE_ROOT=$SDE_ROOT" >> run.sde-eventcount.$pgm.$i.$rid.sh
        echo "ulimit -s unlimited" >> run.sde-eventcount.$pgm.$i.$rid.sh
        echo "#time $COMMAND" >> run.sde-eventcount.$pgm.$i.$rid.sh
        echo "time $COMMAND" >> run.sde-eventcount.$pgm.$i.$rid.sh
        chmod +x run.sde-eventcount.$pgm.$i.$rid.sh
        echo " run.sde-eventcount.$pgm.$i.$rid.sh created"
      done
