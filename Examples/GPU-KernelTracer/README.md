# Overview
This is a combined GPU+CPU analysis tool.

CPU-Pin is used to drive everything, it loads a specified GT-Pin tool shared library and initializes it.  Hence both CPU and GPU instrumentation/analyses are active at the same time in the same process.

The example GT-Pin tool (GTPinTool/GTPinShim.cpp) allows the CPU-Pin tool to register callbacks on GPU kernel events 'on_kernel_build', 'on_kernel_run', and 'on_kernel_complete', 'on_GPU_fini'.

# Build/Use
The setup works with x86 CPUs (uses Pin) and Intel GPUs (using GT-Pin) only.

## Pre-requisites:
1.  Pin or SDE kit 
   - For Pin build: 
    - Set ***PIN_ROOT*** to point to the latest [Pin kit](https://pintool.intel.com)
  ***OR***
   - For SDE build:
    - Set ***SDE_BUILD_KIT*** to point to an [SDE kit (9.14 or later)](https://www.intel.com/content/www/us/en/developer/articles/tool/software-development-emulator.html)
    - Set ***MBUILD*** to mbuild sources cloned from https://github.com/intelxed/mbuild.git

2. Set ***GTPIN_KIT*** to point to the 'Profilers' directory from the latest [GT-Pin kit](https://www.intel.com/content/www/us/en/developer/articles/tool/gtpin.html ).

## How to build:
```
cd CPUPinTool
./pin.build.sh # Pin-based build : both Probe and JIT mode supported
```
  ***OR***
```
./sde.build.sh # SDE-based build : only JIT mode supported 
```

## How to run:
set ***PINPLAYTOOLS*** __<path to pinplay-tools >__
```
export ZET_ENABLE_API_TRACING_EXP=1 # GT-Pin with level-0 requires this
export ZET_ENABLE_PROGRAM_INSTRUMENTATION=1 # GT-Pin with level-0 requires this
export ZE_ENABLE_TRACING_LAYER=1 # GT-Pin with level-0 requires this
```
### WARNING: do not use SDE-built tool with Pin or vice versa

### Pin-based run
```
$PIN_ROOT/pin -t $PINPLAYTOOLS/Examples/GPU-KernelTracer/CPUPinTool/obj-intel64/xpu-pin-kerneltracer.so -gtpindir $GTPIN_KIT -gtpintool $PINPLAYTOOLS/Examples/GPU-KernelTracer/GTPinTool/build/GTPinShim.so -- matrix_mul_omp
```
### SDE-based run
```
$SDE_BUILD_KIT/sde64 -t64 $PINPLAYTOOLS/Examples/GPU-KernelTracer/CPUPinTool/obj-intel64/xpu-pin-kerneltracer.so -gtpindir $GTPIN_KIT -gtpintool $PINPLAYTOOLS/Examples/GPU-KernelTracer/GTPinTool/build/GTPinShim.so -- matrix_mul_omp
```

### Sample output
```
Result of matrix multiplication using OpenMP: Success - The results are correct!
        ->CPU_on_kernel_build() : kerenel: __omp_offloading_10302_54c0134__Z28MatrixMulOpenMpGpuOffloadingv_l107
        ->CPU_on_kernel_run() : kerenel: __omp_offloading_10302_54c0134__Z28MatrixMulOpenMpGpuOffloadingv_l107
        ->CPU_on_kernel_complete() : kernel: __omp_offloading_10302_54c0134__Z28MatrixMulOpenMpGpuOffloadingv_l107
Result of matrix multiplication using GPU offloading: Success - The results are correct!
        ->CPU_on_gpu_fini()
```

# Details:
```
GPU-KernelTracer/
├── CPUPinTool  <--- a CPU-Pin driver tool that explicitly loads a GT-Pin tool dll.
│   ├── clean.sh
│   ├── makefile
│   ├── makefile.rules
│   ├── mfile.py
│   ├── pin.build.sh
│   ├── sde.build.sh
│   └── xpu-pin-kerneltracer.cpp 
├── GTPinTool <--- a GT-Pin example tool that supports CPU-Pin callbacks on key 
|                  GPU kernel events
│   ├── build
│   │   ├── build.sh
│   │   ├── clean.sh
│   │   └── README
│   ├── CMakeLists.txt
│   └── GTPinShim.cpp
├── Include
│   ├── GTPinLoaderShim.H
│   └── GTPinShim.h
└── README.md
```

## xpu-pin-kerneltracer knobs/switches

```
$PIN_ROOT/pin -t $PINPLAYTOOLS/Examples/GPU-KernelTracer/CPUPinTool/obj-intel64/xpu-pin-kerneltracer.so  -help -- /bin/ls
Pin tools switches

-gt 
        Argument to GT-Pin tool: multiple allowed '-gt arg'... 
-gtpindir  [default ]
        Path to GT-Pin 'Profilers' directory
-gtpintool  [default ]
        Path to GT-Pin tool
..
-probemode  [default 0]
        Using Pin Probe mode
..
-verbose  [default 0]
```


## Registering GPU event callbacks:

### In the CPU Pin tool: 
```
      GTPIN_LOADER gtpin_loader;
      gtpin_loader.Activate(CPU_on_kernel_build, CPU_on_kernel_run, CPU_on_kernel_complete, CPU_on_gpu_fini);


      GTPIN_LOADER tries to find GTPinShimRegisterCallbacks() in the loaed GT-Pin tool.
         If found, it registers callbacks on various GPU events.
```

### In the GT-Pin tool: 
```
  Provide: EXPORT_C_FUNC void GTPinShimRegisterCallbacks(void * ptrb, void * ptrr, void * ptrc, void * ptrf)
In OnKernelBuild(IGtKernelInstrument& instrumentor):
         if(cpu_on_kernel_build)
        {
           (*cpu_on_kernel_build)(kernel.Name().Get());
        }

In  OnKernelRun(IGtKernelDispatch& dispatcher)
       if(cpu_on_kernel_run)
        {
          (*cpu_on_kernel_run)(kernel.Name().Get());
        }

In  OnKernelComplete(IGtKernelDispatch& dispatcher)
        if(cpu_on_kernel_complete)
        {
           (*cpu_on_kernel_complete)(kernel.Name().Get());
        }

In OnFini()
       if(cpu_on_gpu_fini)
        {
           (*cpu_on_gpu_fini)();
        }
```

