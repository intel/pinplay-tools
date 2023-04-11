/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <iostream>
#include <dlfcn.h>
#include "pin.H"

using namespace std;

class NVBIT_HANDLER
{

private:
  static AFUNPTR dlopen_addr;
  static AFUNPTR dlsym_addr;
  static AFUNPTR dlerror_addr;
  static std::ostream* out;
  static KNOB<BOOL> KnobVerbose;
  static KNOB<string> KnobNVBitTool;
  static KNOB<string> KnobNVBitDir;
  static AFUNPTR myrtn_addr;
  static cpu_on_gpu_fini_ptr_t callback_on_gpu_fini;
  static cpu_on_kernel_run_ptr_t callback_on_kernel_run;
  static cpu_on_kernel_complete_ptr_t callback_on_kernel_complete;

  static VOID Error(const char * errstring)
  {
    fprintf(stderr, "%s\n", errstring);
    exit(1);
  }

  typedef VOID (*FP_INIT)(void);
  typedef   void *(*dlopen_ptr_t)(const char * const , int );
  typedef void *(*dlsym_ptr_t)(void *, const char * );
  typedef char *(*dlerror_ptr_t)(void );
  typedef void (*nvbit_at_init)();
  typedef void (*nvbit_at_term)();
  typedef void (*nvbit_start)();
  typedef void (*libnvbit_init)();
  typedef void (*nvbit)();
  typedef void (*nvbit_register_t)(cpu_on_kernel_run_ptr_t,cpu_on_kernel_complete_ptr_t,cpu_on_gpu_fini_ptr_t) ;

  static VOID NVBitHandler()
  {
    static bool init = false;

    if (init)          return;
    if(myrtn_addr)
    {
      *out <<  "Calling NVBitShimRegisterCallbacks()" << endl;
      nvbit_register_t myfunc = (nvbit_register_t) myrtn_addr;
      (*myfunc)(callback_on_kernel_run, callback_on_kernel_complete,  callback_on_gpu_fini);
    }
    init = true;
  }

  static VOID LibdlInitProbeNVBitHandler(FP_INIT origInit)
  {
    origInit();

    if(KnobVerbose) *out << "LibdlInitProbeNVBitHandler called" << endl;
    NVBitHandler();
  }

  static VOID imageProbe(IMG img, VOID *arg)
  {
    const char* dynloaderImageName = "libdl";
    const char* libcImageName      = "libc";
    const char* dlopenFuncName = "dlopen";
    const char* dlsymFuncName = "dlsym";
    const char* dlerrorFuncName = "dlerror";
    static bool _initFound = false;

    string imgName = IMG_Name(img);
        if(KnobVerbose) *out << "Loaded image: " << imgName << endl;

    if (strcmp(KnobNVBitTool.Value().c_str(),imgName.c_str()) == 0)
    {
        *out << "Loaded NVBit Tool Image: " << imgName << endl;
        RTN myrtn = RTN_FindByName(img, "NVBitShimRegisterCallbacks");
        if(RTN_Valid(myrtn))
          myrtn_addr = RTN_Funptr(myrtn);
        *out << "\tfound " << RTN_Name(myrtn) << endl;
    }

    // The dynamic linking loader interface, which includes three functions: dlopen, dlsym and dlerror, 
    // can be defined in either libdl or libc library. It is assumed that all three functions are defined
    // in the same library.
    if (!dlopen_addr)
    {
        if (imgName.find(dynloaderImageName) != string::npos ||
            imgName.find(libcImageName) != string::npos)
        {
          RTN rtn = RTN_FindByName(img, dlopenFuncName);
          if (!RTN_Valid(rtn))
          {
            return;
          }

        dlopen_addr = RTN_Funptr(rtn);

        rtn = RTN_FindByName(img, dlsymFuncName);
        if (!RTN_Valid(rtn))
            Error("Unable to find dlsym function");
        else
            if(KnobVerbose) fprintf(stderr, "found %s \n", dlsymFuncName);

        dlsym_addr = RTN_Funptr(rtn);

        rtn = RTN_FindByName(img, dlerrorFuncName);
        if (!RTN_Valid(rtn))
            Error("Unable to find dlerror function");
        else
            if(KnobVerbose) fprintf(stderr, "found %s \n", dlerrorFuncName);

        dlerror_addr = RTN_Funptr(rtn);
        RTN initRtn = RTN_FindByName(img, "_init");
        if (RTN_Valid(initRtn))
        {
            if(KnobVerbose) *out << "_init 0x" << std::hex << RTN_Address(initRtn) << " given name:" << RTN_Name(initRtn) << " in image " << IMG_Name(img) << endl;
            if (RTN_IsSafeForProbedReplacement(initRtn))
            {
                // Define a function prototype that describes the application routine that will be replaced.
                PROTO proto_init = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "_init", PIN_PARG_END());

                // Replace Replace libdl._init with the replacement function, and pass pointer original function as a parameter.
                RTN_ReplaceSignatureProbed(initRtn, AFUNPTR(LibdlInitProbeNVBitHandler), IARG_PROTOTYPE, proto_init, IARG_ORIG_FUNCPTR, IARG_END);

                // Free the function prototype.
                PROTO_Free(proto_init);
                _initFound = true;
            }
            else
            {
                    *out << "Skip replacing _init in " << imgName << " since it is not safe." << endl;
            }
        }
        else
        {
          *out << "_init not found in " << imgName << endl;
        }
      }
        if (_initFound) return;
    }

    if (!IMG_IsMainExecutable(img))
    {
        return;
    }

    // Add probe at first instruction in the image
    ADDRINT entry = IMG_EntryAddress(img);
    
    RTN firstInstructionRtn = RTN_FindByAddress(entry);
    if (!RTN_Valid(firstInstructionRtn))
    {
        firstInstructionRtn = RTN_CreateAt(entry, string("main-entry"));
        if (!RTN_Valid(firstInstructionRtn))
            Error("Unable to find routine for first instruction (Image entry) executed in image");
    }
    
    if (RTN_IsSafeForProbedInsertion(firstInstructionRtn) == false)
    {
        fprintf(stderr, "can not inject NVBit Handler to image  + %s\n", imgName.c_str());
        return;
    }

    if(KnobVerbose) *out << "At image: " << IMG_Name(img) << " (" << entry <<")" << " Inserting probe to NVBit Handler at (init) 0x" << std::hex << RTN_Address(firstInstructionRtn) << endl;
    RTN_InsertCallProbed(firstInstructionRtn, IPOINT_BEFORE, (AFUNPTR)NVBitHandler, IARG_END);

    // Add probe at _start
    RTN startRtn = RTN_FindByName(img, "_start");
    if (!RTN_Valid(startRtn))
    {
        startRtn = RTN_CreateAt(entry, string("main-entry"));
        if (!RTN_Valid(startRtn))
            Error("Unable to find routine for _start in the image");
    }

