GlobalLoopPoint: sources for tools doing global profiling of multi-threaded programs to find representative simulation regions.

  See the HPCA-2022 paper:
  LoopPoint: Checkpoint-driven Sampled Simulation for Multi-threaded Applications
Alen Sabu (National University of Singapore), Harish Patil, Wim Heirman (Intel), Trevor E. Carlson (National University of Singapore)

TIPS:
 1. If using PinPoints scripts, do not use "\~" to specify any paths, instead use "$HOME"
   e.g. the option “--sdehome=~/sde-external-9.33.0-2024-01-07-lin” should be “--sdehome=$HOME/sde-external-9.33.0-2024-01-07-lin”. 
 2. Genearating pinballs for SPEC207 ref inputs requires a lot of memory so if pinball generation fails because of memory issues, try a machine with a larger physical memory.
