/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stapin.h>
#include <unordered_set>
#include <thread>
//#include "STAPINPerf.H"


using std::cerr;
using std::endl;
using std::hex;
using std::ios;
using std::string;
using namespace std;

typedef struct _RINFO
{
 string name;
 UINT32 icount_static;
 UINT64 count;
} RINFO;
typedef RINFO *RINFO_PTR;
typedef std::map<string, RINFO_PTR> RTNNAME_MAP;
typedef RTNNAME_MAP::iterator RTNNAME_MAP_ITER;

PIN_LOCK rtnperf_lock;
RTNNAME_MAP RtnNameMap;


typedef void (*EXITFUNCPTR)(int);
EXITFUNCPTR origExit;

KNOB<string> KnobImage(KNOB_MODE_WRITEONCE, "pintool", "image", "", "Image of interest");
KNOB<string> KnobRoutine(KNOB_MODE_WRITEONCE, "pintool", "routine", "", "Routine of interest");
KNOB<string> KnobVar(KNOB_MODE_WRITEONCE, "pintool", "var", "", "Variable of interest");
KNOB<INT32> KnobLineNo(KNOB_MODE_WRITEONCE, "pintool", "lineno", "-1", "Symbol visible at this lineno.");
KNOB<BOOL> KnobSTAPIN(KNOB_MODE_WRITEONCE, "pintool", "useSTAPIN", "1", "Use STAPIN");
KNOB<BOOL> KnobAllCalls(KNOB_MODE_WRITEONCE, "pintool", "allcalls", "0", "Process all calls for each RTN ");
KNOB<string> KnobOutFile(KNOB_MODE_WRITEONCE, "pintool", "outfile", "stapinperf.out.txt", "Output report in this file.");
KNOB<string> KnobMsgFile(KNOB_MODE_WRITEONCE, "pintool", "msgfile", "stapinperf.msg.txt", "Output messages in this file.");
KNOB<BOOL> KnobProbe(KNOB_MODE_WRITEONCE, "pintool", "probemode", "1", "Use probe mode");
KNOB<BOOL> KnobSetContextOnly(KNOB_MODE_WRITEONCE, "pintool", "set_context_only", "0", "Only do set_context() and skip get_symbols()");
string ImageOI="";
string RoutineOI="";
ofstream outFile;
ofstream msgFile;


bool charstr_is_empty(const char *str) {
    // Check if the pointer is NULL or points to an empty string
    return str == NULL || *str == '\0';
}

void print_symbol_info(stapin::Symbol symbol, CONTEXT* ctx){
    stapin::Symbol tsymbol;
    msgFile <<"*******************************************************************"<<std::endl;
    msgFile <<"SYMBOL NAME: "<<symbol.name<<std::endl;
    msgFile <<"SYMBOL SIZE: "<<std::dec<<symbol.size<<std::endl;
    msgFile <<"SYMBOL MEMORY: "<<std::hex<<symbol.memory<<std::endl;
    msgFile <<"SYMBOL unique id: "<<symbol.uniqueId<<std::endl;
    msgFile <<"SYMBOL flags: "<<static_cast<int>(symbol.flags)<<std::endl;
    msgFile <<"SYMBOL type: "<<static_cast<int>(symbol.type)<<std::endl;
    msgFile <<"SYMBOL type unique ID : "<<symbol.typeUniqueId<<std::endl;
    msgFile <<"SYMBOL type unique ID : "<<symbol.typeUniqueId<<std::endl;
#if 1
    if(stapin::get_symbol_by_id(symbol.typeUniqueId, &tsymbol))
      msgFile <<"TYPE SYMBOL name: "<<tsymbol.name<<std::endl;
    else
      msgFile <<"Failed to get TYPE SYMBOL name: "<<std::endl;
#endif

    if(stapin::E_symbol_flags::VALUE_IN_REG == symbol.flags){
         msgFile <<"variable " << symbol.name << " is in REG " << REG_StringShort(REG_FullRegName(symbol.reg)) << endl;
         if(!charstr_is_empty(tsymbol.name))
          if(REG_is_gr(symbol.reg)) msgFile <<"variable Value" << hex << PIN_GetContextReg(ctx, (symbol.reg)) << endl;
    }
}

VOID PrintStats()
{
 static BOOL called=false;
 cerr << "Entered PrintStats() called before: " << called << endl;
 if(called) return;
 called=true;

 for(RTNNAME_MAP_ITER it = RtnNameMap.begin(); it != RtnNameMap.end(); it++)
 {
    RINFO *trinfo = it->second;
     if (trinfo)
     {
         msgFile << "RTN_Name: " << trinfo->name << " ( " <<  it->first << " ) " <<  " Static instruction count " << dec << trinfo->icount_static << endl;
        msgFile << "  Call count " << trinfo->count << endl;
     }
  }

    return;
}

static VOID ExitInProbeMode(EXITFUNCPTR origExit, int code)
{
    PrintStats();
    origExit(code);
}


// Should we process this IMG?
BOOL ProcessImage(IMG img) 
{     
  if(ImageOI.empty()) return true; // all images will be processed
  if(IMG_Name(img).find("libc.so") != string::npos)  return true; // for handling "_exit()"
  if(IMG_Name(img).find(ImageOI) != string::npos)  return true; // for handling  user-specified image
  return false;
}            

// Should we process this RTN?
BOOL ProcessRoutine(RTN rtn) 
{
  if (RoutineOI.empty()) return true; // No -routine, process all routines
  string rtnname = RTN_Name(rtn);
  if (rtnname == RoutineOI)
  {
    return true;
  }
  return false;
}


