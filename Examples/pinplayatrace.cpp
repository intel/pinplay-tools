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
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include "pin.H"
#include "pinplay.H"
#  include "sde-init.H"
#  include "sde-control.H"
#include "sde-pinplay-supp.H"

class THREAD_DATA
{
  private:
    FILE * trace;

  public:
    VOID InitThread(THREADID tid);
    VOID EndThread();
    VOID RecordMemRead(VOID *ip, VOID * addr);
    VOID RecordMemWrite(VOID * ip, VOID * addr);
};

VOID THREAD_DATA::RecordMemRead(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: R %p\n", ip, addr);
}

VOID THREAD_DATA::RecordMemWrite(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: W %p\n", ip, addr);
}

VOID THREAD_DATA::InitThread(THREADID tid)
{
    CHAR filename[128];
    sprintf(filename, "pinatrace.out.%u", tid);
    trace = fopen(filename, "w");
}

VOID THREAD_DATA::EndThread()
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

#define MAX_THREADS 10
THREAD_DATA threads[MAX_THREADS];

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, THREADID tid)
{
    threads[tid].RecordMemRead(ip, addr);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, THREADID tid)
{
    threads[tid].RecordMemWrite(ip, addr);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // instruments loads using a predicated call, i.e.
    // the call happens iff the load will be actually executed
    // (this does not matter for ia32 but arm and ipf have predicated instructions)
    if (INS_IsMemoryRead(ins))
    {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
            IARG_INST_PTR,
            IARG_MEMORYREAD_EA,
            IARG_THREAD_ID,
            IARG_END);
    }

    // instruments stores using a predicated call, i.e.
    // the call happens iff the store will be actually executed
    if (INS_IsMemoryWrite(ins))
    {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
            IARG_INST_PTR,
            IARG_MEMORYWRITE_EA,
            IARG_THREAD_ID,
            IARG_END);
    }
}

VOID InitThread(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    threads[tid].InitThread(tid);
}

VOID EndThread(THREADID  tid, const CONTEXT *ctxt,  INT32 code, VOID *v)
{
    threads[tid].EndThread();
}

VOID Fini(INT32 code, VOID *v)
{
    threads[0].EndThread();
}

int main(int argc, char *argv[])
{
    sde_pin_init(argc,argv);
    sde_init();
    PIN_InitSymbols();

    threads[0].InitThread(0);

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_AddThreadStartFunction(InitThread, NULL);
    PIN_AddThreadFiniFunction(EndThread, NULL);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
