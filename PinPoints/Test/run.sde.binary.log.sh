#!/bin/bash
export SDE_ROOT=/user/hgpatil/latest-sde
ulimit -s unlimited

#Start: pc : 0x55d2021d82b0 image: dotproduct-st offset: 0x12b0 absolute_count: 2692 source-info: Unknown:0
#End:  300,000 instructions after Start

time $SDE_ROOT/sde64 -controller_log -controller_olog region.binary -control start:address:dotproduct-st+0x12b0:count2692 -control stop:icount:300000 -log -log:basename pinball.region.binary/rpb -- dotproduct-st
