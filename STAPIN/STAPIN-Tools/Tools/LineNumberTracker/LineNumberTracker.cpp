/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "pin.H"
#include "atomic.hpp"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <unordered_map>
#include <stapin.h>
#include <iostream>
#include <string>
#include "LineNumberTracker.H"


using std::cerr;
using std::endl;
using std::hex;
using std::ios;
using std::string;


KNOB<string> KnobImage(KNOB_MODE_WRITEONCE, "pintool", "image", "", "Image of interest");
KNOB<string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Function of interest");
KNOB<string> KnobOutFile(KNOB_MODE_WRITEONCE, "pintool", "outfile", "LNT.out.txt", "Output report in this file.");
KNOB<string> KnobMsgFile(KNOB_MODE_WRITEONCE, "pintool", "msgfile", "LNT.msg.txt", "Output messages in this file.");
std::string ImageOI;
std::string FuncOI;
std::ofstream outFile;
std::ofstream msgFile;

std::map<UINT32, LNT::LINEINFO *> line_map;


void print_linenumber_info(stapin::Source_loc* curSourceLoc){
    msgFile <<"FILE NAME: "<<curSourceLoc->srcFileName<<std::endl;
    msgFile <<"START LINE: "<<curSourceLoc->startLine<<std::endl;
    msgFile <<"END LINE: "<<curSourceLoc->endLine<<std::endl;
}
void print_location_info(stapin::Source_loc* curSourceLoc){
    msgFile <<"FILE NAME: "<<curSourceLoc->srcFileName<<std::endl;
    msgFile <<"START COL: "<<curSourceLoc->startColumn<<std::endl;
    msgFile <<"END COL: "<<curSourceLoc->endColumn<<std::endl;
    msgFile <<"START LINE: "<<curSourceLoc->startLine<<std::endl;
    msgFile <<"END LINE: "<<curSourceLoc->endLine<<std::endl;
    msgFile <<"START  ADDRESS: "<<curSourceLoc->startAddress<<std::endl;
    msgFile <<"END ADDRESS: "<<curSourceLoc->endAddress<<std::endl;
    msgFile <<"NEXT ADDRESS: "<<curSourceLoc->nextAddress<<std::endl;
    msgFile <<"NEXT staement address: "<<curSourceLoc->nextStmtAddress<<std::endl;
    msgFile <<"***********************************************"<<std::endl;
}

VOID get_source_locations_wrapper(ADDRINT startAddress, ADDRINT size_range){
    size_t locCount = 5;
    auto locations = new stapin::Source_loc[locCount];
    //USE OF GET SOURCE LOCATION FUNCTION 
    auto res = stapin::get_source_locations(startAddress, startAddress+size_range, locations,locCount );
    msgFile <<"*******RESULTS OF GET SOURCE LOCATION ARE*****"<<std::endl;
    for (size_t i = 0; i < res; i++)
    {
        auto curSourceLoc = &locations[i];
        print_location_info(curSourceLoc);
    }
    delete[] locations;
}


VOID IncrementCount(UINT64 *countptr)
{
  ATOMIC::OPS::Increment<UINT64>(countptr, 1);
}

VOID ProcessIns(INS ins)
{
    size_t locCount = 5;
    auto locations = new stapin::Source_loc[locCount];
    LNT::LINEINFO * ins_lineinfo[5];
    LNT::LINEINFO * lineinfo;
    ADDRINT addr = INS_Address(ins);
    auto res = stapin::get_source_locations(INS_Address(ins), INS_Address(ins)+INS_Size(ins), locations,locCount );
    //msgFile <<"INS Address:0x"<< std::hex << INS_Address(ins) << " Location count " << std::dec << res << std::endl;
    //msgFile <<"FILE NAME: "<<curSourceLoc->srcFileName<<std::endl;
    //msgFile <<"START LINE: "<<curSourceLoc->startLine<<std::endl;
    //msgFile <<"END LINE: "<<curSourceLoc->endLine<<std::endl;
    for (size_t i = 0; i < res; i++)
    {
        auto curSourceLoc = &locations[i];
        int32_t lineno = curSourceLoc->startLine;
        const char* filename = curSourceLoc->srcFileName;
        if(lineno == 0)
        {
          msgFile << "WARNING: Line number is 0 for  Ins at 0x" << std::hex << addr << std::endl;
        }
        if (line_map.count(lineno) > 0)
        {
          lineinfo = line_map[lineno];   
          if(addr < lineinfo->low_pc) lineinfo->low_pc = addr;
          if(addr > lineinfo->high_pc) lineinfo->high_pc = addr;
          lineinfo->ins_addrs.insert(addr);
        }
        else
        {
          lineinfo = new LNT::LINEINFO;
          lineinfo->lineno = lineno; 
          lineinfo->filename = filename; 
          lineinfo->low_pc = addr;
          lineinfo->high_pc = addr;
          lineinfo->ins_addrs.insert(addr);
          line_map[lineno] = lineinfo;   
        }
        ins_lineinfo[i] = lineinfo;
    }
    for (size_t i=0; i < res; i++) 
    {
      LNT::LINEINFO * outer = ins_lineinfo[i];
      for (size_t j=i+1; j < res; j++) 
      {
        LNT::LINEINFO * inner = ins_lineinfo[j];
        std::cerr << "Ins at 0x" << std::hex << addr << "belongs to " << std::dec << outer->lineno << " and " << std::dec << inner->lineno << std::endl;
        // ins belongs to both outer and inner
      }
    }
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) IncrementCount, IARG_PTR, &(lineinfo->execution_count), IARG_END);
    delete[] locations;
}

VOID getSourceLocation_UseSTAPIN(TRACE trace, VOID *v){
    BBL bbl_head = TRACE_BblHead(trace);

    RTN rtn = TRACE_Rtn(trace);
    if (!RTN_Valid(rtn))  return;
    
    SEC sec = RTN_Sec(rtn);
    if (!SEC_Valid(sec))  return;
    
    IMG img = SEC_Img(sec);
    if (!IMG_Valid(img))  return;

    if(ImageOI.empty() || (IMG_Name(img).find(ImageOI) != std::string::npos))
    {
      if(!FuncOI.empty() && (RTN_Name(rtn).find(FuncOI) != std::string::npos))
      {
        for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
        {
         for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
         {
          ProcessIns(ins);
         }
       }
      }
    }
}

VOID Load_Image(IMG img, VOID *v){
    stapin::notify_image_load(img);  //USE OF NOTIFY IMAGE LOAD  
}

VOID Unload_Image(IMG img, VOID *v){
    stapin::notify_image_unload(img); //USE OF NOTIFY IMAGE UNLOAD
}
 
// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    stapin::notify_thread_start(ctxt, threadid); //USE OF NOTIFY THREAD START FUNCTION 
}
 
// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    stapin::notify_thread_fini(threadid); //USE OF NOTIFY THREAD FINI FUNCTION 
}



VOID Fini(INT32 code, VOID *v)
{
    // called when finished, print the info.
    // Using iterators
    // Outer loop
    for (std::map<UINT32, LNT::LINEINFO *>::iterator outer_it = line_map.begin(); outer_it != line_map.end(); ++outer_it) 
     {
      //std::cout << "Outer LineNumber: " << outer_it->first << " LININFO PTR: " << outer_it->second << std::endl;
     if(outer_it->first == 0)
     {
       ADDRINT addr  = *(outer_it->second->ins_addrs.begin()); 
       cerr << "WARNING: Line number is 0 for  Ins at 0x" << std::hex << addr << std::endl;
       msgFile << "WARNING: Line number is 0 for  Ins at 0x" << std::hex << addr << std::endl;
     }
     for (ADDRINT ins_addr : outer_it->second->ins_addrs) 
     {
       //std::cout << " INS address 0x" << std::hex << ins_addr << endl;
       for (std::map<UINT32, LNT::LINEINFO *>::iterator inner_it = outer_it; inner_it != line_map.end(); ++inner_it) 
       {
        if (outer_it != inner_it) 
        { // Optionally skip the current element of the outer loop
          //std::cout << "  Inner LineNumber: " << inner_it->first << " LININFO PTR: " << inner_it->second << std::endl;
          if(inner_it->second->ins_addrs.count(ins_addr) > 0 ) 
          {
              // this captures the (rare) case ins_addrs belongs to both lines
              msgFile << " INS add 0x" << std::hex << ins_addr << " RARE  Interference LineNumber: " << std::dec << outer_it->first << " and " << std::dec << inner_it->first << endl;
          }
          if( (ins_addr >= inner_it->second->low_pc) && (ins_addr <= inner_it->second->high_pc))
          {
              msgFile << " INS add 0x" << std::hex << ins_addr << " COMMON  Interference LineNumber: " << std::dec << outer_it->first << " and " << std::dec << inner_it->first << endl;
              msgFile << "\t inner:low_pc 0x" << std::hex << inner_it->second->low_pc << " inner:high_pc 0x" << std::hex << inner_it->second->high_pc <<  endl;
              outer_it->second->static_interference_count++;
              inner_it->second->static_interference_count++;
          }
        }
      } //inner_it
     }
    } //outer_it

    bool first=true;
    for (auto it = line_map.begin(); it != line_map.end(); ++it) {
        if(first)
        {
         it->second->FiniHeader(&outFile); 
         first=false;
        }
         it->second->Fini(&outFile); 
    }
    return;
}

// /* ===================================================================== */
// /* Print Help Message                                                    */
// /* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool is a try to connect Pintool and plugin that uses TCF" << std::endl;
    cerr << std::endl
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
    
    if(!stapin::init()){ //USE OF INIT STAPIN FUNCTION 
        return 0;
    }
    
    ImageOI = KnobImage.Value();
    FuncOI = KnobFunc.Value();
    outFile.open(KnobOutFile.Value().c_str(), std::ofstream::out);
    msgFile.open(KnobMsgFile.Value().c_str(), std::ofstream::out);
    msgFile << "ImageOI: " << ImageOI << endl;
    msgFile << "FuncOI: " << FuncOI << endl;
    IMG_AddInstrumentFunction(Load_Image, 0);
    IMG_AddUnloadFunction(Unload_Image, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    //IMG_AddInstrumentFunction(getSourceLocation_UseSTAPIN, 0);
    TRACE_AddInstrumentFunction(getSourceLocation_UseSTAPIN, 0);
    
    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
