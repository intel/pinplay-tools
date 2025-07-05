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

void print_expression_info(stapin::Expression const& expression){
    stapin::Symbol Tsymbol {0}; 
    stapin::Symbol symbol {0};
    std::cout<<"*******************************************************************"<<std::endl;
    std::cout<<"Expression Name: "<<expression.name<<std::endl;
    std::cout<<"Expression Parent Name: "<<expression.parentName<<std::endl;
    std::cout<<"Expression Size: "<<std::dec<<expression.size<<std::endl;
    std::cout<<"Expression Level: "<<std::dec<<expression.level<<std::endl;
    std::cout<<"Expression TypeFlag: "<<std::dec<<(int)expression.typeFlag<<std::endl;
    std::cout<<"Expression Type Unique ID : "<<expression.typeUniqueID<<std::endl;
    std::cout<<"Expression Symbol Unique ID : "<<expression.symbolUniqueID<<std::endl;
    if(stapin::get_symbol_by_id(expression.typeUniqueID, &Tsymbol))
    {
        std::cout<<"TYPE IS "<<Tsymbol.name<<std::endl;
    }
    if(stapin::get_symbol_by_id(expression.symbolUniqueID, &symbol))
    {
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
    switch (expression.typeFlag)
    {
    case stapin::E_expression_type::UNSIGNED:
    {
        auto val = reinterpret_cast<const unsigned*>(expression.data);
        std::cout<<"Expression value: "<<*val<<std::endl;
        break;
    }
    case stapin::E_expression_type::DOUBLE:
    {
        auto val = reinterpret_cast<const double*>(expression.data);
        std::cout<<"Expression value: "<<*val<<std::endl;
        break;
    }
    case stapin::E_expression_type::ENUM:
    case stapin::E_expression_type::SIGNED:
    {
        auto val = reinterpret_cast<const signed*>(expression.data);
        std::cout<<"Expression value: "<<*val<<std::endl;
        break;
    }
    // case stapin::E_expression_type::POINTER:
    // {        
    //     char** val = reinterpret_cast<char**>(expression.data);
    //     std::cout << "Expression value: " << *val << std::endl;
    //     break;
    // }
    default:
        break;
    }
}

VOID get_expressions_start(CONTEXT* ctx){
    std::cout<<"STARTED GET EXPRESSION!!!"<<std::endl;
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
    auto epxressionIterator = stapin::evaluate_expression("simpleArr");
    if(nullptr == epxressionIterator){
        return;
    }
    stapin::Expression expr {0}; 
    std::cout<<"RESULT OF EXPRESSIONS!!!"<<std::endl;
    while(stapin::get_next_expression(epxressionIterator, &expr)){
        print_expression_info(expr);
    }
    stapin::close_expression_iterator(epxressionIterator);
}


VOID STAPIN_Get_Expressions_Start(IMG img,  VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); 
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_AFTER, (AFUNPTR)get_expressions_start, IARG_CONTEXT,  IARG_END); 
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
    PIN_InitSymbols();
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }
    
    if(!stapin::init()){
        return 0;
    }
    IMG_AddInstrumentFunction(Load_Image, 0);
    IMG_AddUnloadFunction(Unload_Image, 0);
    PIN_AddThreadStartFunction(Thread_Start, 0);
    PIN_AddThreadFiniFunction(Thread_Fini, 0);
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, After_Fork_In_Child, 0);
    IMG_AddInstrumentFunction(STAPIN_Get_Expressions_Start, 0); 
    // call FINI when done.
    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */