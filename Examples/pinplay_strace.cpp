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
/*
 *  This file contains a pinplay-enabled ISA-portable PIN tool for tracing system calls
 */

#include <stdio.h>

#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>

#include "pin.H"
#include "instlib.H"
#ifdef TARGET_LINUX
extern CHAR *syscall_names[];
#endif
#include "pinplay.H"
#  include "sde-init.H"
#  include "sde-control.H"
#include "sde-pinplay-supp.H"


using namespace INSTLIB;

// Track the number of instructions executed
ICOUNT icount;
FILE * trace;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,         "pintool",
    "o", "strace.out", "trace file");
KNOB<BOOL> KnobPid(KNOB_MODE_WRITEONCE, "pintool", "pid", "0" , "Attach pid to filenames.");

// Print syscall number and arguments
VOID SysBefore(VOID * ip, INT32 num, VOID * arg0, VOID * arg1, VOID * arg2, VOID * arg3, VOID * arg4, VOID * arg5)
{
#if defined(TARGET_IA32) 
    // On ia32, there are only 5 registers for passing system call arguments, 
    // but mmap needs 6. For mmap on ia32, the first argument to the system call 
    // is a pointer to an array of the 6 arguments
    if (num == SYS_mmap)
    {
        VOID ** mmapArgs = static_cast<VOID**>(arg0);
        arg0 = mmapArgs[0];
        arg1 = mmapArgs[1];
        arg2 = mmapArgs[2];
        arg3 = mmapArgs[3];
        arg4 = mmapArgs[4];
        arg5 = mmapArgs[5];
    }

    fprintf(trace, "icount  %lld :: ", icount.Count());
#else
    fprintf(trace, "icount  %ld :: ", icount.Count());
#endif

#ifdef TARGET_LINUX
    if ((num == SYS_open) || (num== SYS_unlink))
    {
        fprintf(trace,"%p: %d %s (%s, %p, %p, %p, %p, %p)\n", ip, num, syscall_names[num], (char *)arg0, arg1, arg2, arg3, arg4, arg5);
    }
    else
    {
        fprintf(trace,"%p: %d %s (%p, %p, %p, %p, %p, %p)\n", ip, num, syscall_names[num], arg0, arg1, arg2, arg3, arg4, arg5);
    }
#else
    fprintf(trace,"%p: %d(%p, %p, %p, %p, %p, %p)\n", ip, num, arg0, arg1, arg2, arg3, arg4, arg5);
#endif
}

// Print the return value of the system call
VOID SysAfter(VOID * ret)
{
    fprintf(trace,"returns: %p\n", ret);
    fflush(trace);
}

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace,"#eof\n");
    fclose(trace);
}

INT32 Usage()
{
    cerr <<
        "This tool reports system calls.\n"
        "\n";


    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

VOID SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD
std, VOID *v)
{
    SysBefore((VOID *)PIN_GetContextReg(ctxt, REG_INST_PTR),
          PIN_GetSyscallNumber(ctxt, std),
            (VOID *)PIN_GetSyscallArgument(ctxt, std,0),
            (VOID *)PIN_GetSyscallArgument(ctxt, std,1),
            (VOID *)PIN_GetSyscallArgument(ctxt, std,2),
            (VOID *)PIN_GetSyscallArgument(ctxt, std,3),
            (VOID *)PIN_GetSyscallArgument(ctxt, std,4),
            (VOID *)PIN_GetSyscallArgument(ctxt, std,5)
            );
}

VOID SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD
std, VOID *v)
{
     SysAfter((VOID *) PIN_GetSyscallReturn(ctxt, std));
}


int main(int argc, char *argv[])
{
    static   string filename;
    sde_pin_init(argc,argv);
    sde_init();
    PIN_InitSymbols();

    filename =  KnobOutputFile.Value();
    if( KnobPid )
        filename += "." + decstr( getpid() );
    trace = fopen(filename.c_str(), "w");

    PIN_InitSymbols();

    // start of code for your tool

    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddFiniFunction(Fini, 0);
    icount.Activate();

    // end of code for your tool    

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
