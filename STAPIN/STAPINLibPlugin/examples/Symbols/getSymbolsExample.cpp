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


using std::cerr;
using std::endl;
using std::hex;
using std::ios;
using std::string;
using namespace std;

#define MAIN "main"
//#define FUNC "func"

KNOB<string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Function of interest");
string FuncOI="";

void print_symbol_info(stapin::Symbol const& symbol){
    stapin::Symbol Tsymbol {0}; 
    stapin::Symbol BTsymbol {0}; 
    std::cout<<"*******************************************************************"<<std::endl;
    std::cout<<"SYMBOL NAME: "<<symbol.name<<std::endl;
    std::cout<<"SYMBOL SIZE: "<<std::dec<<symbol.size<<std::endl;
    std::cout<<"SYMBOL MEMORY: "<<std::hex<<symbol.memory<<std::endl;
    std::cout<<"SYMBOL unique id: "<<symbol.uniqueId<<std::endl;
    std::cout<<"SYMBOL flags: "<<static_cast<int>(symbol.flags)<<std::endl;
    std::cout<<"SYMBOL type: "<<static_cast<int>(symbol.type)<<std::endl;
    std::cout<<"SYMBOL type unique ID : "<<symbol.typeUniqueId<<std::endl;
    std::cout<<"SYMBOL base type unique ID : "<<symbol.baseTypeUniqueId<<std::endl;
    if(stapin::get_symbol_by_id(symbol.typeUniqueId, &Tsymbol)){
        std::cout<<"TYPE IS "<<Tsymbol.name<<std::endl;
    }
    if(!std::string(symbol.baseTypeUniqueId).empty() && stapin::get_symbol_by_id(symbol.baseTypeUniqueId, &BTsymbol)){
        std::cout<<"BASE TYPE IS "<<BTsymbol.name<<std::endl;
    }

}

VOID get_symbols_start( CONTEXT* ctx){
    std::cout<<"STARTED GET SYMBOLS!!!"<<std::endl;
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    stapin::Symbol symbol{0}; 
    std::cout<<"RESULT OF SYMBOLS!!!"<<std::endl;
    while(stapin::get_next_symbol(symbolIterator, &symbol)){
        print_symbol_info(symbol);
    }
    stapin::close_symbol_iterator(symbolIterator);
}


