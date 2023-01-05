// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GTPIN_LOADER_SHIM_H
#define  GTPIN_LOADER_SHIM_H

#include <iostream>
#include <dlfcn.h>
#include "pin.H"
#include "gtpin.h"
#include "GTPinShim.h"

using namespace std;

class GTPIN_LOADER
{

private:
  static AFUNPTR dlopen_addr;
  static AFUNPTR dlsym_addr;
  static AFUNPTR dlerror_addr;
  static std::ostream* out;
  static KNOB<BOOL> KnobVerbose;
  static KNOB<string> KnobGTPinTool;
  static KNOB<string> KnobGTPinDir;
  static KNOB<string> KnobGTPinArg;
  static UINT32 gtpin_argc;
  static const char ** gtpin_argv;
  static cpu_on_kernel_build_ptr_t callback_on_kernel_build;
  static cpu_on_kernel_run_ptr_t callback_on_kernel_run;
  static cpu_on_kernel_complete_ptr_t callback_on_kernel_complete;
  static cpu_on_gpu_fini_ptr_t callback_on_gpu_fini;

  static VOID Error(const char * errstring)
  {
    fprintf(stderr, "%s\n", errstring);
    exit(1);
  }

  typedef VOID (*FP_INIT)(void);
  typedef   void *(*dlopen_ptr_t)(const char * const , int );
  typedef void *(*dlsym_ptr_t)(void *, const char * );
  typedef char *(*dlerror_ptr_t)(void );
  typedef void (*gtpin_entry_t)(int , const char *[]);
  typedef void (*gtpin_register_t)(cpu_on_kernel_build_ptr_t, cpu_on_kernel_run_ptr_t, cpu_on_kernel_complete_ptr_t, cpu_on_gpu_fini_ptr_t) ;

  static VOID GTPinLoader(const void* img_name)
  {
    static bool init = false;

    if (init)          return;
    if (!dlopen_addr)  return;

    dlopen_ptr_t  dlopen_ptr   = (dlopen_ptr_t)dlopen_addr;
    dlsym_ptr_t   dlsym_ptr    = (dlsym_ptr_t)dlsym_addr;
    //dlerror_ptr_t dlerror_ptr  = (dlerror_ptr_t)dlerror_addr;

    // Load the GTPpin tool
    void *handle = (*dlopen_ptr)( KnobGTPinTool.Value().c_str(), RTLD_LAZY);
    if(KnobVerbose) fprintf(stderr, "found %s %lx\n", KnobGTPinTool.Value().c_str(), (UINT64)handle);

    // Retrieve address of GTPinShimRegisterCallbacks in the GTPpin tool
    gtpin_register_t gtpin_register_ptr = (gtpin_register_t)(*dlsym_ptr)(handle, "GTPinShimRegisterCallbacks");
    if(KnobVerbose) fprintf(stderr, "found %s %lx\n", "GTPinShimRegisterCallbacks", (UINT64)gtpin_register_ptr);

    // Call GTPinShimRegisterCallbacks to register on_kernel_* callbacks
   if(gtpin_register_ptr)
     (*gtpin_register_ptr)(callback_on_kernel_build, callback_on_kernel_run, callback_on_kernel_complete, callback_on_gpu_fini);
    else
      fprintf(stderr, "Skipping GTPinShimRegisterCallbacks() call");

    // Retrieve address of GTPin_Entry in the GTPpin tool
    gtpin_entry_t gtpin_entry_ptr = (gtpin_entry_t)(*dlsym_ptr)(handle, "GTPin_Entry");
    if(KnobVerbose) fprintf(stderr, "found %s %lx\n", "GTPin_Entry", (UINT64)gtpin_entry_ptr);

    // Call GTPin_Entry to start GTPin tool
   if(gtpin_entry_ptr)
      (*gtpin_entry_ptr)(gtpin_argc, gtpin_argv );
   else
      fprintf(stderr, "Skipping GTPin_Entry() call");

    init = true;
  }

  static VOID LibdlInitJITGTPinLoader(FP_INIT origInit)
  {
    static bool init = false;
    if (init)          return;
    if(KnobVerbose) *out << "LibdlInitJITGTPinLoader called" << endl;
    GTPinLoader("libdl.so");
    init = true;
    return;
  }

  static VOID LibdlInitProbeGTPinLoader(FP_INIT origInit)
  {
    origInit();

    if(KnobVerbose) *out << "LibdlInitProbeGTPinLoader called" << endl;
    GTPinLoader("libdl.so");
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
                // Define a function prototype that describes the application routine that will be replaced.
                PROTO proto_init = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "_init", PIN_PARG_END());

                // Replace Replace libdl._init with the replacement function, and pass pointer original function as a parameter.
                RTN_ReplaceSignatureProbed(initRtn, AFUNPTR(LibdlInitProbeGTPinLoader), IARG_PROTOTYPE, proto_init, IARG_ORIG_FUNCPTR, IARG_END);

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
                if(KnobVerbose) *out << "_init 0x" << std::hex << RTN_Address(initRtn) << " given name:" << RTN_Name(initRtn) << endl;
                RTN instrtn = initRtn; 
                RTN_Open(instrtn);
                RTN_InsertCall(instrtn, IPOINT_AFTER, AFUNPTR(LibdlInitJITGTPinLoader), IARG_END);
                RTN_Close(instrtn);
          }
        }
        else // "_init" routine not valid. Workaround for JIT mode
        {
          if (!KnobProbe)
          {
            //JIT mode
                if(KnobVerbose) *out << "_init RTN not found: JIT mode workaround" << endl;
                RTN instrtn = rtn; 
                if(KnobVerbose) *out << "instrtn 0x" << std::hex << RTN_Address(instrtn) << " given name:" << RTN_Name(instrtn) << endl;
                RTN_Open(instrtn);
                RTN_InsertCall(instrtn, IPOINT_BEFORE, AFUNPTR(LibdlInitJITGTPinLoader), IARG_END);
                RTN_Close(instrtn);
          }
        }
      }
        return;
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
    
          if (KnobProbe)
          {
    if (RTN_IsSafeForProbedInsertion(firstInstructionRtn) == false)
    {
        //Error("can not inject gtpin to image " + imgName);
        fprintf(stderr, "can not inject gtpin to image  + %s\n", imgName.c_str());
        return;
    }

    {
        if(KnobVerbose) *out << "At image: " << IMG_Name(img) << " (" << entry <<")" << " Inserting probe to gtpin loader at (init) 0x" << std::hex << RTN_Address(firstInstructionRtn) << endl;
    }
    RTN_InsertCallProbed(firstInstructionRtn, IPOINT_BEFORE, (AFUNPTR)GTPinLoader, IARG_PTR, strdup(imgName.c_str()), IARG_END);
          }
          else
          {
            //JIT mode
                RTN_Open(firstInstructionRtn);
                RTN_InsertCall(firstInstructionRtn, IPOINT_BEFORE, AFUNPTR(GTPinLoader), IARG_END);
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
        //if (DLLINJECT_UTILS::IsVerbose())
        {
            if(KnobVerbose) *out << "Inserting probe to gtpin loader at (start) 0x" << std::hex << RTN_Address(startRtn) << endl;
        }
        RTN_InsertCallProbed(startRtn, IPOINT_BEFORE, (AFUNPTR)GTPinLoader, IARG_PTR, strdup(imgName.c_str()), IARG_END);
          }
          else
          {
            //JIT mode
                RTN_Open(startRtn);
                RTN_InsertCall(startRtn, IPOINT_BEFORE, AFUNPTR(GTPinLoader), IARG_END);
                RTN_Close(startRtn);
          }
    }
    else 
    {
        *out << "_start and Image entry are at the same address - skipping second probe" << endl;
    }
  }


