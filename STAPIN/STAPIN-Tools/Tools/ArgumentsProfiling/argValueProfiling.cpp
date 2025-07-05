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
#include <mutex>
#include <vector>
#include <string>

/*
Explanation:
This tool finds the values of all arguments of a target function at each instruction of the function.
Finally, it saves the results in CSV file.
*/
using std::cerr;
using std::endl;
using std::hex;
using std::ios;
using std::string;
using namespace std;

KNOB<string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Function of interest");
static std::vector<std::string> argNames;
static std::vector<std::tuple<string, string, string, string>> argValues; // To store argument values
static std::mutex argNamesMutex;
static std::mutex argValuesMutex;

string FuncOI="";

VOID start_value_profiling(CONTEXT* ctx, UINT32 insIndex){
    // Set the context for the current thread
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }

    // Iterate over each argument name
    for(auto& name: argNames)
    {
        // Evaluate the expression for the argument name
        auto epxressionIterator = stapin::evaluate_expression(name);
        if(nullptr == epxressionIterator)
        {
            return;
        }

        stapin::Expression expr {0}; 
        // Iterate over each expression result
        while(stapin::get_next_expression(epxressionIterator, &expr)){
            string valueStr;
            string typeStr;

            // Determine the type of the expression and convert it to a string
            switch (expr.typeFlag)
            {
                case stapin::E_expression_type::UNSIGNED:
                {
                    auto val = reinterpret_cast<const unsigned*>(expr.data);
                    valueStr = std::to_string(*val);
                    typeStr = "UNSIGNED";
                    break;
                }
                case stapin::E_expression_type::DOUBLE:
                {
                    auto val = reinterpret_cast<const double*>(expr.data);
                    valueStr = std::to_string(*val);
                    typeStr = "DOUBLE";
                    break;
                }
                case stapin::E_expression_type::ENUM:
                case stapin::E_expression_type::SIGNED:
                {
                    auto val = reinterpret_cast<const signed*>(expr.data);
                    valueStr = std::to_string(*val);
                    typeStr = "SIGNED";
                    break;
                }
                default:
                    break;
            }

            // Guard the access to argValues using a lock_guard
            {
                std::lock_guard<std::mutex> lock(argValuesMutex);
                argValues.push_back(std::make_tuple(name, typeStr, valueStr, std::to_string(insIndex)));
            }
        }
        stapin::close_expression_iterator(epxressionIterator);
    }
}

VOID find_arg_names(CONTEXT* ctx)
{
    // Set the context for the current thread
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }

    // Find the symbol for the function
    auto funcSymbolIterator = stapin::find_symbol_by_name(FuncOI.c_str());
    if(nullptr == funcSymbolIterator)
    {
        return;
    }

    stapin::Symbol symbol {0}; 
    int counter = -1;

    // Get the function symbol and count the number of arguments
    if(stapin::get_next_symbol(funcSymbolIterator, &symbol)) 
    {
        auto argsSymbolIterator = stapin::get_symbols(&symbol);
        if(nullptr != argsSymbolIterator)
        {
            stapin::Symbol argSymbol {0}; 
            while(stapin::get_next_symbol(argsSymbolIterator, &argSymbol))
            {
                if(-1 == counter)
                {
                    // First result is the return type of the function
                    ++counter;
                    continue;
                }                
                ++counter;
            }
        }
        stapin::close_symbol_iterator(argsSymbolIterator);
    }

    stapin::close_symbol_iterator(funcSymbolIterator);

    // Get the argument symbols and store their names
    auto argsSymbolIterator = stapin::get_symbols();
    if(nullptr != argsSymbolIterator)
    {
        stapin::Symbol argSym {0}; 
        while(stapin::get_next_symbol(argsSymbolIterator, &argSym) && counter > 0)
        {
            // Guard the access to argNames using a lock_guard
            {
                std::lock_guard<std::mutex> lock(argNamesMutex);
                argNames.push_back(argSym.name);
            }
            --counter;
        }
    }

    stapin::close_symbol_iterator(argsSymbolIterator);
}


VOID STAPIN_start_value_profiling(IMG img,  VOID *v){
    static bool firstInRtn = true;
    RTN myFunc = RTN_FindByName(img, FuncOI.c_str()); 
    if (RTN_Valid(myFunc)) // found
    {
        RTN_Open(myFunc);
        if(firstInRtn)
        {
            RTN_InsertCall(myFunc, IPOINT_BEFORE, (AFUNPTR)find_arg_names, IARG_CONTEXT,  IARG_END);
            firstInRtn = false;
        }
        // For each instruction of the routine
        UINT32 insIndex = 1;
        for (INS ins = RTN_InsHead(myFunc); INS_Valid(ins); ins = INS_Next(ins))
        {
            if(INS_IsValidForIpointAfter(ins))
            {
                // Insert a call to start_value_profiling to collect argument values
                INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)start_value_profiling, IARG_CONTEXT, IARG_UINT32, insIndex, IARG_END);
            }
            insIndex++;
            if(insIndex==10) break;
        }
#if 0
        INS ins = RTN_InsHead(myFunc); 
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)start_value_profiling, IARG_CONTEXT, IARG_UINT32, 0, IARG_END);
#endif
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
    // Create a CSV file
    std::ofstream csvFile("arg_values.csv");

    // Write the headers
    csvFile << "Argument Name,Type,Value,Instruction Index\n";

    // Write the collected argument values
    for (const auto& arg : argValues) {
        csvFile << std::get<0>(arg) << "," << std::get<1>(arg) << "," << std::get<2>(arg) << "," << std::get<3>(arg) << "\n";
    }

    // Close the CSV file
    csvFile.close();
}

// /* ===================================================================== */
// /* Print Help Message                                                    */
// /* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool profiles arguments values" << endl;
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
    IMG_AddInstrumentFunction(STAPIN_start_value_profiling, 0); 
    IMG_AddInstrumentFunction(Load_Image, 0);
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
