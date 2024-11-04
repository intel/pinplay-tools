#!/bin/bash
ulimit -s unlimited
if [ -z $PIN_ROOT ];
then
  echo "Set PIN_ROOT to point to the latest Pin kit"
  exit
fi

if [ -z $PINBALL2ELF ];
then
  echo "Set PINBALL2ELF to point to the latest pinball2elf code (https://github.com/intel/pinball2elf.git)"
  exit
fi

ROIPERF=$PINBALL2ELF/pintools/ROIProbe/obj-intel64/ROIperf.so
if [ ! -e $ROIPERF ];
then
  echo " $ROIPERF is missing"
  exit 1
fi

  i=1 
  pgm=dotproduct-st
  b=$pgm
  if [ ! -e  whole_program.$i ];
  then
      echo " whole_program.$i is missing"
      exit 1
  fi
    prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
    wpb=$prefix
    prefix=`basename $prefix`
    ppdir=$prefix.pp
    ddir=$prefix.Data
    rsvcount=`find $ddir -name "*.CSV" | wc -l`
    if [ $rsvcount -eq 0 ];
    then
      echo "No per-regions CSV files found in $ddir"
      exit 1
    fi
      for rcvs in $ddir/*.CSV
      do
        rec=`grep "RegionId" $rcvs | grep -v Warmup` 
## RegionId = 10 Slice = 423 Icount = 338400033004 Length = 800000135 Weight = 0.37778 Multiplier = 611.963 ClusterSlicecount = 612 ClusterIcount = 489657278048
        rid=`echo $rec | awk '{print $4}'`
        ridlong=`echo $rec | awk -F"," '{printf "%03d",$3}'`
        rstarticount=`echo $rec | awk '{print $10}'`
        rlength=`echo $rec | awk '{print $13}'`
        rendicount=`echo $rstarticount+$rlength | bc`
        tid="t0"
        region=$tid"r"$rid
        rpbname=$prefix"_"$region
        srec=`grep "simulation" $rcvs`
#cluster 9 from slice 423,0,10,0x4046b0,ft.C.x,0x46b0,5473634570,0x404890,ft.C.x,0x4890,4390787815,11874820,800000135,0.37778,611.963,simulation
        PC=`echo $srec | awk -F"," '{print $8}'`
        sePC=`echo $PC | sed '/0x/s///' | tr '[:lower:]' '[:upper:]'  | awk '{print "obase=10;ibase=16;"$1}' | bc  | awk '{printf "0x%09x\n",$1}'`
        seCount=`echo $srec | awk -F"," '{print $12}'`
        sLength=`echo $srec | awk -F"," '{print $13}'`
        weight=`echo $srec | awk -F"," '{print $14}' | sed '/\./s//-/'`
        mult=`echo $srec | awk -F"," '{print $15}' | sed '/\./s//-/'`
        haswarmup=`grep -c -i warmup $rcvs`
        wPC="0x000000"
        wCount=0
        wLength=0
        if [ $haswarmup -gt 0 ];
        then
          wrec=`grep "warmup" $rcvs` 
#Warmup for regionid 10,0,21,0x4049ab,ft.C.x,0x49ab,2177531858,0x4046b0,ft.C.x,0x46b0,5473634570,29666648,1600000124,0.00000,0.000,warmup:10
        PC=`echo $wrec | awk -F"," '{print $8}'`
        wePC=`echo $PC | sed '/0x/s///' | tr '[:lower:]' '[:upper:]'  | awk '{print "obase=10;ibase=16;"$1}' | bc  | awk '{printf "0x%09x\n",$1}'`
          weCount=`echo $wrec | awk -F"," '{print $12}'`
          weABSPC=$wePC
          weABSCount=`echo $wrec | awk -F"," '{print $11}'`
          wLength=`echo $wrec | awk -F"," '{print $13}'`
        else
          weABSPC=$rip # initial PC in the wpb
          weABSCount=1
        fi
        fullrpbname=$rpbname"_warmupendPC"$wePC"_warmupendPCCount"$weCount"_warmupendlength"$wLength"_endPC"$sePC"_endPCCount"$seCount"_length"$sLength"_multiplier"$mult"_"$ridlong"_"$weight
        rpbname=$prefix"_"$region
        efile=`ls $ppdir/$rpbname.event_icount.0.txt`
        if [ ! -e $efile ];
        then
          echo "ERROR: $efile not found"
          exit 1
        fi
        tid0sicount=`grep "Sim-Start tid: 0" $efile | awk '{print $NF}'`
        tid0eicount=`grep "Sim-End tid: 0" $efile | awk '{print $NF}'`
        rm probe.in
    #echo "start:main:1" > probe.in
    #echo "stop:exit:1" >> probe.in
    echo "istart:$tid0sicount" >> probe.in
    echo "istop:$tid0eicount" >> probe.in

    cfgfile=$b.$i.cfg
    cmd=`grep command $cfgfile | sed '/command: /s///'`
#COMMAND="\$PIN_ROOT/pin -t \$PIN_ROOT/intel64/ROIperf.so -probecontrol:in probe.in -probecontrol:verbose -enable_on_start -perfout $ppdir/$rpbname.0.BIN.perf.txt -- $cmd"
COMMAND="\$PIN_ROOT/pin -t $ROIPERF -probecontrol:in probe.in -probecontrol:verbose -perfout $ppdir/$rpbname.0.BIN.perf.txt -- $cmd"
      echo "#!/bin/bash" > run.regionbinperf.sh
      echo "export PIN_ROOT=$PIN_ROOT" >> run.regionbinperf.sh
      echo "export ROIPERF_PERFLIST=\"0:0,0:1\"" >> run.regionbinperf.sh
      echo "ulimit -s unlimited" >> run.regionbinperf.sh
      cat $sch.env.sh  >> run.regionbinperf.sh
      echo "time $COMMAND" >> run.regionbinperf.sh
      chmod +x run.regionbinperf.sh
export ROIPERF_PERFLIST="0:0,0:1"
      echo "run.regionbinperf.sh created for $pgm.$i.$rid"
      echo "~/bin/clean_slate"
      ~/bin/clean_slate
      pwd
      #for run in 1 2 3 4 5 6 7 8
      for run in 1 2 3 4 5 6 7 8
      do
        echo "RUN: $run $b.$i region $rid run.regionbinperf.sh"
        time ./run.regionbinperf.sh
        cp  $ppdir/$rpbname.0.BIN.perf.txt $ppdir/$rpbname.0.BIN.perf.txt.$run
        cat $ppdir/$rpbname.0.BIN.perf.txt.$run
      done
    done
