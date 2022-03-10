#!/bin/bash
#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
for pb in `ls *.pp/*.address`
do
 prefix=`echo $pb | sed '/.address/s///'`
 $PIN_ROOT/pin -xyzzy  -virtual_segments 1 -reserve_memory $pb -t $PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so -replay -xyzzy -replay:deadlock_timeout 0 -replay:basename $prefix -sysstate:out $prefix -- $PIN_ROOT/extras/pinplay/bin/intel64/nullapp
 $PIN_ROOT/pin -xyzzy  -virtual_segments 1 -reserve_memory $pb -t $PIN_ROOT/extras/dcfg/bin/intel64/global_looppoint.so -replay -xyzzy -replay:deadlock_timeout 0 -replay:basename $prefix -replay:injection 0 -sysstate:in $prefix.sysstate -- $PIN_ROOT/extras/pinplay/bin/intel64/nullapp
done
