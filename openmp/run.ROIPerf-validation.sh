#!/bin/bash
./run.wpROIperf.sh

./create.sde_pinball_region_event_icount.sh
./runall.region-event-icounter.sh 
./runall.rgnbinperf.sh

wpb="whole_program.1"
ppdir=`ls | grep '\.pp' | awk "{print $NF}"`
datadir=`ls | grep '.Data' | awk "{print $NF}"`

./extrapolate.py -w $wpb -r $ppdir -d $datadir
