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

#define FIRST_FUNCTION  "foo"
#define SECOND_FUNCTION "foo2"



static CONTEXT myCtx; //will contain the previous context
static bool flag = false; // to know if we already assigned the context.

void print_symbol_info(stapin::Symbol symbol){
    stapin::Symbol Tsymbol; 
    std::cout<<"*******************************************************************"<<std::endl;
    std::cout<<"SYMBOL NAME: "<<symbol.name<<std::endl;
    std::cout<<"SYMBOL SIZE: "<<std::dec<<symbol.size<<std::endl;
    std::cout<<"SYMBOL MEMORY: "<<std::hex<<symbol.memory<<std::endl;
    std::cout<<"SYMBOL unique id: "<<symbol.uniqueId<<std::endl;
    std::cout<<"SYMBOL flags: "<<static_cast<int>(symbol.flags)<<std::endl;
    std::cout<<"SYMBOL type: "<<static_cast<int>(symbol.type)<<std::endl;
    std::cout<<"SYMBOL type unique ID : "<<symbol.typeUniqueId<<std::endl;
    if(stapin::get_symbol_by_id(symbol.typeUniqueId, &Tsymbol)){
        std::cout<<"TYPE IS "<<Tsymbol.name<<std::endl;
    }

}

VOID get_symbols_start( CONTEXT* ctx){
    if(!flag)
    {
        //Assign the current context which is the context of the first function.
        PIN_SaveContext(ctx, &myCtx);
        flag = true;
    }
     if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
    std::cout<<"STARTED GET SYMBOLS!!!"<<std::endl;
    
    auto symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    stapin::Symbol symbol; 
    std::cout<<"RESULT OF SYMBOLS!!!"<<std::endl;
    while(stapin::get_next_symbol(symbolIterator, &symbol)){
        print_symbol_info(symbol);
    }
    stapin::close_symbol_iterator(symbolIterator);
}

VOID get_symbols_start_2(CONTEXT* ctx){
    std::cout<<"************ TRYING WITH OLD CONTEXT!********"<<std::endl;
    //***************Use the previous context!******************
     if(!stapin::set_context(&myCtx, stapin::E_context_update_method::FULL)){
        return;
    }
    std::cout<<"STARTED GET SYMBOLS!!!"<<std::endl;
    
    auto symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    stapin::Symbol symbol; 
    std::cout<<"RESULT OF SYMBOLS!!!"<<std::endl;
    while(stapin::get_next_symbol(symbolIterator, &symbol)){
        print_symbol_info(symbol);
    }
    stapin::close_symbol_iterator(symbolIterator);

    std::cout<<"************ TRYING WITH NEW CONTEXT!********"<<std::endl;
    //***************Use the new context!******************
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
    std::cout<<"STARTED GET SYMBOLS!!!"<<std::endl;
    
    symbolIterator = stapin::get_symbols();
    if(nullptr == symbolIterator){
        return;
    }
    std::cout<<"RESULT OF SYMBOLS!!!"<<std::endl;
    while(stapin::get_next_symbol(symbolIterator, &symbol)){
        print_symbol_info(symbol);
    }
    stapin::close_symbol_iterator(symbolIterator);

}

VOID STAPIN_Get_Symbols_Start(IMG img,  VOID *v){
    RTN myFunc = RTN_FindByName(img, FIRST_FUNCTION); 
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_AFTER, (AFUNPTR)get_symbols_start, IARG_CONTEXT,  IARG_END); //GET SYMBOLS START EXAMPLE
        RTN_Close(myFunc);
    }
    RTN myFunc_2 = RTN_FindByName(img, SECOND_FUNCTION); 
    if (RTN_Valid(myFunc_2)) // found
    {
        RTN_Open(myFunc_2);
        RTN_InsertCall(myFunc_2, IPOINT_AFTER, (AFUNPTR)get_symbols_start_2, IARG_CONTEXT,  IARG_END); //GET SYMBOLS START EXAMPLE
        RTN_Close(myFunc_2);
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
    IMG_AddUnloadFunction(Unload_Image, 0);
    PIN_AddThreadStartFunction(Thread_Start, 0);
    PIN_AddThreadFiniFunction(Thread_Fini, 0);
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, After_Fork_In_Child, 0);
    IMG_AddInstrumentFunction(STAPIN_Get_Symbols_Start, 0); //symbols start

    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */