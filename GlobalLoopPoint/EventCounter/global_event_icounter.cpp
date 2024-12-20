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
#include <string>
#include <sstream>
#include <iomanip>

#include "pin.H"
#include "dcfg_pin_api.H"
#include "control_manager.H"
#include "global_pcregions_control.H"
#include "global_iregions_control.H"
#include "pinball-sysstate.H"
#if defined(SDE_INIT)
#  include "sde-init.H"
#  include "sde-control.H"
#endif
#if defined(PINPLAY)
#if defined(SDE_INIT)
#include "sde-pinplay-supp.H"
#endif
#include "pinplay.H"
#include "filter.mod.H"
static PINPLAY_ENGINE *pinplay_engine;
#endif
#include "instlib.H"
#include "ialarm.H"
#include "atomic.hpp"
static PINPLAY_ENGINE pp_pinplay_engine;
PINBALL_SYSSTATE::SYSSTATE pbsysstate;

// This tool demonstrates how to register a handler for various
// events reported by SDE's controller module.
// At every event, global (all threads) instruction count is
// printed along with the triggering thread number and PC.

// Optional feature:
// Monitor occurrences of up to three PCs. These PC can be provided
// using the three Knob*PC knobs below.
// A global (all threads) count of the monitored PCs is output on
// certain events.

using namespace INSTLIB;

using namespace CONTROLLER;

static FILTER_MOD filter;
static BOOL filter_used = FALSE;
static BOOL warmup_seen = FALSE;

static CACHELINE_COUNTER global_ins_counter  = {0,0};
static CACHELINE_COUNTER global_filtered_ins_counter  = {0,0};

static PIN_LOCK output_lock;

static FILE  **out_fp=NULL;
static UINT64  *thread_icount;
static UINT64  *thread_filtered_icount;

struct AddrData {
  ADDRINT addr;
  CACHELINE_COUNTER global_addr_counter  = {0,0};
  CACHELINE_COUNTER global_filtered_addr_counter  = {0,0};
  UINT64  *thread_addrcount;
  UINT64  *thread_filtered_addrcount;
};

struct ImageOffsetData {
  string image_offset_spec;
  string img_name;
  UINT32 offset;
  struct AddrData addr_data;
};

struct AddrData *AddrDataArray;
struct ImageOffsetData *ImageOffsetDataArray;
static  UINT32 numAddrs = 0;
static  UINT32 numOffsets = 0;

CONTROL_MANAGER * control_manager = NULL;
static CONTROLLER::CONTROL_MANAGER control("pinplay:");

#if defined(SDE_INIT)
  CONTROL_ARGS args("","pintool:control:pinplay");
#else
  CONTROL_ARGS args("pinplay:","pintool:control:pinplay");
#endif

#if ! defined(SDE_INIT)
#if defined(PINPLAY)
#define KNOB_LOG_NAME  "log"
#define KNOB_REPLAY_NAME "replay"
#define KNOB_FAMILY "pintool:pinplay-driver"

KNOB_COMMENT pinplay_driver_knob_family(KNOB_FAMILY, "PinPlay Driver Knobs");

KNOB<BOOL>KnobPinPlayReplayer(KNOB_MODE_WRITEONCE, KNOB_FAMILY,
                       KNOB_REPLAY_NAME, "0", "Replay a pinball");
KNOB<BOOL>KnobPinPlayLogger(KNOB_MODE_WRITEONCE,  KNOB_FAMILY,
                     KNOB_LOG_NAME, "0", "Create a pinball");
KNOB<BOOL> KnobHandleXsave(KNOB_MODE_WRITEONCE, "pintool", "handle_xsave", "1",
        "Handle xsave to be like SNB architecture");
#endif
#endif
KNOB<UINT32> KnobThreadCount(KNOB_MODE_WRITEONCE, "pintool", "thread_count",
         "0", "Number of threads");
KNOB<string> KnobPrefix(KNOB_MODE_WRITEONCE, "pintool", "prefix", "",
        "Prefix for output event_icount.tid.txt files");
KNOB<BOOL>KnobExitOnStop(KNOB_MODE_WRITEONCE,"pintool", "exit_on_stop", "0",
                       "Exit on Sim-End");
KNOB<BOOL>KnobWarmup(KNOB_MODE_WRITEONCE,"pintool", "warmup", "0",
                       "Treat the first start event as warmup-start");
KNOB<ADDRINT> KnobWatchAddr(KNOB_MODE_APPEND, "pintool", "watch_addr", "",
                  "Address to watch");
KNOB<string> KnobWatchImageOffset(KNOB_MODE_APPEND, "pintool", "watch_image_offset", "",
        "IMG:Offset to watch");


CONTROL_GLOBALPCREGIONS *control_pcregions = NULL;
CONTROL_GLOBALIREGIONS *control_iregions = NULL;

static void PrintAddrcount(THREADID tid, string eventstr)
{
 for (UINT32 a = 0; a < numAddrs ; a++)
 {
   struct AddrData *ad = &AddrDataArray[a];
   cerr << "addr 0x" << hex << ad->addr << " global count " << dec << ad->global_addr_counter._count <<   endl;
  UINT32 tcount = KnobThreadCount; 
    for (UINT32 i = 0; i < tcount; i++)
    {
      UINT64 taddrcount = ad->thread_addrcount[i];
      UINT64 tfilteredaddrcount = taddrcount - ad->thread_filtered_addrcount[i];
#ifdef TARGET_IA32E
    if(filter_used)
      fprintf(out_fp[i], "%s %lx global_filtered_addrcount: %ld global_addrcount: %ld", eventstr.c_str(), ad->addr, ad->global_addr_counter._count - ad->global_filtered_addr_counter._count, ad->global_addr_counter._count);
    else
      fprintf(out_fp[i], "%s %lx  global_addrcount: %ld", eventstr.c_str(), ad->addr, ad->global_addr_counter._count);
#else
    if(filter_used)
      fprintf(out_fp[i], "%s %x global_filtered_addrcount: %lld global_addrcount: %lld", eventstr.c_str(), ad->addr, ad->global_addr_counter._count - ad->global_filtered_addr_counter._count, ad->global_addr_counter._count);
    else
      fprintf(out_fp[i], "%s %x  global_addrcount: %lld", eventstr.c_str(), ad->addr, ad->global_addr_counter._count);
#endif
    fprintf(out_fp[i], "\n");
#ifdef TARGET_IA32E
    if(filter_used)
      fprintf(out_fp[i], "%s %lx tid: %d filtered_addrcount %ld addrcount %ld", eventstr.c_str(), ad->addr, i, tfilteredaddrcount, taddrcount) ;
    else
      fprintf(out_fp[i], "%s %lx tid: %d addrcount %ld", eventstr.c_str(), ad->addr, i, taddrcount) ;
#else
    if(filter_used)
      fprintf(out_fp[i], "%s %x tid: %d filtered_addrcount %lld addrcount %lld", eventstr.c_str(), ad->addr, i, tfilteredaddrcount, taddrcount) ;
    else
      fprintf(out_fp[i], "%s %x tid: %d addrcount %lld", eventstr.c_str(), ad->addr, i, taddrcount) ;
#endif
      fprintf(out_fp[i], "\n");
      fflush(out_fp[i]);
    }
 }
}