public:
  static KNOB<BOOL> KnobProbe;
  GTPIN_LOADER()
  {
  }
  int Activate(cpu_on_kernel_build_ptr_t on_kernel_build, cpu_on_kernel_run_ptr_t on_kernel_run, cpu_on_kernel_complete_ptr_t on_kernel_complete, cpu_on_gpu_fini_ptr_t on_gpu_fini)
  {
    callback_on_kernel_build = on_kernel_build;
    callback_on_kernel_run = on_kernel_run;
    callback_on_kernel_complete = on_kernel_complete;
    callback_on_gpu_fini = on_gpu_fini;
    if (strcmp(KnobGTPinTool.Value().c_str(),"") == 0)
    {
      *out << "Use '-gtpintool <path to gtpin tool>" << endl;
      return 0;
    }
    if (strcmp(KnobGTPinDir.Value().c_str(),"") == 0)
    {
      *out << "Use '-gtpindir <path to gtpin root>" << endl;
      return 0;
    }
    if (KnobGTPinArg.NumberOfValues() > 0)
    {
      gtpin_argc = KnobGTPinArg.NumberOfValues();
      gtpin_argc +=  2; // for "--installDir", KnobGTPinDir
      gtpin_argv = new const char * [gtpin_argc];
      for (UINT32 i = 0; i < gtpin_argc - 2; i++)
      {
        gtpin_argv[i] = KnobGTPinArg.Value(i).c_str();
        *out << "gtpinarg " << i << " " << gtpin_argv[i] << endl;
      }
      gtpin_argv[gtpin_argc-2] =  "--installDir";
        *out << "gtpinarg " << gtpin_argc - 2 << " " << gtpin_argv[gtpin_argc - 2] << endl;
      gtpin_argv[gtpin_argc-1] =  KnobGTPinDir.Value().c_str();
        *out << "gtpinarg " << gtpin_argc - 1 << " " << gtpin_argv[gtpin_argc - 1] << endl;
    }
    IMG_AddInstrumentFunction(image, 0);
    return 0;
  }
};
AFUNPTR GTPIN_LOADER::dlopen_addr = (AFUNPTR)0;
AFUNPTR GTPIN_LOADER::dlsym_addr = (AFUNPTR)0;
AFUNPTR GTPIN_LOADER::dlerror_addr = (AFUNPTR)0;
std::ostream* GTPIN_LOADER::out = &cerr;
UINT32 GTPIN_LOADER::gtpin_argc = 0;
cpu_on_kernel_build_ptr_t GTPIN_LOADER::callback_on_kernel_build = 0;
cpu_on_kernel_run_ptr_t GTPIN_LOADER::callback_on_kernel_run = 0;
cpu_on_kernel_complete_ptr_t GTPIN_LOADER::callback_on_kernel_complete = 0;
cpu_on_gpu_fini_ptr_t GTPIN_LOADER::callback_on_gpu_fini = 0;
const char ** GTPIN_LOADER::gtpin_argv = (const char **)NULL;
// Static knobs
KNOB<BOOL> GTPIN_LOADER::KnobProbe (KNOB_MODE_WRITEONCE,
    "pintool",
    "probemode", "0", "Using Pin Probe mode");
KNOB<BOOL> GTPIN_LOADER::KnobVerbose (KNOB_MODE_WRITEONCE,
    "pintool",
    "verbose", "0", "Verbose mode");
KNOB<string> GTPIN_LOADER::KnobGTPinTool (KNOB_MODE_WRITEONCE,
    "pintool",
    "gtpintool", "", "Path to GT-Pin tool");
KNOB<string> GTPIN_LOADER::KnobGTPinDir (KNOB_MODE_WRITEONCE,
    "pintool",
    "gtpindir", "", "Path to GT-Pin 'Profilers' directory");
KNOB<string> GTPIN_LOADER::KnobGTPinArg (KNOB_MODE_APPEND,
    "pintool",
    "gt", "", "Argument to GT-Pin tool: multiple allowed '-gt arg'... ");
#endif