// THIS IS FINDING VARIABLES INSIDE SCOPE LIKE STRUCT FOR EXAMPLE
VOID get_symbols_by_name( CONTEXT* ctx){
    std::cout<<"STARTED GET SYMBOL BY NAME!!!"<<std::endl;
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbolIterator_1 =  stapin::find_symbol_by_name("referenceCheck"); //find the struct variable
    if(nullptr == symbolIterator_1 ){
        std::cout<<"COULD NOT FIND SYMBOL BY NAME"<<std::endl;
        return;
    }
    
    stapin::Symbol symbol {0}; 
    if (!stapin::get_next_symbol(symbolIterator_1, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    print_symbol_info(symbol);
    
    auto symbolIterator_2 =  stapin::get_symbols(&symbol); //children of the struct
    if(nullptr == symbolIterator_2 ){
        stapin::close_symbol_iterator(symbolIterator_1);
        std::cout<<"COULD NOT FIND SYMBOL BY NAME"<<std::endl;
        return;
    }
    
    while (stapin::get_next_symbol(symbolIterator_2, &symbol)){
        print_symbol_info(symbol);
    } //get the symbol itself of the struct variable 
    stapin::close_symbol_iterator(symbolIterator_1);
    stapin::close_symbol_iterator(symbolIterator_2);
}


// THIS IS FINDING VARIABLES USING ID 
VOID get_symbols_by_id( CONTEXT* ctx){
    std::cout<<"STARTED GET SYMBOL BY ID!!!"<<std::endl;
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    
    stapin::Symbol symbol {0}; 
    if (!stapin::get_next_symbol(symbolIterator, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    print_symbol_info(symbol);

    if(!stapin::get_symbol_by_id(symbol.uniqueId, &symbol)){
        std::cout<<"COULD NOT FIND SYMBOL BY ID"<<std::endl;
        stapin::close_symbol_iterator(symbolIterator);
        return;
    }
    print_symbol_info(symbol);
    stapin::close_symbol_iterator(symbolIterator);
}

// THIS IS FINDING VARIABLES USING ADDRESS
VOID get_symbols_by_address( CONTEXT* ctx){
    std::cout<<"STARTED GET SYMBOL BY ADDRESS!!!"<<std::endl;
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    
    stapin::Symbol symbol {0}; 
    if (!stapin::get_next_symbol(symbolIterator, &symbol)){
        return;
    } //get the symbol itself of the struct variable 
    print_symbol_info(symbol);

    if(!stapin::get_symbol_by_address(symbol.memory, &symbol)){
        std::cout<<"COULD NOT FIND SYMBOL BY ADDRESS"<<std::endl;
        stapin::close_symbol_iterator(symbolIterator);
        return;
    }
    print_symbol_info(symbol);
    stapin::close_symbol_iterator(symbolIterator);
}

VOID get_symbols_by_rtn(CONTEXT* ctx, ADDRINT rtnAddress){
    std::cout<<"STARTED GET SYMBOL BY RTN!!!"<<std::endl;
    PIN_LockClient();
    RTN rtn = RTN_FindByAddress(rtnAddress);
    PIN_UnlockClient();
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    stapin::Symbol symbol {0}; 
    if(!stapin::get_rtn_symbol(rtn, &symbol)){
        std::cout<<"COULD NOT FIND SYMBOL BY RTN"<<std::endl;
        return;
    }
    print_symbol_info(symbol);
}

VOID get_symbols_by_reg(CONTEXT* ctx){
    std::cout<<"STARTED GET SYMBOL BY REG!!!"<<std::endl;
   if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    
    stapin::Symbol symbol {0}; 
    while(stapin::get_next_symbol(symbolIterator, &symbol)){
        stapin::Symbol resSymbol;
        if(stapin::E_symbol_flags::VALUE_IN_REG != symbol.flags || 
           !stapin::get_symbol_by_reg(symbol.reg, &resSymbol)   ||
           0 != strcmp(symbol.uniqueId, resSymbol.uniqueId)){
        std::cout<<"COULD NOT FIND SYMBOL BY REG FOR SYMBOL "<<symbol.name<<std::endl;
        continue;
    }
        print_symbol_info(resSymbol);
    }
    stapin::close_symbol_iterator(symbolIterator);
}





VOID STAPIN_Get_Symbols_Start(IMG img,  VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_start, IARG_CONTEXT,  IARG_END); //GET SYMBOLS START EXAMPLE
        RTN_Close(myFunc);
        
    }
}

VOID STAPIN_Get_Symbols_By_Name(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_name, IARG_CONTEXT,  IARG_END);       //GET SYMBOLS BY NAME EXAMPLE
        RTN_Close(myFunc);
        
    }
}
VOID STAPIN_Get_Symbols_By_Address(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_address, IARG_CONTEXT,  IARG_END);    //GET SYMBOLS BY ADDRESS EXAMPLE
        RTN_Close(myFunc);
        
    }
}
VOID STAPIN_Get_Symbols_By_ID(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_symbols_by_id, IARG_CONTEXT,  IARG_END);         //GET SYMBOLS BY ID EXAMPLE
        RTN_Close(myFunc);
        
    }
}



VOID STAPIN_Get_Symbols_By_Reg(IMG img, VOID *v ){
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



VOID STAPIN_Get_Symbol_By_Rtn(IMG img, VOID *v){
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
VOID Thread_Start(THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    stapin::notify_thread_start(ctxt, threadid);
}
 
// This routine is executed every time a thread is destroyed.
VOID Thread_Fini(THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    stapin::notify_thread_fini(threadid);  
}



VOID After_Fork_In_Child(THREADID threadid, const CONTEXT* ctxt, VOID* arg)
{
    stapin::notify_fork_in_child(ctxt, threadid);
}



VOID Fini(INT32 code, VOID *v)
{
    // called when finished, print the info.
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
    IMG_AddInstrumentFunction(Load_Image, 0);
    IMG_AddInstrumentFunction(STAPIN_Get_Symbols_Start, 0); //symbols start
    IMG_AddInstrumentFunction(STAPIN_Get_Symbols_By_Name, 0); //symbols by name
    IMG_AddInstrumentFunction(STAPIN_Get_Symbols_By_Address, 0); //symbols by adress
    IMG_AddInstrumentFunction(STAPIN_Get_Symbols_By_ID, 0); //symbols by ID
    // IMG_AddInstrumentFunction(STAPIN_Get_Symbols_By_Reg, 0); //symbols by reg
    IMG_AddInstrumentFunction(STAPIN_Get_Symbol_By_Rtn, 0); //symbols by rtn
    IMG_AddUnloadFunction(Unload_Image, 0);
    PIN_AddThreadStartFunction(Thread_Start, 0);
    PIN_AddThreadFiniFunction(Thread_Fini, 0);
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, After_Fork_In_Child, 0);
    // call FINI when done.
    PIN_AddFiniFunction(Fini, 0);
    FuncOI = KnobFunc.Value();
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