static bool first_time = true;
VOID get_symbols_start( CONTEXT* ctx, VOID *v){
  if(KnobProbe)
  {
    if(first_time)
    {
      first_time = false;
      stapin::notify_thread_start(ctx, 0);
    }
  }
  RINFO_PTR rinfo =  ( RINFO_PTR )v;
  PIN_GetLock(&rtnperf_lock, 1);
  UINT64 count = rinfo->count += 1;
  PIN_ReleaseLock(&rtnperf_lock);

  if(count==1)
  {
    if(KnobSTAPIN)
    {
      if(!ctx) return;
      msgFile << "RTN: " << rinfo->name << " STARTED GET SYMBOLS!!!"<<std::endl;
      if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
      auto symbol_iterator = stapin::get_symbols();
      if(nullptr == symbol_iterator){
        return;
      }
   stapin::Symbol symbol;
    while(stapin::get_next_symbol(symbol_iterator, &symbol)){
        print_symbol_info(symbol, ctx);
    }
     stapin::close_symbol_iterator(symbol_iterator);
    }
    else
    {
      msgFile << "RTN: " << rinfo->name << " SKIPPED GET SYMBOLS!!!"<<std::endl;
    }
  }
  else
  {
    if(KnobAllCalls)
    {
      if(KnobSTAPIN)
      {
        if(!ctx) return;
        //msgFile << "RTN: " << rinfo->name << " CONTINUED GET SYMBOLS!!!"<<std::endl;
        if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
        }
        if(KnobSetContextOnly) return;
        auto symbol_iterator = stapin::get_symbols();
        if(nullptr == symbol_iterator){
          return;
        }
        stapin::close_symbol_iterator(symbol_iterator);
      }
    //else
    //{
      //msgFile << "RTN: " << rtnname << " SKIPPED GET SYMBOLS!!!"<<std::endl;
    //}
    }
  }
}



VOID STAPIN_GetSymbolsStart(IMG img,  VOID *v){
    if(!ProcessImage(img)) return;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
      for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
      {
            if (RTN_Name(rtn) == "_exit" || RTN_Name(rtn) == "exit")
            {
             if(!RTN_IsSafeForProbedInsertion(rtn))
             {
                cerr << "WARNING:Cannot insert probe at "+RTN_Name(rtn)+"\n";
                continue;
             }
               cerr << "Replacing " << RTN_Name(rtn) << " from " << IMG_Name(img) << endl;
               PROTO myexit_proto  = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "_exit", PIN_PARG(int), PIN_PARG_END());
               origExit =  (EXITFUNCPTR)RTN_ReplaceSignatureProbed(rtn, (AFUNPTR)ExitInProbeMode, IARG_PROTOTYPE, myexit_proto,
                        IARG_ORIG_FUNCPTR, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_PTR, v,  IARG_END);
               PROTO_Free(myexit_proto);
                continue;
           }

        if(ProcessRoutine(rtn))
        {
          if(RtnNameMap[ RTN_Name(rtn)])
          {
            // This RTN_Name() was already seen. This may be a case of inlined function. Do not handle.
            cerr <<  RTN_Name(rtn) << " from " << IMG_Name(img) << "was seen before" << endl;
            continue;
          }
          RINFO *trinfo = new RINFO;
          string trname = RTN_Name(rtn);
          trinfo->icount_static = RTN_NumIns(rtn);
          trinfo->name = trname;
          trinfo->count = 0;
          RtnNameMap[ RTN_Name(rtn)] = trinfo;
          if(KnobProbe)
          {
             if(!RTN_IsSafeForProbedInsertion(rtn))
             {
                cerr << "WARNING:Cannot insert probe at "+RTN_Name(rtn)+"\n";
                continue;
             }
        	   RTN_InsertCallProbed(rtn, IPOINT_BEFORE, (AFUNPTR)get_symbols_start, IARG_CONTEXT, IARG_PTR, trinfo,  IARG_END); //GET SYMBOLS START EXAMPLE
          }
          else
          {
        	  RTN_Open(rtn);
        	  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)get_symbols_start, IARG_CONTEXT, IARG_PTR, trinfo,  IARG_END); //GET SYMBOLS START EXAMPLE
        	  RTN_Close(rtn);
          }
	      }
	    }
    }
}


VOID Unload_Image(IMG img, VOID *v){
    stapin::notify_image_unload(img); //USE OF NOTIFY IMAGE UNLOAD
}


VOID Load_Image(IMG img, VOID *v){
    stapin::notify_image_load(img);  //USE OF NOTIFY IMAGE LOAD  
}

 
// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    stapin::notify_thread_start(ctxt, threadid);
}
 
// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    stapin::notify_thread_fini(threadid);
}


VOID Fini(INT32 code, VOID *v)
{
    // called when finished, print the info.
    PrintStats();
    return;
}

// /* ===================================================================== */
// /* Print Help Message                                                    */
// /* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool is a try to connect Pintool and plugin that uses TCF" << endl;
    cerr << endl
         << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }
    
      if(!stapin::init()){
        return 0;
      }
    
    ImageOI = KnobImage.Value();
    RoutineOI = KnobRoutine.Value();
    msgFile.open(KnobMsgFile.Value().c_str(), std::ofstream::out);
    PIN_InitLock(&rtnperf_lock);
   
    
    IMG_AddInstrumentFunction(Load_Image, 0);
    IMG_AddUnloadFunction(Unload_Image, 0);
    IMG_AddInstrumentFunction(STAPIN_GetSymbolsStart, 0); //symbols start

    if(!KnobProbe)
    {
      PIN_AddThreadStartFunction(ThreadStart, 0);
      PIN_AddThreadFiniFunction(ThreadFini, 0);
    }

    //TRACE_AddInstrumentFunction(MonitorVarOI, 0);
    // call FINI when done.
    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    if(KnobProbe)
      PIN_StartProgramProbed();
    else
      PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
