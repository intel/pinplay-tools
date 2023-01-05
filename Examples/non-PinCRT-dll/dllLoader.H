// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */

#ifndef DLL_LOADER_SHIM_H
#define  DLL_LOADER_SHIM_H

#include <iostream>
#include <dlfcn.h>
#include "pin.H"

using namespace std;

class DLL_LOADER
{

private:
  static AFUNPTR dlopen_addr;
  static AFUNPTR dlsym_addr;
  static AFUNPTR dlerror_addr;
  static std::ostream* out;
  static myfunc_t myfunc_ptr;
  static KNOB<BOOL> KnobVerbose;
  static KNOB<string> KnobMyDLL;

  static VOID Error(const char * errstring)
  {
    fprintf(stderr, "%s\n", errstring);
    exit(1);
  }

  typedef VOID (*FP_INIT)(void);
  typedef   void *(*dlopen_ptr_t)(const char * const , int );
  typedef void *(*dlsym_ptr_t)(void *, const char * );
  typedef char *(*dlerror_ptr_t)(void );

  static VOID DLLLoader(const void* img_name)
  {
    static bool init = false;

    if (init)          return;
    if (!dlopen_addr)  return;

    dlopen_ptr_t  dlopen_ptr   = (dlopen_ptr_t)dlopen_addr;
    dlsym_ptr_t   dlsym_ptr    = (dlsym_ptr_t)dlsym_addr;

    // Load the specified library
    // NOTE: if you use PinCRT dlopen(), the following call will fail
    void *handle = (*dlopen_ptr)( KnobMyDLL.Value().c_str(), RTLD_LAZY);
    if(KnobVerbose) fprintf(stderr, "dlopen of %s %s\n", KnobMyDLL.Value().c_str(), (UINT64)handle?"worked":"FAILED");
     if(!handle)
      fprintf(stderr, "dlopen of %s FAILED\n", KnobMyDLL.Value().c_str());

    // Retrieve address of the dll function of interest
    // NOTE: if you use PinCRT dlsym(), the following call will fail
    myfunc_ptr = (myfunc_t)(*dlsym_ptr)(handle, FunctionOfInterest);
    if(KnobVerbose) fprintf(stderr, "Lookup for  %s:%s %s\n", KnobMyDLL.Value().c_str(), FunctionOfInterest, (UINT64)myfunc_ptr?"worked":"FAILED");
     if(!handle)
        fprintf(stderr, "Lookup for  %s:%s FAILED\n",KnobMyDLL.Value().c_str(), FunctionOfInterest);

    // Call FunctionOfInterst
    CallMyFunction("Hi from DLLLoader"); // CHANGEME: move this to your pintool
    init = true;
  }

  static VOID LibdlInitJITDLLLoader(FP_INIT origInit)
  {
    //static bool init = false;
    //if (init)          return;
    if(KnobVerbose) *out << "LibdlInitJITDLLLoader called" << endl;
    DLLLoader("libdl.so");
    //init = true;
    return;
  }

  static VOID LibdlInitProbeDLLLoader(FP_INIT origInit)
  {
    origInit();

    if(KnobVerbose) *out << "LibdlInitProbeDLLLoader called" << endl;
    DLLLoader("libdl.so");
  }

