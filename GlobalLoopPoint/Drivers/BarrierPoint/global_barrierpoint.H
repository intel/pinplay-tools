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
 The BARRIERPOINT class 
 Based on the work: BarrierPoint: Sampled Simulation of Multi-Threaded 
 Applications Trevor E. Carlson, Wim Heirman, Kenzo Van Craeynest, 
 Lieven Eeckhout Presented at ISPASS 2014 
 DOI: http://dx.doi.org/10.1109/ISPASS.2014.6844456
*/

#ifndef BARRIERPOINT_H
#define BARRIERPOINT_H

#include <iomanip>
#include <string>

#if !defined (TARGET_WINDOWS)
#  include <sys/cdefs.h>
#endif
#include <unordered_map>

#include "dcfg_pin_api.H"
#include "pinplay.H"
#include "global_isimpoint_inst.H"


using namespace std;
//using namespace tr1;

namespace barrierpoint {
class BARRIERPOINT {
    GLOBALISIMPOINT *gisimpointPtr;
    ADDRINT GOMP_parallel_start_addr; // gcc:OpenMP versions < 4.0
    ADDRINT GOMP_parallel_addr; // gcc:OpenMP version 4.0
    ADDRINT kmpc_fork_call_addr; // Intel icc
    KNOB<string> _MarkerFileKnob;
    KNOB<BOOL> _GlobalProfileKnob;
    ofstream mfile;  // for writing out information about barriers 

    public:
    BARRIERPOINT():_MarkerFileKnob(KNOB_MODE_WRITEONCE, "pintool", 
        "barrierpoint:barrier_info", "", 
        "Output file containing regions skipped due to overlap"),
        _GlobalProfileKnob(KNOB_MODE_WRITEONCE, "pintool",
          "barrierpoint:global_profile", "0",
          "Support global (all-theads) profiling")
    { }

    // Process DCFG and add instrumentation.
    void activate(GLOBALISIMPOINT *gisimpoint)
    {
        gisimpointPtr = gisimpoint;
        
       if (strcmp(_MarkerFileKnob.Value().c_str(),"") != 0)
        {
            mfile.open(_MarkerFileKnob.Value().c_str());
            if (!mfile.is_open())
            {
                cerr << "Could not open output  file " <<
                    _MarkerFileKnob.Value().c_str() << endl;
                PIN_ExitProcess(-1);
            }
        }

        // Add Pin instrumentation.
        IMG_AddInstrumentFunction(loadImage, this);
        TRACE_AddInstrumentFunction(handleTrace, this);
    }
        

    ////// Pin analysis and instrumentation routines.

