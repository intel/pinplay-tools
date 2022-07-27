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
#include <iostream>
#include <unistd.h>

#include "pin.H"
#include "instlib.H"
#include "control_manager.H"
#include "pinplay.H"
#  include "sde-init.H"
#  include "sde-control.H"
#include "sde-pinplay-supp.H"

static PINPLAY_ENGINE *pinplay_engine;
static std::ofstream out;
using namespace INSTLIB;
using namespace CONTROLLER;

static KNOB<INT32> PauseReplayKnob(KNOB_MODE_WRITEONCE, "pintool", "pause_replay",  "0", "Pause after reaching the region-of-interst so that  debugger can start listening.");
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,         "pintool",
    "o", "controller.out", "trace file");
KNOB<BOOL>   KnobPid(KNOB_MODE_WRITEONCE,                "pintool",
    "i", "0", "append pid to output");
KNOB<BOOL>   KnobEarlyOut(KNOB_MODE_WRITEONCE,                "pintool",
    "my_early_out", "0", "Exit after reporting a STOP event");

using namespace INSTLIB;

// Track the number of instructions executed
ICOUNT icount;

// Contains knobs and instrumentation to recognize start/stop points
CONTROL_MANAGER local_control;
CONTROL_MANAGER *controller;

BOOL stop_seen = FALSE;
/* ===================================================================== */

VOID PauseReplay(UINT32 seconds)
{
    if (seconds != 0 )
    {
        cerr << "Pausing replay for " << dec << seconds 
            << " seconds\n";
        sleep(seconds);
    }
}

VOID PrintIcounts(const char * prefix)
{
    for(THREADID tid=0; tid <  ISIMPOINT_MAX_THREADS; tid++)
    {
        UINT64 count = icount.Count(tid);
        if(count)
        {
            out << prefix << ": tid: " << dec << tid << " icount: " 
                << dec << count << endl; 
        }
    }
}

VOID Handler(EVENT_TYPE ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid,
    BOOL bcast)
{
    std::cerr << "tid: " << dec << tid << " ip: 0x" << hex << ip; 
#if !defined(TARGET_WINDOWS)
    // get line info on current instruction
    INT32 colnum = 0;
    INT32 linenum = 0;
    string filename = "unknown";
    PIN_LockClient();
    PIN_GetSourceLocation((ADDRINT)ip, &colnum, &linenum, &filename);
    PIN_UnlockClient();
    std::cerr << " ( "  << filename << ":" << dec << linenum << " )"; 
#endif
    std::cerr <<  dec << " Inst. Count " << icount.Count(tid) << " ";

#if 0
    if (controller->PCregionsActive())
    {
        CONTROLLER::PCREGION * r = controller->CurrentPCregion(tid);
        cerr << " rid " << dec <<  r->GetRegionId();
        cerr << " start_PC 0x" << hex <<  r->GetRegionStartPC();
        cerr << " start_PC_count " << dec <<  r->GetRegionStartCount();
        cerr << " end_PC 0x" << hex <<  r->GetRegionEndPC();
        cerr << " end_PC_count " << dec <<  r->GetRegionEndCount();
        cerr << " end_count_relative " << dec 
            <<  r->GetRegionEndCountRelative();
        cerr << " length " << dec <<  r->GetRegionLength();
        cerr << " multiplier " <<  r->GetRegionMultiplier();
        if(ev==EVENT_WARMUP_START)
        {
            CONTROLLER::PCREGION * pr = r->GetParentSimulationRegion();
            cerr << " PARENT simulation region rid " << dec 
                <<  pr->GetRegionId();
        }
        cerr << endl;
    }
#endif
    switch(ev)
    {
      case EVENT_START:
      case EVENT_WARMUP_START:
        if(ev==EVENT_WARMUP_START)  std::cerr << "WARMUP ";
        std::cerr << "Start" << endl;
        PrintIcounts("Start");
        if(PauseReplayKnob)
        {
            ASSERTX(pinplay_engine->IsReplayerActive());
            PauseReplay(PauseReplayKnob);
        }
        break;

      case EVENT_STOP:
      case EVENT_WARMUP_STOP:
        if(ev==EVENT_WARMUP_STOP)  std::cerr << "WARMUP ";
        std::cerr << "Stop" << endl;
        stop_seen = TRUE;
        PrintIcounts("Stop");
        if(KnobEarlyOut)
        {
            std::cerr << "Exiting due to -my_early_out" << endl;
            PIN_ExitApplication(0);
        }
        break;

      default:
        ASSERTX(false);
        break;
    }
}
    
INT32 Usage()
{
    cerr <<
        "This pin tool demonstrates use of CONTROL to identify "
        "start/stop points in a program\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID Image(IMG img, VOID * v)
{
    cerr << "G: " << IMG_Name(img) << " LowAddress: " << hex  
    << IMG_LowAddress(img) << " LoadOffset: " << hex << IMG_LoadOffset(img) 
    << dec << " Inst. Count " << icount.Count((ADDRINT)0) << endl;
}

VOID Fini(INT32 code, VOID *v)
{
  cerr << "Fini" << endl;
  if(!stop_seen) PrintIcounts("Fini");
}

VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    cerr << "ThreadFini tid:" << threadid << endl;
}
// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    sde_pin_init(argc,argv);
    sde_init();
    PIN_InitSymbols();

    string filename =  KnobOutputFile.Value();

    if( KnobPid )
    {
        filename += "." + decstr( getpid() );
    }

    // Do this before we activate controllers
    out.open(filename.c_str());
    out << hex << right;
    out.setf(ios::showbase);

    pinplay_engine = sde_tracing_get_pinplay_engine();
    controller = SDE_CONTROLLER::sde_controller_get();
    icount.Activate();

    if(pinplay_engine->IsLoggerActive())
    {
        controller->RegisterHandler(Handler,0, TRUE);
    }
    else
    {
        local_control.RegisterHandler(Handler,0, TRUE);
        local_control.Activate();
        controller = &local_control;
    }
    PIN_AddThreadFiniFunction(ThreadFini, NULL);
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