  static VOID image(IMG img, VOID *arg)
  {
    const char* dynloaderImageName = "libdl";
    const char* libcImageName      = "libc";
    const char* dlopenFuncName = "dlopen";
    const char* dlsymFuncName = "dlsym";
    const char* dlerrorFuncName = "dlerror";

    string imgName = IMG_Name(img);
        if(KnobVerbose) *out << "Loaded image: " << imgName << endl;

    // The dynamic linking loader interface, which includes three functions: dlopen, dlsym and dlerror, 
    // can be defined in either libdl or libc library. It is assumed that all three functions are defined
    // in the same library.
    if (!IMG_IsMainExecutable(img))
    {
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
            if (KnobProbe)
            {
              if (RTN_IsSafeForProbedReplacement(initRtn))
              {
                if(KnobVerbose) *out << "ProbeMode: replacing _init in " << imgName << endl;
                // Define a function prototype that describes the application routine that will be replaced.
                PROTO proto_init = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "_init", PIN_PARG_END());

                // Replace Replace libdl._init with the replacement function, and pass pointer original function as a parameter.
                RTN_ReplaceSignatureProbed(initRtn, AFUNPTR(LibdlInitProbeDLLLoader), IARG_PROTOTYPE, proto_init, IARG_ORIG_FUNCPTR, IARG_END);

                // Free the function prototype.
                PROTO_Free(proto_init);
            }
            else
            {
                    *out << "Skip replacing _init in " << imgName << " since it is not safe." << endl;
            }
          }
          else
          {
            //JIT mode
                if(KnobVerbose) *out << "_init 0x" << std::hex << RTN_Address(initRtn) << endl;
                if(KnobVerbose) *out << "JITMode: inserting call before _init in " << imgName << endl;
                RTN_Open(initRtn);
                RTN_InsertCall(initRtn, IPOINT_AFTER, AFUNPTR(LibdlInitJITDLLLoader), IARG_END);
                RTN_Close(initRtn);
          }
        }
      }
        return;
      }
    }
    else // Main image
    {
      // Add probe at first instruction in the image
      ADDRINT entry = IMG_EntryAddress(img);
    
      if(KnobVerbose) *out << "Found entry address for " << imgName << " : " << entry << endl;
      RTN firstInstructionRtn = RTN_FindByAddress(entry);
      if (!RTN_Valid(firstInstructionRtn))
      {
        firstInstructionRtn = RTN_CreateAt(entry, string("main-entry"));
        if (!RTN_Valid(firstInstructionRtn))
            Error("Unable to find routine for first instruction (Image entry) executed in image");
      }
    
      if (KnobProbe)
      {
        if (RTN_IsSafeForProbedInsertion(firstInstructionRtn) == false)
        {
          //Error("can not inject dll to image " + imgName);
          fprintf(stderr, "can not inject dll to image  + %s\n", imgName.c_str());
          return;
        }
        if(KnobVerbose) *out << "At image: " << IMG_Name(img) << " (" << entry <<")" << " Inserting probe to dll loader at (init) 0x" << std::hex << RTN_Address(firstInstructionRtn) << endl;
        RTN_InsertCallProbed(firstInstructionRtn, IPOINT_BEFORE, (AFUNPTR)DLLLoader, IARG_PTR, strdup(imgName.c_str()), IARG_END);
      }
      else
      {
        //JIT mode
        RTN_Open(firstInstructionRtn);
        if(KnobVerbose) *out << "At image: " << IMG_Name(img) << " (" << entry <<")" << " Inserting call to dll loader at (init) 0x" << std::hex << RTN_Address(firstInstructionRtn) << endl;
        RTN_InsertCall(firstInstructionRtn, IPOINT_BEFORE, AFUNPTR(DLLLoader), IARG_END);
        RTN_Close(firstInstructionRtn);
      }

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
        if (KnobProbe)
        {
          if(KnobVerbose) *out << "Inserting probe to dll loader at (start) 0x" << std::hex << RTN_Address(startRtn) << endl;
          RTN_InsertCallProbed(startRtn, IPOINT_BEFORE, (AFUNPTR)DLLLoader, IARG_PTR, strdup(imgName.c_str()), IARG_END);
        }
        else
        {
          //JIT mode
          RTN_Open(startRtn);
          RTN_InsertCall(startRtn, IPOINT_BEFORE, AFUNPTR(DLLLoader), IARG_END);
          RTN_Close(startRtn);
        }
      }
      else 
      {
        *out << "_start and Image entry are at the same address - skipping second probe" << endl;
      }
    }
  }


public:
  static KNOB<BOOL> KnobProbe;
  DLL_LOADER()
  {
  }
  int Activate()
  {
    if (strcmp(KnobMyDLL.Value().c_str(),"") == 0)
    {
      *out << "Use '-mydll <path to shared library>" << endl;
      return 0;
    }
    IMG_AddInstrumentFunction(image, 0);
    return 0;
  }
//CHANGEME 
  static void CallMyFunction(const char *ptr)
  {
   if(myfunc_ptr)
     (*myfunc_ptr)(ptr);
    else
      fprintf(stderr, "Skipping MyFunction() call\n");
  }
};
AFUNPTR DLL_LOADER::dlopen_addr = (AFUNPTR)0;
AFUNPTR DLL_LOADER::dlsym_addr = (AFUNPTR)0;
AFUNPTR DLL_LOADER::dlerror_addr = (AFUNPTR)0;
std::ostream* DLL_LOADER::out = &cerr;
myfunc_t DLL_LOADER::myfunc_ptr = (myfunc_t)NULL;
// Static knobs
KNOB<BOOL> DLL_LOADER::KnobProbe (KNOB_MODE_WRITEONCE,
    "pintool",
    "probemode", "0", "Using Pin Probe mode");
KNOB<BOOL> DLL_LOADER::KnobVerbose (KNOB_MODE_WRITEONCE,
    "pintool",
    "verbose", "0", "Verbose mode");
KNOB<string> DLL_LOADER::KnobMyDLL (KNOB_MODE_WRITEONCE,
    "pintool",
    "mydll", "", "Path to your shared library");
#endif
