// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
#ifndef GTPIN_SHIM_H
#define  GTPIN_SHIM_H
typedef void (*cpu_on_kernel_build_ptr_t)(const char * kname);
typedef void (*cpu_on_kernel_run_ptr_t)(const char * kname);
typedef void (*cpu_on_kernel_complete_ptr_t)(const char * kname);
typedef void (*cpu_on_gpu_fini_ptr_t)(void);
#endif
