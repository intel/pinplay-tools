#!/bin/bash
export PIN_ROOT=/user/hgpatil/latest-pin
export ROIPERF_PERFLIST="0:0,0:1"
ulimit -s unlimited
time $PIN_ROOT/pin -t /user/hgpatil/Tools/pinball2elf/pintools/ROIProbe/obj-intel64/ROIperf.so -probecontrol:in probe.in -probecontrol:verbose -perfout dotproduct-st.1_990299.pp/dotproduct-st.1_990299_t0r3.0.BIN.perf.txt -- ./dotproduct-st