static void PrintImageOffsetcount(THREADID tid, string eventstr)
{
 for (UINT32 a = 0; a < numOffsets ; a++)
 {
   struct ImageOffsetData *ad = &ImageOffsetDataArray[a];
   cerr << "addr 0x" << hex << ad->addr_data.addr << " global count " << dec << ad->addr_data.global_addr_counter._count <<   endl;
  UINT32 tcount = KnobThreadCount; 
    for (UINT32 i = 0; i < tcount; i++)
    {
      UINT64 taddrcount = ad->addr_data.thread_addrcount[i];
      UINT64 tfilteredaddrcount = taddrcount - ad->addr_data.thread_filtered_addrcount[i];
#ifdef TARGET_IA32E
    if(filter_used)
      fprintf(out_fp[i], "%s%s %lx global_filtered_addrcount: %ld global_addrcount: %ld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, ad->addr_data.global_addr_counter._count - ad->addr_data.global_filtered_addr_counter._count, ad->addr_data.global_addr_counter._count);
    else
      fprintf(out_fp[i], "%s%s %lx  global_addrcount: %ld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, ad->addr_data.global_addr_counter._count);
#else
    if(filter_used)
      fprintf(out_fp[i], "%s%s %x global_filtered_addrcount: %lld global_addrcount: %lld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, ad->addr_data.global_addr_counter._count - ad->addr_data.global_filtered_addr_counter._count, ad->addr_data.global_addr_counter._count);
    else
      fprintf(out_fp[i], "%s%s %x  global_addrcount: %lld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, ad->addr_data.global_addr_counter._count);
#endif
    fprintf(out_fp[i], "\n");
#ifdef TARGET_IA32E
    if(filter_used)
      fprintf(out_fp[i], "%s%s %lx tid: %d filtered_addrcount %ld addrcount %ld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, i, tfilteredaddrcount, taddrcount) ;
    else
      fprintf(out_fp[i], "%s%s %lx tid: %d addrcount %ld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, i, taddrcount) ;
#else
    if(filter_used)
      fprintf(out_fp[i], "%s%s %x tid: %d filtered_addrcount %lld addrcount %lld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, i, tfilteredaddrcount, taddrcount) ;
    else
      fprintf(out_fp[i], "%s%s %x tid: %d addrcount %lld", eventstr.c_str(),ad->image_offset_spec.c_str(), ad->addr_data.addr, i, taddrcount) ;
#endif
      fprintf(out_fp[i], "\n");
      fflush(out_fp[i]);
    }
 }
}

static void PrintIcount(THREADID tid, string eventstr, UINT32 rid=0)
{
  UINT32 tcount = KnobThreadCount;
  {
    for (UINT32 i = 0; i < tcount; i++)
    {
      UINT64 ticount = thread_icount[i];
      UINT64 tnotfilteredicount = ticount - thread_filtered_icount[i];
      //if(KnobReplayer)
      //  ticount = pinplay_engine.ReplayerGetICount(i);
#ifdef TARGET_IA32E
    if(filter_used)
      fprintf(out_fp[i], "%s global_filtered_icount: %ld global_icount: %ld", eventstr.c_str(), global_ins_counter._count - global_filtered_ins_counter._count, global_ins_counter._count);
    else
      fprintf(out_fp[i], "%s global_icount: %ld", eventstr.c_str(), global_ins_counter._count);
#else
    if(filter_used)
      fprintf(out_fp[i], "%s global_filtered_icount: %lld global_icount: %lld", eventstr.c_str(), global_ins_counter._count - global_filtered_ins_counter._count, global_ins_counter._count);
    else
      fprintf(out_fp[i], "%s global_icount: %lld", eventstr.c_str(), global_ins_counter._count);
#endif
      if(rid)
      {
        fprintf(out_fp[i], " rid: %d \n", rid);
      }
      else
      {
        fprintf(out_fp[i], "\n");
      }
#ifdef TARGET_IA32E
    if(filter_used)
      fprintf(out_fp[i], "%s tid: %d not_filtered_icount %ld icount %ld", eventstr.c_str(), i, tnotfilteredicount, ticount) ;
    else
      fprintf(out_fp[i], "%s tid: %d icount %ld", eventstr.c_str(), i, ticount) ;
#else
    if(filter_used)
      fprintf(out_fp[i], "%s tid: %d not_filtered_icount %lld icount %lld", eventstr.c_str(), i, tnotfilteredicount, ticount) ;
    else
      fprintf(out_fp[i], "%s tid: %d icount %lld", eventstr.c_str(), i, ticount) ;
#endif
      if(rid)
      {
        fprintf(out_fp[i], " rid: %d \n", rid);
      }
      else
      {
        fprintf(out_fp[i], "\n");
      }
      fflush(out_fp[i]);
    }
  }
}

VOID Handler(EVENT_TYPE ev, VOID * v, CONTEXT * ctxt, VOID * ip,
             THREADID tid, BOOL bcast)
{
    PIN_GetLock(&output_lock, 1);

    string eventstr;

    switch(ev)
    {
      case EVENT_START:
        eventstr="Sim-Start";
        if(KnobWarmup && (warmup_seen==FALSE))
        {
          eventstr="Warmup-Start";
          warmup_seen = TRUE;
        }
        break;

      case EVENT_WARMUP_START:
        eventstr="Warmup-Start";
        break;

      case EVENT_STOP:
        eventstr="Sim-End";
        break;

     case EVENT_WARMUP_STOP:
        eventstr="Warmup-Stop";
        break;

      case EVENT_THREADID:
        std::cerr << "ThreadID" << endl;
        eventstr="ThreadID";
        break;

      case EVENT_STATS_EMIT:
        std::cerr << "STATS_EMIT" << endl;
        eventstr="STATS_EMIT";
        break;

      default:
        ASSERTX(false);
        break;
    }
    cerr << " global icount " << dec << global_ins_counter._count;
    cerr <<  " " << eventstr << endl;

    PrintIcount(tid, eventstr);
    if(numAddrs) PrintAddrcount(tid, "\tMarker ");
    if(numOffsets) PrintImageOffsetcount(tid, "\tImage+Offset:");
    if((ev == EVENT_STOP) && KnobExitOnStop)
      PIN_ExitProcess(0);
    PIN_ReleaseLock(&output_lock);
}

VOID LateHandler(EVENT_TYPE ev, VOID * v, CONTEXT * ctxt, VOID * ip,
             THREADID tid, BOOL bcast)
{
    PIN_GetLock(&output_lock, 1);

    string eventstr;

    switch(ev)
    {
      case EVENT_START:
        eventstr="Late-Sim-Start";
        break;

      case EVENT_WARMUP_START:
        eventstr="Late-Warmup-Start";
        break;

      case EVENT_STOP:
        eventstr="Late-Sim-End";
        break;

     case EVENT_WARMUP_STOP:
        eventstr="Late-Warmup-Stop";
        break;

      case EVENT_THREADID:
        std::cerr << "ThreadID" << endl;
        eventstr="Late-ThreadID";
        break;

      case EVENT_STATS_EMIT:
        std::cerr << "STATS_EMIT" << endl;
        eventstr="Late-STATS_EMIT";
        break;

      default:
        ASSERTX(false);
        break;
    }
    cerr << " global icount " << dec << global_ins_counter._count;
    cerr <<  " " << eventstr << endl;
    PIN_ReleaseLock(&output_lock);
}

static VOID ProcessFini(INT32 code, VOID *v)
{
  PrintIcount(0, "Fini ");
  PrintAddrcount(0, "Addr ");
}

// This function is called before every block
// Use the fast linkage for calls
VOID docount(THREADID tid, ADDRINT c)
{
    ATOMIC::OPS::Increment<UINT64>(&global_ins_counter._count, c);
    //global_ins_counter._count+= c;
    thread_icount[tid]+=c;
}

VOID docount_filtered(THREADID tid, ADDRINT c)
{
  ATOMIC::OPS::Increment<UINT64>(&global_filtered_ins_counter._count, c);
  thread_filtered_icount[tid]+=c;
}


#if ! defined(SDE_INIT)
#if defined(PINPLAY)
static void ChangeEAX(ADDRINT *eax)
{
    // turn off all bits beyond x87/SSE/AVX
    *eax = *eax & 0x7;
}
#endif
#endif

static VOID  AddrBefore(THREADID tid, VOID * v)
{
    struct AddrData * ad = (struct AddrData *) v;
    ATOMIC::OPS::Increment<UINT64>(&(ad->global_addr_counter._count), 1);
    ad->thread_addrcount[tid]+=1;
}

static VOID  AddrBefore_filtered(THREADID tid, VOID * v)
{
    struct AddrData * ad = (struct AddrData *) v;
    ATOMIC::OPS::Increment<UINT64>(&(ad->global_filtered_addr_counter._count), 1);
    ad->thread_filtered_addrcount[tid]+=1;
}

static VOID  AddrBeforeAlt(THREADID tid, VOID * v)
{
    struct ImageOffsetData * ad = (struct ImageOffsetData *) v;
    ATOMIC::OPS::Increment<UINT64>(&(ad->addr_data.global_addr_counter._count), 1);
    ad->addr_data.thread_addrcount[tid]+=1;
}

static VOID  AddrBefore_filteredAlt(THREADID tid, VOID * v)
{
    struct ImageOffsetData * ad = (struct ImageOffsetData *) v;
    ATOMIC::OPS::Increment<UINT64>(&(ad->addr_data.global_filtered_addr_counter._count), 1);
    ad->addr_data.thread_filtered_addrcount[tid]+=1;
}

static VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
  if(tid == KnobThreadCount) 
  {
    cerr << "ERROR: Unexpected tid " << tid <<  " check '-thread_count' value " << KnobThreadCount << endl;
    cerr << " (if using replay, provide '-xyzzy -replay:deadlock_timeout 0') " << endl;
    ASSERTX(FALSE);
  }
}

string main_img_name;
ADDRINT main_img_low_address;
VOID ImageLoad(IMG img, VOID* v)
{
  if(!numOffsets) return;
  for (UINT32 a = 0; a < numOffsets ; a++)
  {
    string img_name = ImageOffsetDataArray[a].img_name;
    UINT32 offset = ImageOffsetDataArray[a].offset;
    if(IMG_Name(img).find(img_name) != string::npos) 
    {
      ImageOffsetDataArray[a].addr_data.addr = 
           IMG_LowAddress(img)+offset;
      cerr << "Watching IMG+offset: " <<  ImageOffsetDataArray[a].img_name << "+" << hex << ImageOffsetDataArray[a].offset << " addr 0x" << hex << ImageOffsetDataArray[a].addr_data.addr << endl;
    }
  }
}

VOID Instruction(INS ins, BOOL trace_filtered, VOID *v)
{
  for (UINT32 a = 0; a < numAddrs ; a++)
  {
    if(INS_Address(ins) == AddrDataArray[a].addr)
    {
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(AddrBefore),
                       IARG_THREAD_ID,
                       IARG_PTR, (VOID *)&AddrDataArray[a],
                       IARG_END);
      if (trace_filtered)
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(AddrBefore_filtered),
                       IARG_THREAD_ID,
                       IARG_PTR, (VOID *)&AddrDataArray[a],
                       IARG_END);
    }
  }
}

VOID InstructionAlt(INS ins, BOOL trace_filtered, VOID *v)
{
  for (UINT32 a = 0; a < numOffsets ; a++)
  {
    if(INS_Address(ins) == ImageOffsetDataArray[a].addr_data.addr)
    {
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(AddrBeforeAlt),
                       IARG_THREAD_ID,
                       IARG_PTR, (VOID *)&ImageOffsetDataArray[a],
                       IARG_END);
      if (trace_filtered)
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(AddrBefore_filteredAlt),
                       IARG_THREAD_ID,
                       IARG_PTR, (VOID *)&ImageOffsetDataArray[a],
                       IARG_END);
    }
  }
}

VOID Trace(TRACE trace, VOID *v)
{
    BOOL filtered = !filter.SelectTrace(trace);
    if (filtered ) filter_used = TRUE; // will be set multiple times and that's ok
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to docount for every bbl, passing
        // the number of instructions.
        // IPOINT_ANYWHERE allows Pin to schedule the call
        //  anywhere in the bbl
        // to obtain best performance.
        // Use a fast linkage for the call.
        BBL_InsertCall(bbl, IPOINT_BEFORE, AFUNPTR(docount),
                       IARG_THREAD_ID,
                       IARG_UINT32, BBL_NumIns(bbl),
                       IARG_END);
        if(filtered)
          BBL_InsertCall(bbl, IPOINT_BEFORE, AFUNPTR(docount_filtered),
                       IARG_THREAD_ID,
                       IARG_UINT32, BBL_NumIns(bbl),
                       IARG_END);
#if ! defined(SDE_INIT)
#if defined(PINPLAY)
        if (KnobHandleXsave)
        {
          for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
          {
            if ((INS_Category(ins) == XED_CATEGORY_XSAVE) || 
                INS_Category(ins) == XED_CATEGORY_XSAVEOPT ||
                INS_Opcode(ins) == XED_ICLASS_XRSTOR)
            {
                  INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(ChangeEAX),
                    IARG_REG_REFERENCE, REG_GAX,
                    IARG_END);
            }
          }
        }
#endif
#endif
     for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
     {
      if(numAddrs)Instruction(ins, filtered, v);
      if(numOffsets)InstructionAlt(ins, filtered, v);
     }
    }
}

static void CheckWatchKnobs()
{
 numAddrs = KnobWatchAddr.NumberOfValues();
 numOffsets = KnobWatchImageOffset.NumberOfValues();
 // The default empty value counts as 1 ; fix that
 if((numAddrs == 1) && KnobWatchAddr == 0) numAddrs = 0;
 if((numOffsets == 1) && KnobWatchImageOffset.Value().empty()) numOffsets = 0;
 if(numAddrs && numOffsets)
 {
   cerr << "ERROR: Cannot combine '-watch_addr' and '-watch_image_offset'\n";
   cerr << "numAddrs " << numAddrs << "numOffsets " << numOffsets << endl;
   PIN_ExitProcess(1);
 }
 cerr << "CheckWatchKnobs():  numAddrs " << numAddrs << " numOffsets " << numOffsets << endl;
}


void split_image_offset(string image_offset_spec, string *img_name_ptr, UINT32 *offset_ptr) 
{
    string img_name;
    UINT32 offset;

    // Find the position of the plus
    size_t plus_pos = image_offset_spec.find('+');
    
    // Check if plus was found
    if (plus_pos != std::string::npos) {
        // Extract img_name using substr
        img_name = image_offset_spec.substr(0, plus_pos);
        
        // Extract offset using substr and convert to uint32_t
        string offset_str = image_offset_spec.substr(plus_pos + 1);
        istringstream(offset_str) >> hex >> offset; // Convert string to UINT32

        // Output the results
        std::cout << "Image Name: " << img_name << std::endl;
        std::cout << "Offset: 0x" << hex << offset << std::endl;
    } else {
        std::cerr << "Invalid format. Expected 'img_name:offset'." << std::endl;
    }

   *img_name_ptr = img_name;
   *offset_ptr = offset;
    return;
}

static void OpenThreadFiles()
{
 if(!KnobThreadCount)
 {
   cerr << "ERROR: must use '-thread_count N'\n";
   PIN_ExitProcess(1);
 }

 if(KnobPrefix.Value().empty())
 {
   cerr << "ERROR: must use '-prefix prefix'\n";
   PIN_ExitProcess(1);
 }
 UINT32 tcount = KnobThreadCount;
 thread_icount = new UINT64 [tcount]();
 thread_filtered_icount = new UINT64 [tcount]();
 out_fp = new FILE *[tcount]();
 for (UINT32 i = 0; i < tcount; i++)
 {
    char  fname[100];
    sprintf(fname, "%s.event_icount.%d.txt", KnobPrefix.Value().c_str(), i);
    out_fp[i] = fopen(fname, "w");
    ASSERTX(out_fp[i]);
 }

 if(numAddrs)
 {
  AddrDataArray = new struct AddrData [numAddrs]();
  for (UINT32 a = 0; a < numAddrs ; a++)
  {
    AddrDataArray[a].addr = KnobWatchAddr.Value(a);
    cerr << "Watching addr 0x" << hex << AddrDataArray[a].addr << endl;
    AddrDataArray[a].thread_addrcount = new UINT64 [tcount]();
    AddrDataArray[a].thread_filtered_addrcount = new UINT64 [tcount]();
  }
 }

 if(numOffsets)
 {
  ImageOffsetDataArray = new struct ImageOffsetData [numOffsets]();
  for (UINT32 a = 0; a < numOffsets ; a++)
  {
    string image_offset_spec = KnobWatchImageOffset.Value(a);
    ImageOffsetDataArray[a].image_offset_spec = image_offset_spec;
    string img_name;
    UINT32 offset;
    split_image_offset(image_offset_spec, &img_name, &offset);
    ImageOffsetDataArray[a].img_name = img_name;
    ImageOffsetDataArray[a].offset = offset;
    ImageOffsetDataArray[a].addr_data.addr = 0; // to be updated
    cerr << "Watching IMG+offset: " <<  ImageOffsetDataArray[a].img_name << "+" << hex << ImageOffsetDataArray[a].offset << endl;
    ImageOffsetDataArray[a].addr_data.thread_addrcount = new UINT64 [tcount]();
    ImageOffsetDataArray[a].addr_data.thread_filtered_addrcount = new UINT64 [tcount]();
  }
 }
}
 
