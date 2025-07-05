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

void print_location_info(stapin::Source_loc* curSourceLoc){
    std::cout<<"FILE NAME: "<<curSourceLoc->srcFileName<<std::endl;
    std::cout<<"START COL: "<<curSourceLoc->startColumn<<std::endl;
    std::cout<<"END COL: "<<curSourceLoc->endColumn<<std::endl;
    std::cout<<"START LINE: "<<curSourceLoc->startLine<<std::endl;
    std::cout<<"END LINE: "<<curSourceLoc->endLine<<std::endl;
    std::cout<<"START  ADDRESS: "<<curSourceLoc->startAddress<<std::endl;
    std::cout<<"END ADDRESS: "<<curSourceLoc->endAddress<<std::endl;
    std::cout<<"NEXT ADDRESS: "<<curSourceLoc->nextAddress<<std::endl;
    std::cout<<"NEXT staement address: "<<curSourceLoc->nextStmtAddress<<std::endl;
    std::cout<<"***********************************************"<<std::endl;
}

VOID get_source_locations_wrapper(ADDRINT startAddress, ADDRINT sizeRange){
    size_t locCount = 5;
    auto locations = new stapin::Source_loc[locCount];
    //USE OF GET SOURCE LOCATION FUNCTION 
    auto res = stapin::get_source_locations(startAddress, startAddress+sizeRange, locations,locCount );
    std::cout<<"*******RESULTS OF GET SOURCE LOCATION ARE*****"<<std::endl;
    for (size_t i = 0; i < res; ++i)
    {
        auto curSourceLoc = &locations[i];
        print_location_info(curSourceLoc);
    }
    delete[] locations;
}


VOID getSourceLocation_UseSTAPIN(IMG img, VOID *v){
    RTN myFunc = RTN_FindByName(img, MAIN); //if one wants to know symbols about this function must send its context before!!
     if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)get_source_locations_wrapper, IARG_INST_PTR, IARG_UINT32, 1,  IARG_END);
        RTN_Close(myFunc);
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
    
    if(!stapin::init()){ //USE OF INIT STAPIN FUNCTION 
        return 0;
    }
    
    IMG_AddInstrumentFunction(Load_Image, 0);
    IMG_AddUnloadFunction(Unload_Image, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, After_Fork_In_Child, 0);

    IMG_AddInstrumentFunction(getSourceLocation_UseSTAPIN, 0);
    
    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */