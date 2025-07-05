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
#include "SymbolLocationTracker.H"


using std::cerr;
using std::endl;
using std::hex;
using std::ios;
using std::string;
using namespace std;
using namespace SLT;


KNOB<string> KnobImage(KNOB_MODE_WRITEONCE, "pintool", "image", "", "Image of interest");
KNOB<string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Function of interest");
KNOB<string> KnobVar(KNOB_MODE_WRITEONCE, "pintool", "var", "", "Variable of interest");
KNOB<INT32> KnobLineNo(KNOB_MODE_WRITEONCE, "pintool", "lineno", "-1", "Symbol visible at this lineno.");
KNOB<string> KnobOutFile(KNOB_MODE_WRITEONCE, "pintool", "outfile", "SLT.out.txt", "Output report in this file.");
KNOB<string> KnobMsgFile(KNOB_MODE_WRITEONCE, "pintool", "msgfile", "SLT.msg.txt", "Output messages in this file.");
string ImageOI="";
string FuncOI="";
string VarOI="";
ofstream outFile;
ofstream msgFile;

VARLOCINFO *LocVarOI;

PIN_LOCK SLT_lock;
mutex mymutex;

#define MAIN "main"


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

static bool first_time = true;
VOID get_symbols_start( CONTEXT* ctx){
const std::lock_guard<std::mutex> lock(mymutex);
    if(!first_time) return;
    first_time = false;
    msgFile <<"STARTED GET SYMBOLS!!!"<<std::endl;
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator = stapin::get_symbols();
    if(nullptr == symbol_iterator){
        return;
    }
    stapin::Symbol symbol; 
    msgFile <<"RESULT OF SYMBOLS!!!"<<std::endl;
    while(stapin::get_next_symbol(symbol_iterator, &symbol)){
        print_symbol_info(symbol, ctx);
    }
    stapin::close_symbol_iterator(symbol_iterator);
}


VOID get_symbols_by_name( CONTEXT* ctx){
    msgFile <<"STARTED GET SYMBOL BY NAME!!!"<<std::endl;
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator_1 =  stapin::find_symbol_by_name("myStruct"); //find the struct variable
    if(nullptr == symbol_iterator_1 ){
        msgFile <<"COULD NOT FIND SYMBOL BY NAME"<<std::endl;
        return;
    }
    
    stapin::Symbol symbol; 
    if (!stapin::get_next_symbol(symbol_iterator_1, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    print_symbol_info(symbol, ctx);
    
    auto symbol_iterator_2 =  stapin::get_symbols(&symbol); //children of the struct
    if(nullptr == symbol_iterator_2 ){
        stapin::close_symbol_iterator(symbol_iterator_1);
        msgFile <<"COULD NOT FIND SYMBOL BY NAME"<<std::endl;
        return;
    }
    
    while (stapin::get_next_symbol(symbol_iterator_2, &symbol)){
        print_symbol_info(symbol, ctx);
    } //get the symbol itself of the struct variable 
    stapin::close_symbol_iterator(symbol_iterator_1);
    stapin::close_symbol_iterator(symbol_iterator_2);
}


// THIS IS FINDING VARIABLES USING ID 
VOID get_symbols_by_id( CONTEXT* ctx){
    msgFile <<"STARTED GET SYMBOL BY ID!!!"<<std::endl;
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator = stapin::get_symbols();
    if(nullptr == symbol_iterator){
        return;
    }
    
    stapin::Symbol symbol; 
    if (!stapin::get_next_symbol(symbol_iterator, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    print_symbol_info(symbol, ctx);

    if(!stapin::get_symbol_by_id(symbol.uniqueId, &symbol)){
        msgFile <<"COULD NOT FIND SYMBOL BY ID"<<std::endl;
        stapin::close_symbol_iterator(symbol_iterator);
        return;
    }
    print_symbol_info(symbol, ctx);
    stapin::close_symbol_iterator(symbol_iterator);
}

// THIS IS FINDING VARIABLES USING ADDRESS
VOID get_symbols_by_address( CONTEXT* ctx){
    msgFile <<"STARTED GET SYMBOL BY ADDRESS!!!"<<std::endl;
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator = stapin::get_symbols();
    if(nullptr == symbol_iterator){
        return;
    }
    
    stapin::Symbol symbol; 
    if (!stapin::get_next_symbol(symbol_iterator, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    print_symbol_info(symbol, ctx);

    if(!stapin::get_symbol_by_address(symbol.memory, &symbol)){
        msgFile <<"COULD NOT FIND SYMBOL BY ADDRESS"<<std::endl;
        stapin::close_symbol_iterator(symbol_iterator);
        return;
    }
    print_symbol_info(symbol, ctx);
    stapin::close_symbol_iterator(symbol_iterator);
}

VOID get_symbols_by_rtn(CONTEXT* ctx, ADDRINT rtnAddress){
    msgFile <<"STARTED GET SYMBOL BY RTN!!!"<<std::endl;
    PIN_LockClient();
    RTN rtn = RTN_FindByAddress(rtnAddress);
    PIN_UnlockClient();
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    stapin::Symbol symbol; 
    if(!stapin::get_rtn_symbol(rtn, &symbol)){
        msgFile <<"COULD NOT FIND SYMBOL BY RTN"<<std::endl;
        return;
    }
    print_symbol_info(symbol, ctx);
}

VOID get_symbols_by_reg(CONTEXT* ctx){
    msgFile <<"STARTED GET SYMBOL BY REG!!!"<<std::endl;
   if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator = stapin::get_symbols();
    if(nullptr == symbol_iterator){
        return;
    }
    
    stapin::Symbol symbol; 
    while(stapin::get_next_symbol(symbol_iterator, &symbol)){
        stapin::Symbol res_symbol;
        if(stapin::E_symbol_flags::VALUE_IN_REG != symbol.flags || 
           !stapin::get_symbol_by_reg(symbol.reg, &res_symbol)   ||
           0 != strcmp(symbol.uniqueId, res_symbol.uniqueId)){
        msgFile <<"COULD NOT FIND SYMBOL BY REG FOR SYMBOL "<<symbol.name<<std::endl;
        continue;
    }
        print_symbol_info(res_symbol, ctx);
    }
    stapin::close_symbol_iterator(symbol_iterator);
}


static unordered_set<ADDRINT> insset;

VOID TrackVarOI(ADDRINT addr, CONTEXT* ctxt, THREADID tid)
{
const std::lock_guard<std::mutex> lock(mymutex);
 if(insset.find(addr) == insset.end())
 {
    insset.insert(addr);
    if(!set_context(ctxt,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator_1 =  stapin::find_symbol_by_name(VarOI.c_str()); //find the struct variable
    if(nullptr == symbol_iterator_1 ){
        msgFile <<"COULD NOT FIND SYMBOL " << VarOI << "BY NAME"<<std::endl;
        return;
    }
    stapin::Symbol symbol; 
    if (!stapin::get_next_symbol(symbol_iterator_1, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    //PIN_GetLock(&SLT_lock, tid);
    LocVarOI->name =  VarOI;
    LocVarOI->uniqueId =  symbol.uniqueId;
    //PIN_ReleaseLock(&SLT_lock);
    msgFile <<"INS Address: 0x" << hex << addr << endl;
    switch(symbol.flags){
      case (stapin::E_symbol_flags::VALUE_IN_REG):
      {
         msgFile <<"variable " << symbol.name << " is in REG " << REG_StringShort(REG_FullRegName(symbol.reg)) << endl;
         //PIN_GetLock(&SLT_lock, tid);
         LocVarOI->location_count[SLT::REGISTER]+=1;
         LocVarOI->static_location_count[SLT::REGISTER]+=1;
         LocVarOI->loc_map[addr] = SLT::REGISTER;
         //PIN_ReleaseLock(&SLT_lock);
         break;
      }
      case (stapin::E_symbol_flags::VALUE_IN_MEM):
      {
         //PIN_GetLock(&SLT_lock, tid);
         LocVarOI->location_count[SLT::MEMORY]+=1;
         LocVarOI->static_location_count[SLT::MEMORY]+=1;
         LocVarOI->loc_map[addr] = SLT::MEMORY;
         //PIN_ReleaseLock(&SLT_lock);
         msgFile <<" variable " << VarOI <<  " is in Memory at 0x" << hex << symbol.memory << endl;
#if 0
        if (symbol.memory)
        {
         size_t size = symbol.size;
         if (size == 4 )
            msgFile <<" " << VarOI <<  " 4-byte decimal value " << dec << *((UINT32  *)symbol.memory) << endl;
         if (size == 8 )
            msgFile <<" " << VarOI <<  " 8-byte decimal value " << dec << *((UINT64  *)symbol.memory) << endl;
        }
#endif 
         break;
      }
      case (stapin::E_symbol_flags::OPTIMIZED_AWAY):
      {
         msgFile <<" variable " << VarOI <<  " is opttimized  away" << endl;
         //PIN_GetLock(&SLT_lock, tid);
         LocVarOI->location_count[SLT::OPTIMIZED_AWAY]+=1;
         LocVarOI->static_location_count[SLT::OPTIMIZED_AWAY]+=1;
         LocVarOI->loc_map[addr] = SLT::OPTIMIZED_AWAY;
         //PIN_ReleaseLock(&SLT_lock);
         break;
        }
      case (stapin::E_symbol_flags::DW_STACK_VALUE):
      { 
        msgFile <<" variable " << VarOI <<  " is  dwarf stack value" << endl;
        LocVarOI->location_count[SLT::DWARF_STACK_VAL]+=1;
         LocVarOI->static_location_count[SLT::DWARF_STACK_VAL]+=1;
         LocVarOI->loc_map[addr] = SLT::DWARF_STACK_VAL;
        break;
      }
      case (stapin::E_symbol_flags::IS_BIT_FIELD):
      { 
        msgFile <<" variable " << VarOI <<  " is bit field" << endl;
        LocVarOI->location_count[SLT::BIT_FIELD]+=1;
         LocVarOI->static_location_count[SLT::BIT_FIELD]+=1;
         LocVarOI->loc_map[addr] = SLT::BIT_FIELD;
        break;
      } 
      case (stapin::E_symbol_flags::LOCATION_FETCH_ERROR):
       {
         msgFile <<" variable " << VarOI <<  " is at an FETCH_ERROR location" << endl;
         //PIN_GetLock(&SLT_lock, tid);
         LocVarOI->location_count[SLT::FETCH_ERROR]+=1;
         LocVarOI->static_location_count[SLT::FETCH_ERROR]+=1;
         LocVarOI->loc_map[addr] = SLT::FETCH_ERROR;
         //PIN_ReleaseLock(&SLT_lock);
         break;
        }
       default:
       {
         msgFile <<" variable " << VarOI <<  " is at an unknown location" << endl;
         //PIN_GetLock(&SLT_lock, tid);
         LocVarOI->location_count[SLT::UNKNOWN]+=1;
         LocVarOI->static_location_count[SLT::UNKNOWN]+=1;
         LocVarOI->loc_map[addr] = SLT::UNKNOWN;
         //PIN_ReleaseLock(&SLT_lock);
         break;
        }
    }
    msgFile <<" LOCATION: variable " << VarOI << " " << std::dec << LocVarOI->loc_map[addr] << " " << SLT::LocName[LocVarOI->loc_map[addr]] << endl;
 }
 else
 {
         //PIN_GetLock(&SLT_lock, tid);
         enum SLT::LOCATION  loc = LocVarOI->loc_map[addr];
         ATOMIC::OPS::Increment<UINT64>(&(LocVarOI->location_count[loc]),1);
         //PIN_ReleaseLock(&SLT_lock);
 }
}


VOID ProcessIns(INS ins)
{
 bool entry_handled = false;
  if(!VarOI.empty())
  {
    if (INS_IsValidForIpointAfter(ins))
    {
      INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR) TrackVarOI, IARG_ADDRINT, INS_Address(ins), IARG_CONTEXT, IARG_THREAD_ID, IARG_END);
    }
    else
    {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) TrackVarOI, IARG_ADDRINT, INS_Address(ins), IARG_CONTEXT, IARG_END);
    }
  }
  if(!entry_handled)
  {
    size_t locCount = 5;
    auto locations = new stapin::Source_loc[locCount];
    ADDRINT addr = INS_Address(ins);
    auto res = stapin::get_source_locations(INS_Address(ins), INS_Address(ins)+INS_Size(ins), locations,locCount );
    for (size_t i = 0; i < res; i++)
    {
        auto curSourceLoc = &locations[i];
        int32_t lineno = curSourceLoc->startLine;
        if(KnobLineNo.Value() > 0)
        {
          if(lineno == 0)
          {
            msgFile << "WARNING: Line number is 0 for  Ins at 0x" << std::hex << addr << std::endl;
          }
          if(lineno == KnobLineNo.Value())
          {
            entry_handled = true;
            msgFile << "Instrumenting  line number " << std::dec << lineno << " Ins at 0x" << std::hex << addr << std::endl;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) get_symbols_start, IARG_CONTEXT, IARG_END);
          }
        }
    }
  }
}


VOID MonitorVarOI(TRACE trace, VOID *v){
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

VOID STAPIN_GetSymbolsStart(IMG img,  VOID *v){
    if(ImageOI.empty() || (IMG_Name(img).find(ImageOI) != string::npos))
    {
    RTN myFunc = RTN_FindByName(img, FuncOI.c_str()); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        msgFile <<"Found/instrumenting " << FuncOI << std::endl;
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_start, IARG_CONTEXT,  IARG_END); //May not actually be called as RTN entry is not always executed
      if( !VarOI.empty())
      {
        msgFile << "Tracking " << ImageOI << ":" << FuncOI << "():" << VarOI << endl;
      }
      RTN_Close(myFunc);
    }
  }
}

VOID STAPIN_GetSymbolsByName(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_name, IARG_CONTEXT,  IARG_END);       //GET SYMBOLS BY NAME EXAMPLE
        RTN_Close(myFunc);
        
    }
}
VOID STAPIN_GetSymbolsByAddress(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_address, IARG_CONTEXT,  IARG_END);    //GET SYMBOLS BY ADDRESS EXAMPLE
        RTN_Close(myFunc);
        
    }
}
VOID STAPIN_GetSymbolsByID(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_id, IARG_CONTEXT,  IARG_END);         //GET SYMBOLS BY ID EXAMPLE
        RTN_Close(myFunc);
        
    }
}



