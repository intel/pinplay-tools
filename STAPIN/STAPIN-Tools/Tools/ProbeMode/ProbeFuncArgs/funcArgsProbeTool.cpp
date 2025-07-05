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
#include <string>

#define MAIN "main"
//#define FUNC_NAME "foo"
KNOB<std::string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Function of interest");

std::string FuncOI="";

void print_symbol_info(stapin::Symbol symbol){
    stapin::Symbol Tsymbol; 
    std::cout<<"*******************************************************************"<<std::endl;
    std::cout<<"Symbol Name: "<<symbol.name<<std::endl;
    std::cout<<"Symbol Size: "<<std::dec<<symbol.size<<std::endl;
    std::cout<<"Symbol Memory: "<<std::hex<<symbol.memory<<std::endl;
    std::cout<<"Symbol unique id: "<<symbol.uniqueId<<std::endl;
    std::cout<<"Symbol flags: "<<static_cast<int>(symbol.flags)<<std::endl;
    std::cout<<"Symbol type: "<<static_cast<int>(symbol.type)<<std::endl;
    std::cout<<"Symbol type unique ID : "<<symbol.typeUniqueId<<std::endl;
    if(stapin::get_symbol_by_id(symbol.typeUniqueId, &Tsymbol)){
        std::cout<<"Type is "<<Tsymbol.name<<std::endl;
    }

}

VOID get_func_args_start(CONTEXT* ctx){
    std::cout<<"Started get function args probed function"<<std::endl;
    stapin::notify_thread_start(ctx, 0);
    if(!set_context(ctx,  stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbolIterator_1 =  stapin::find_symbol_by_name(FuncOI.c_str()); //this ia function name!
    if(nullptr == symbolIterator_1 ){
        std::cout<<"Could not find the function name"<<std::endl;
        return;
    }
    
    stapin::Symbol symbol; 
    if (!stapin::get_next_symbol(symbolIterator_1, &symbol)){
        return;
    } //get the symbol itself of the function  
    print_symbol_info(symbol);
    
    auto symbolIterator_2 =  stapin::get_symbols(&symbol); //children of the funciton -> its arguments! 
    if(nullptr == symbolIterator_2 ){
        stapin::close_symbol_iterator(symbolIterator_1);
        std::cout<<"Could not find arguments!"<<std::endl;
        return;
    }
    
    while (stapin::get_next_symbol(symbolIterator_2, &symbol)){
        print_symbol_info(symbol); //argument!
    } 
    stapin::close_symbol_iterator(symbolIterator_1);
    stapin::close_symbol_iterator(symbolIterator_2);
}



VOID STAPIN_Get_Function_ARGS_Start(IMG img,  VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); 
    if (RTN_Valid(myFunc)) // found
    {
        if (RTN_IsSafeForProbedInsertion(myFunc)) {
            PROTO proto_main_after = PROTO_Allocate(PIN_PARG(void*), CALLINGSTD_DEFAULT, MAIN, PIN_PARG_END()); //To Allow "IPOINT_AFTER" if needed.
            RTN_InsertCallProbed(myFunc, IPOINT_AFTER, (AFUNPTR)get_func_args_start, IARG_PROTOTYPE, proto_main_after,
                                 IARG_CONTEXT, IARG_END); 
        } else {
            std::cerr << "Function " << MAIN << " is not safe for probed insertion." << std::endl;
        }
    }
}


VOID Unload_Image(IMG img, VOID *v){
    stapin::notify_image_unload(img); //USE OF NOTIFY IMAGE UNLOAD
}


VOID Load_Image(IMG img, VOID *v){
    stapin::notify_image_load(img);  //USE OF NOTIFY IMAGE LOAD  
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
    std::cerr << "This tool is a try to connect Pintool and plugin that uses TCF" << std::endl;
    std::cerr << std::endl
         << KNOB_BASE::StringKnobSummary() << std::endl;
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
    IMG_AddInstrumentFunction(STAPIN_Get_Function_ARGS_Start, 0); 
    // call FINI when done.
    PIN_AddFiniFunction(Fini, 0);
    FuncOI = KnobFunc.Value();
    
    // Never returns
    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
