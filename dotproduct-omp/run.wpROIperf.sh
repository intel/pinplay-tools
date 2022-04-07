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
  i=1 
  pgm=dotproduct
  b=$pgm
  prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
  if [ ! -e  whole_program.$i ];
  then
      echo " whole_program.$i is missing"
      exit 1
  fi
  prefix=`ls whole_program.$i/*.address | sed '/.address/s///'`
  wpb=$prefix
  cfgfile=$b.$i.cfg
  cmd=`grep command $cfgfile | sed '/command: /s///'`
COMMAND="$PIN_ROOT/pin -t $ROIPERF -probecontrol:in probe.in -probecontrol:verbose 1 -verbose -perfout perf.$i.wp.0.perf.txt -- $cmd"
  echo "#!/bin/bash" > run.wpbinperf.sh
  echo "export PIN_ROOT=$PIN_ROOT" >> run.wpbinperf.sh
  echo "export ROIPERF_PERFLIST=\"0:0,0:1\"" >> run.wpbinperf.sh
  echo "ulimit -s unlimited" >> run.wpbinperf.sh
  cat $sch.env.sh  >> run.wpbinperf.sh
  echo "time $COMMAND" >> run.wpbinperf.sh
  chmod +x run.wpbinperf.sh
  rm probe.in
  echo "start:main:1" > probe.in
  echo "stop:exit:1" >> probe.in
  echo "~/bin/clean_slate"
  ~/bin/clean_slate
  export ROIPERF_PERFLIST="0:0,0:1"
  pwd
  echo "perf counting  ROIPERF_PERFLIST $ROIPERF_PERFLIST"
  for run in 1 2 3 4 5 6 7 8
  do
        echo "RUN: $run $b.$i wp bin "
        time ./run.wpbinperf.sh
        mv perf.$i.wp.0.perf.txt  $wpb.0.BIN.perf.txt.$run
        cat $wpb.0.BIN.perf.txt.$run
  done
