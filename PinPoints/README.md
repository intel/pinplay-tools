# PinPoints:  Instructions for running Pin+SimPoint toolchain with the following steps:
  1. Generate whole-program pinball (wpb)
  2. Replay wpb and create basic block vectors (bbvs)
  3. Use SimPoint to cluster bbvs and find representative regions 
  4. Replay wpb and use representative region specification to create region pinballs (rpb's)
  5. Replay rpbs,

#  See the HPCA-2013 Tutorial 
[Deterministic PinPoints](https://snipersim.org/w/Tutorial:HPCA_2013_PinPoints)

# Example : run PinPoints flow on 'dotproduct-st'
  cd ../openmp
  <See README>
  make clean; make
  sde-run.pinpoints.single-threaded.sh