VOID STAPIN_GetSymbolsByReg(IMG img, VOID *v ){
    RTN myFunc = RTN_FindByName(img, MAIN); 
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        for (INS ins = RTN_InsHead(myFunc); INS_Valid(ins); ins = INS_Next(ins)) // this is to find by reg ! 
            {
                 if (INS_IsDirectCall(ins)) {
                    auto targetAddr = INS_DirectControlFlowTargetAddress(ins);
                    // Find the routine by the target address
                    RTN rtn = RTN_FindByAddress(targetAddr);
                    auto rtnName = RTN_Name(rtn);
                    if(FuncOI == rtnName){
                        IPOINT where = IPOINT_AFTER;
                        if (!INS_IsValidForIpointAfter(ins)) where = IPOINT_TAKEN_BRANCH;
                        INS_InsertCall(ins, where, (AFUNPTR)get_symbols_by_reg, IARG_CONTEXT, IARG_END);  //GET SYMBOLS BY REG EXAMPLE
                    } 
                }
            }
        RTN_Close(myFunc);
    }
}



VOID STAPIN_GetSymbolByRtn(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, FuncOI.c_str());
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_rtn, IARG_CONTEXT, IARG_ADDRINT, RTN_Address(myFunc),  IARG_END);  //GET SYMBOLS BY RTN EXAMPLE
        RTN_Close(myFunc);
        
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
    msgFile << "Static Icount: " << dec << insset.size() << endl;
    LocVarOI->Fini(&outFile);
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
    
    PIN_InitLock(&SLT_lock);
    ImageOI = KnobImage.Value();
    FuncOI = KnobFunc.Value();
    VarOI = KnobVar.Value();
    LocVarOI = new VARLOCINFO;
    outFile.open(KnobOutFile.Value().c_str(), std::ofstream::out);
    msgFile.open(KnobMsgFile.Value().c_str(), std::ofstream::out);
   
    
    IMG_AddInstrumentFunction(Load_Image, 0);
    IMG_AddUnloadFunction(Unload_Image, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    IMG_AddInstrumentFunction(STAPIN_GetSymbolsStart, 0); //symbols start
    TRACE_AddInstrumentFunction(MonitorVarOI, 0);
    // call FINI when done.
    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
