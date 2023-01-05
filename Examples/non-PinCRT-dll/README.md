#non-PinCRT-dll : Overview
This tool demonstrates how a Pin tool can dlopen a third-party library (MyLib/mydll.so) built with the default Linux C run-time (***not PinCRT***).

#Pre-requisites:
1.  Pin or SDE kit
   - For Pin build:
    - Set ***PIN_ROOT*** to point to the latest [Pin kit 3.xx](https://pintool.intel.com)
  ***OR***
   - For SDE build:
    - Set ***SDE_BUILD_KIT*** to point to an [SDE kit (9.14 or later)](https://www.intel.com/content/www/us/en/developer/articles/tool/software-development-emulator.html)
    - Set ***MBUILD*** to [mbuild sources cloned](https://github.com/intelxed/mbuild.git)

##How to build:
###Build non-PinCRT shared library
```
cd MyLib
make
cd ..
```
```
./pin.build.sh # Pin-based build : both Probe and JIT mode supported
```
  ***OR***
```
./sde.build.sh # SDE-based build : only JIT mode supported
```

##How to run:
###WARNING: do not use SDE-built tool with Pin or vice versa

###Pin-based run
```
$PIN_ROOT/pin -t obj-intel64/dlopen-driver.so -mydll MyLib/libmydll.so -- /bin/ls
Myfunction called
         argument 'Hi from DLLLoader'
```
###SDE-based run
```
$SDE_BUILD_KIT/sde64 -t64 obj-intel64/dleopen-driver.so -mydll MyLib/libmydll.so -- /bin/ls
Myfunction called
        argument 'Hi from DLLLoader' 
```

#Details:
```
non-PinCRT-dll/
├── clean.sh
├── dllinfo.h
├── dllLoader.H
├── dlopen-driver.cpp
├── makefile
├── makefile.rules
├── mfile.py
├── MyLib
│   ├── Makefile
│   └── mydll.c
├── pin.build.sh
├── README.md
└── sde.build.sh
```
##dllopen-driver knobs/switches
```
$PIN_ROOT/pin -t obj-intel64/dlopen-driver.so -h  -- /bin/ls

Pin tools switches

-mydll  [default ]
        Path to your shared library
-probemode  [default 0]
        Using Pin Probe mode
-verbose  [default 0]
        Verbose mode
```


1. MyLib directory has a 'third-party' library, libmydll.so, that is built using the regular C run-time.
    - This library has a function "void MyFunction(const char \*ptr)"

2. The tool dlopen-driver.so
    - Takes path to the third-party library: -mydll MyLib/libmydll.so 
    - First finds 'dlopen()' and 'dlsym()' function addresses from the *application*. These function  will be typically found in the default libc that the application was linked with.
    - Uses the application  dlopen() to open the specified 'libmydll.so'
    - Uses the application dlsym() to find the function "MyFunction()" 
    - Makes a call 'MyFunction("Hi from DLLLoader")'

##To use with other third-party library of interest:
1. edit dllinfo.h:
    - change typedef of myfunc_t
    - change 'MyFunction' to the name of your function of interest

2. edit dllLoader.H:
    - Currently DLLLoader() calls the function of interest after loading the specified library
    - Instead, your pintool should be calling dll_loader.CallMyFunction() when needed.
```
 75     // Call FunctionOfInterst
 76     CallMyFunction("Hi from DLLLoader"); // CHANGEME: move this to your pintool
```

  - change the body of CallMyFunction() according to your function of interest prototype
```
260 //CHANGEME 
261   static void CallMyFunction(const char *ptr)
262   {
263    if(myfunc_ptr)
264      (*myfunc_ptr)(ptr);
265     else
266       fprintf(stderr, "Skipping MyFunction() call\n");
267   }
```
##Example pintool calliing dll_loader.CallMyFunction()

- See use-myfunction.cpp:  it calls dll_loader.CallMyFunction() at two places
    - Before the first instruction in the main image
    - In Fini() at the end of the execution.
###Pin-based build and run:
```
  make obj-intel64/use-myfunction.so
  $PIN_ROOT/pin -t obj-intel64/use-myfunction.so -mydll MyLib/libmydll.so -- /bin/true
    Myfunction called
         argument 'Hi from DLLLoader'
    Myfunction called
         argument 'Hello from first main image instruction!'
    Myfunction called
         argument 'Hello from Fini'

```