    // called when an image is loaded.
    static VOID loadImage(IMG img, VOID *v)
    {
        BARRIERPOINT *lt = static_cast<BARRIERPOINT *>(v);
        ASSERTX(lt);
        for ( SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec) )
            for ( RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); 
                rtn = RTN_Next(rtn) )
        {
            if(RTN_Name(rtn) == "GOMP_parallel_start")
            {
                // gcc:OpenMP versions < 4.0
                lt->GOMP_parallel_start_addr = RTN_Address(rtn);
            }
            if(RTN_Name(rtn) == "GOMP_parallel")
            {
                // gcc:OpenMP version 4.0
                lt->GOMP_parallel_addr = RTN_Address(rtn);
            }
            if(RTN_Name(rtn) == "__kmpc_fork_call")
            {
                // Intel icc
                lt->kmpc_fork_call_addr = RTN_Address(rtn);
            }
        }
    }

    // Analysis routine for the entry DCFG basic block for a loop
    static VOID 
    enterBarrier(ADDRINT insAddr,
                BARRIERPOINT *lt, 
                UINT32 imageId,
                THREADID tid) 
    {
        if(lt->mfile.is_open())
        {
             lt->mfile << "tid: " << tid << " Entering barrier at 0x"
                                << hex << insAddr << endl;
        }
        if(lt->_GlobalProfileKnob) 
        {
          if(lt->gisimpointPtr->VectorPendingGlobal())
          {
            // A slice ended in gisimpoint but frequency vector
            // was not emitted. Do it now.
            lt->gisimpointPtr->EmitVectorForFriendGlobal(insAddr, 
              imageId, tid, lt->gisimpointPtr, /*markerOffset*/1);
            // insAddr is the marker captured with IPOINT_BEFORE
            // for gisimpoint we provide an offset of 1
            // as otherwise the execution in this bbl is not 
            // going to be counted.
          }
        }
        else
        {
          if(lt->gisimpointPtr->VectorPending(tid))
          {
            // A slice ended in gisimpoint but frequency vector
            // was not emitted. Do it now.
            lt->gisimpointPtr->EmitVectorForFriend(insAddr, imageId,
              tid, lt->gisimpointPtr, /*markerOffset*/1);
              // insAddr is the marker captured with IPOINT_BEFORE
              // for gisimpoint we provide an offset of 1
              // as otherwise the execution in this bbl is not 
              // going to be counted.
          }
        }
    }

    static BOOL inRange(ADDRINT element, ADDRINT start, ADDRINT end) 
        { return ((start <= element) && (element<=end)); }

    static VOID handleTrace(TRACE trace, VOID *v)
    {
        BARRIERPOINT *lt = static_cast<BARRIERPOINT *>(v);
        ASSERTX(lt);

        RTN rtn = TRACE_Rtn(trace);
        IMG img = RTN_Valid(rtn) ? SEC_Img(RTN_Sec(rtn)) : IMG_Invalid();
        UINT32 imgId = IMG_Valid(img)?IMG_Id(img):0;

        if(lt->inRange(lt->GOMP_parallel_start_addr,
                    TRACE_Address(trace),
                    TRACE_Address(trace)+TRACE_Size(trace))
              || lt->inRange(lt->GOMP_parallel_addr,
                    TRACE_Address(trace),
                    TRACE_Address(trace)+TRACE_Size(trace))
              || lt->inRange(lt->kmpc_fork_call_addr,
                    TRACE_Address(trace),
                    TRACE_Address(trace)+TRACE_Size(trace))
           )
        {
            // loop thru all BBLs in this trace.
            for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); 
                    bbl = BBL_Next(bbl))
            {
                // loop thru all the instrs in this BBL.
                for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); 
                    ins = INS_Next(ins))
                {
                    ADDRINT insAddr = INS_Address(ins);

                    // gcc:OpenMP versions < 4.0
                    if ((lt->GOMP_parallel_start_addr != 0 ) 
                        && ( insAddr == lt->GOMP_parallel_start_addr))
                    {
                        if(lt->mfile.is_open())
                        {
                            lt->mfile << "Instrumenting GOMP_parallel_start 0x"
                                << hex << insAddr << endl;
                        }
                        INS_InsertCall(ins, IPOINT_BEFORE,
                            (AFUNPTR)enterBarrier,
                            IARG_ADDRINT, insAddr, 
                            IARG_PTR, lt,
                            IARG_UINT32, imgId,
                            IARG_THREAD_ID,
                            IARG_END);
                    }

                    // gcc:OpenMP version 4.0
                    if ((lt->GOMP_parallel_addr != 0 ) 
                        && ( insAddr == lt->GOMP_parallel_addr))
                    {
                        if(lt->mfile.is_open())
                        {
                            lt->mfile << "Instrumenting GOMP_parallel 0x"
                                << hex << insAddr << endl;
                        }
                            INS_InsertCall(ins, IPOINT_BEFORE,
                                (AFUNPTR)enterBarrier,
                                IARG_ADDRINT, insAddr, 
                                IARG_PTR, lt,
                                IARG_UINT32, imgId,
                                IARG_THREAD_ID,
                                IARG_END);
                    }

                    // Intel icc
                    if ((lt->kmpc_fork_call_addr != 0 ) 
                        && ( insAddr == lt->kmpc_fork_call_addr))
                    {
                        if(lt->mfile.is_open())
                        {
                            lt->mfile << "Instrumenting kmpc_fork_call_addr 0x"
                                << hex << insAddr << endl;
                        }
                        INS_InsertCall(ins, IPOINT_BEFORE,
                                    (AFUNPTR)enterBarrier,
                                    IARG_ADDRINT, insAddr, 
                                    IARG_PTR, lt,
                                    IARG_UINT32, imgId,
                                    IARG_THREAD_ID,
                                    IARG_END);
                    }
                } // INS.
            } // BBL.
        }
    }
};
} // namespace
#endif
