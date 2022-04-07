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

sch="active"
if [ $# -eq 1 ];
then
 sch=$1
fi

SDE_ROOT=$SDE_BUILD_KIT

if [ ! -e $sch.env.sh ];
then
  echo "./$sch.env.sh does not exist; using ./active.env.sh"
  sch="active"
fi
echo "source ./$sch.env.sh"
source ./$sch.env.sh

  i=1 
  b=dotproduct
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
      for rcvs in $ddir/*.CSV
      do
        rec=`grep "simulation" $rcvs` 
#cluster 9 from slice 423,global,10,0x4046b0,ft.C.x,0x46b0,5473634570,0x404890,ft.C.x,0x4890,4390787815,11874820,800000135,0.37778,611.963,simulation
        tid=`echo $rec | awk -F"," '{print $2}'`
        rid=`echo $rec | awk -F"," '{print $3}'`
        ridlong=`echo $rec | awk -F"," '{printf "%03d",$3}'`
        region=$tid"r"$rid
        rpbname=$prefix"_"$region
        startPC=`echo $rec | awk -F"," '{printf $4}'`
        endPC=`echo $rec | awk -F"," '{printf $8}'`
        startCount=`echo $rec | awk -F"," '{print $7}'`
        endCount=`echo $rec | awk -F"," '{print $11}'`
        cfgfile=$b.$i.cfg
        cmd=`grep command $cfgfile | sed '/command: /s///'`
COMMAND="\$SDE_ROOT/sde64 -t64 \$SDE_ROOT/intel64/sde-global-event-icounter.so -thread_count $OMP_NUM_THREADS -prefix $ppdir/$rpbname -controller_log -controller_olog pcevents.controller.$pgm.$i.$rid.txt -watch_addr $startPC -watch_addr $endPC -xyzzy  -control start:address:$startPC:count$startCount:global -control stop:address:$endPC:count$endCount:global -- $cmd"
        echo "#!/bin/bash" > run.sde-eventcount.$pgm.$i.$rid.sh
        echo "export SDE_ROOT=$SDE_ROOT" >> run.sde-eventcount.$pgm.$i.$rid.sh
        echo "ulimit -s unlimited" >> run.sde-eventcount.$pgm.$i.$rid.sh
        cat $sch.env.sh   >> run.sde-eventcount.$pgm.$i.$rid.sh
        echo "time $COMMAND" >> run.sde-eventcount.$pgm.$i.$rid.sh
        chmod +x run.sde-eventcount.$pgm.$i.$rid.sh
        echo " run.sde-eventcount.$pgm.$i.$rid.sh created"
      done
