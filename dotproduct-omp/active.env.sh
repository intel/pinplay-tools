#Copyright (C) 2022 Intel Corporation
#SPDX-License-Identifier: BSD-3-Clause
export KMP_SETTINGS=true
export KMP_HW_SUBSET=1S,8C,1T
export OMP_NUM_THREADS=8
export KMP_STACKSIZE=192M
#export KMP_AFFINITY=compact,0,granularity=thread,verbose
export KMP_AFFINITY=compact,verbose
export KMP_BLOCKTIME=infinite
export OMP_WAIT_POLICY=active
export OMP_DYNAMIC=FALSE
export FORT_BUFFERED=true