    // Insert probe only if routines are different - prevent duplicate probe at same location
    if (RTN_Address(startRtn) != RTN_Address(firstInstructionRtn))
    {
     if(KnobVerbose) *out << "Inserting probe to NVBit Handler at (start) 0x" << std::hex << RTN_Address(startRtn) << endl;
      RTN_InsertCallProbed(startRtn, IPOINT_BEFORE, (AFUNPTR)NVBitHandler, IARG_END);
    }
    else 
    {
        if(KnobVerbose) *out << "_start and Image entry are at the same address - skipping second probe" << endl;
    }
  }


  static VOID imageJIT(IMG img, VOID *arg)
  {
    const char* dynloaderImageName = "libdl";
    const char* libcImageName      = "libc";
    const char* dlopenFuncName = "dlopen";
    const char* dlsymFuncName = "dlsym";
    const char* dlerrorFuncName = "dlerror";

    string imgName = IMG_Name(img);
        if(KnobVerbose) *out << "Loaded image: " << imgName << endl;

    if (strcmp(KnobNVBitTool.Value().c_str(),imgName.c_str()) == 0)
    {
        *out << "Loaded NVBit Tool Image: " << imgName << endl;
        RTN myrtn = RTN_FindByName(img, "NVBitShimRegisterCallbacks");
        if(RTN_Valid(myrtn))
          myrtn_addr = RTN_Funptr(myrtn);
        *out << "\tfound " << RTN_Name(myrtn) << endl;
    }
    // The dynamic linking loader interface, which includes three functions: dlopen, dlsym and dlerror, 
    // can be defined in either libdl or libc library. It is assumed that all three functions are defined
    // in the same library.
    if (!dlopen_addr)
    {
      if (imgName.find(dynloaderImageName) != string::npos ||
            imgName.find(libcImageName) != string::npos)
      {
        RTN rtn = RTN_FindByName(img, dlopenFuncName);
        if (RTN_Valid(rtn))
        {
          dlopen_addr = RTN_Funptr(rtn);

          rtn = RTN_FindByName(img, dlsymFuncName);
          if (!RTN_Valid(rtn))
              Error("Unable to find dlsym function");
          else
            if(KnobVerbose) fprintf(stderr, "found %s \n", dlsymFuncName);

          dlsym_addr = RTN_Funptr(rtn);

          rtn = RTN_FindByName(img, dlerrorFuncName);
          if (!RTN_Valid(rtn))
            Error("Unable to find dlerror function");
          else
            if(KnobVerbose) fprintf(stderr, "found %s \n", dlerrorFuncName);

          dlerror_addr = RTN_Funptr(rtn);
        }
      }
    }

    if (!IMG_IsMainExecutable(img))
    {
        return;
    }

    // Add probe at first instruction in the image
    ADDRINT entry = IMG_EntryAddress(img);
    
    RTN firstInstructionRtn = RTN_FindByAddress(entry);
    if (!RTN_Valid(firstInstructionRtn))
    {
        firstInstructionRtn = RTN_CreateAt(entry, string("main-entry"));
        if (!RTN_Valid(firstInstructionRtn))
            Error("Unable to find routine for first instruction (Image entry) executed in image");
    }
    
    if(KnobVerbose) *out << "At image: " << IMG_Name(img) << " (" << entry <<")" << " Inserting call to NVBit Handler before (init) 0x" << std::hex << RTN_Address(firstInstructionRtn) << endl;
    RTN_Open(firstInstructionRtn);
    RTN_InsertCall(firstInstructionRtn, IPOINT_BEFORE, AFUNPTR(NVBitHandler), IARG_END);
    RTN_Close(firstInstructionRtn);

    // Add probe at _start
    RTN startRtn = RTN_FindByName(img, "_start");
    if (!RTN_Valid(startRtn))
    {
        startRtn = RTN_CreateAt(entry, string("main-entry"));
        if (!RTN_Valid(startRtn))
            Error("Unable to find routine for _start in the image");
    }

    // Insert probe only if routines are different - prevent duplicate probe at same location
    if (RTN_Address(startRtn) != RTN_Address(firstInstructionRtn))
    {
      if(KnobVerbose) *out << "Inserting call to NVBit Handler at (start) 0x" << std::hex << RTN_Address(startRtn) << endl;
      //JIT mode
      RTN_Open(startRtn);
      RTN_InsertCall(startRtn, IPOINT_BEFORE, AFUNPTR(NVBitHandler), IARG_END);
      RTN_Close(startRtn);
    }
    else 
    {
        if(KnobVerbose) *out << "_start and Image entry are at the same address - skipping inserting call to NVBit Handler the second time" << endl;
    }
  }


public:
  static KNOB<BOOL> KnobProbe;
  int Activate(cpu_on_kernel_run_ptr_t on_gpu_kernel_run, cpu_on_kernel_complete_ptr_t on_gpu_kernel_complete, cpu_on_gpu_fini_ptr_t on_gpu_fini)
  {
    if (strcmp(KnobNVBitTool.Value().c_str(),"") == 0)
    {
      *out << "Use '-nvbittool <path to NVBit tool>" << endl;
      return 0;
    }
    callback_on_kernel_run = on_gpu_kernel_run;
    callback_on_kernel_complete = on_gpu_kernel_complete;
    callback_on_gpu_fini = on_gpu_fini;
    if (KnobProbe) IMG_AddInstrumentFunction(imageProbe, 0);
    else IMG_AddInstrumentFunction(imageJIT, 0);
    return 0;
  }
};
AFUNPTR NVBIT_HANDLER::dlopen_addr = (AFUNPTR)0;
AFUNPTR NVBIT_HANDLER::dlsym_addr = (AFUNPTR)0;
AFUNPTR NVBIT_HANDLER::dlerror_addr = (AFUNPTR)0;
AFUNPTR NVBIT_HANDLER::myrtn_addr = 0;
std::ostream* NVBIT_HANDLER::out = &cerr;
cpu_on_gpu_fini_ptr_t NVBIT_HANDLER::callback_on_gpu_fini = 0;
cpu_on_kernel_run_ptr_t NVBIT_HANDLER::callback_on_kernel_run = 0;
cpu_on_kernel_complete_ptr_t NVBIT_HANDLER::callback_on_kernel_complete = 0;
// Static knobs
KNOB<BOOL> NVBIT_HANDLER::KnobProbe (KNOB_MODE_WRITEONCE,
    "pintool",
    "probemode", "0", "Using Pin Probe mode");
KNOB<BOOL> NVBIT_HANDLER::KnobVerbose (KNOB_MODE_WRITEONCE,
    "pintool",
    "verbose", "0", "Verbose mode");
KNOB<string> NVBIT_HANDLER::KnobNVBitTool (KNOB_MODE_WRITEONCE,
    "pintool",
    "nvbittool", "", "Path to NVBit tool");