INT32 Usage()
{
    cerr << "Tool to output global/per-thread counts at controller events." << endl;
    cerr << "Usage examples:" << endl;
    cerr << "... -thread_count 8 -prefix lbm  -pinplay:pcregions:in lbm-s.1_15218.Data/lbm-s.1_15218.global.pinpoints.csv ..." << endl << endl;
    cerr << "... -thread_count 8 -prefix lbm  -pinplay:control start:address:0x00054099f:count10078754:global,stop:address:0x00054099f:count3727356:global -pinplay:controller_log ..." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}
// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{

  control_pcregions = new CONTROL_GLOBALPCREGIONS(args);
  control_iregions = new CONTROL_GLOBALIREGIONS(args);
#if defined(SDE_INIT)
    sde_pin_init(argc,argv);
    sde_init();
#else
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
#endif
    PIN_InitSymbols();

   CheckWatchKnobs();
   OpenThreadFiles();

#if defined(SDE_INIT)
    // This is a replay-only tool (for now)
    pinplay_engine = sde_tracing_get_pinplay_engine();
#else
    pinplay_engine = &pp_pinplay_engine;
    pinplay_engine->Activate(argc, argv, KnobPinPlayLogger, KnobPinPlayReplayer);
#endif

#if defined(PINPLAY)
#if defined(SDE_INIT)
    control_manager = SDE_CONTROLLER::sde_controller_get();
#else

    //if(KnobPinPlayLogger)
     // control_manager = pinplay_engine->LoggerGetController();
   control.Activate();
   control_manager = &control;
#endif
#else //not PinPlay
#if defined(SDE_INIT)
    control_manager = SDE_CONTROLLER::sde_controller_get();
#else
   control.Activate();
   control_manager = &control;
#endif
#endif


    PIN_InitLock(&output_lock);
    control_manager->RegisterHandler(Handler, 0, 0, LateHandler);

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(ProcessFini, 0);

    control_pcregions->Activate(control_manager);
    control_iregions->Activate(control_manager);
    
    filter.Activate();
    pbsysstate.Activate(pinplay_engine);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
