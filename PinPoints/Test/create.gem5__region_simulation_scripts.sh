#!/bin/bash
ulimit -s unlimited
eventsflag=`ls */*event_icount.0.txt | wc -l`
if [ $eventsflag -eq 0 ];
then
  echo "Creating .0.txt files"
  ./create.sde_bin_region_event_icount.sh
  ./runall.region-event-icounter.sh 
else
  echo "Using existing event_icount.0.txt files"
fi
  i=1 
    if [ ! -e  whole_program.$i ];
    then
      echo " whole_program.$i is missing"
      exit 1
    fi
    prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
    prefix=`basename $prefix`
    ddir=$prefix.Data
    pgm=`echo $prefix | awk -F "." '{print $1}'`
    prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
    wpb=$prefix
      #ripno=`grep "^rip" $wpb.global.log | awk '{print $2}'`
      #regfile=`echo $wpb*.0.result | sed '/result/s//reg.bz2/'`
      #rip=`bzcat $regfile| grep -e "^$ripno" | awk '{print $NF}' | head -1 | endian.sh`
    prefix=`basename $prefix`
    ppdir=$prefix.pp
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
        cfgfile=$b.$i.cfg
        rpb=`ls $ppdir/*.address | grep $region |  sed '/.address/s///'`
        cmd="./$rpb.sim.elfie"
        efile=`ls $ppdir/$rpbname.event_icount.0.txt`
        if [ ! -e $efile ];
        then
          echo "ERROR: $efile not found"
          exit 1
        fi
#Warmup-Start global_icount: 150006
#Warmup-Start tid: 0 icount 150006
#  Marker  5570164814e0  global_addrcount: 3875
#  Marker  5570164814e0 tid: 0 addrcount 3875
#  Marker  5570164814e0  global_addrcount: 3875
#  Marker  5570164814e0 tid: 0 addrcount 3875
#Sim-Start global_icount: 375033
#Sim-Start tid: 0 icount 375033
#  Marker  5570164814e0  global_addrcount: 4527
#  Marker  5570164814e0 tid: 0 addrcount 4527
#  Marker  5570164814e0  global_addrcount: 4527
#  Marker  5570164814e0 tid: 0 addrcount 4527
#Sim-End global_icount: 450045
#Sim-End tid: 0 icount 450045
#  Marker  5570164814e0  global_addrcount: 10778
#  Marker  5570164814e0 tid: 0 addrcount 10778
#  Marker  5570164814e0  global_addrcount: 10778
#  Marker  5570164814e0 tid: 0 addrcount 10778
        haswarmup=`grep -c ",warmup" $rcsv`
        if [ $haswarmup -eq 1 ];
        then
          addr1_start_count=`grep -A2  "Warmup-Start tid: 0" $efile | grep "tid: 0 addrcount" | awk '{print $NF}'`
        addr1_end_count=`grep -A2  "Sim-Start tid: 0" $efile | grep "tid: 0 addrcount" | awk '{print $NF}'`
        start_rel_count=`echo $addr1_end_count - $addr1_start_count | bc`
        else
            start_rel_count="1"
        fi
        addr2_start_count=`grep -A4  "Sim-Start tid: 0" $efile | grep "tid: 0 addrcount" | tail -1 | awk '{print $NF}'`
        addr2_end_count=`grep -A4  "Sim-End tid: 0" $efile | grep "tid: 0 addrcount"  | tail -1 | awk '{print $NF}'`
        end_rel_count=`echo $addr2_end_count - $addr2_start_count | bc`
        echo "Creating  gem5.$pgm.$i.$rid.conf.py"
        cat ../gem5_example_elfie_conf.py.txt | awk -v elfie_path=$cmd -v startpc=$startPC -v startpccount=$start_rel_count -v endpc=$endPC -v endpccount=$end_rel_count '
         /ELFIE_PATH=/ {printf "ELFIE_PATH=\"%s\"\n", elfie_path; next}
         /STARTPC=/ {printf "STARTPC=\"%s\"\n", startpc; next}
         /STARTPCCOUNT=/ {printf "STARTPC=%d\n", startpccount; next}
         /ENDPC=/ {printf "ENDPC=\"%s\"\n", endpc; next}
         /ENDPCCOUNT=/ {printf "ENDPCCOUNT=%d\n", endpccount; next}
         {print $0}
        ' >  gem5.$pgm.$i.$rid.conf.py
      done
