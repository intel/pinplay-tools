/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <unordered_map>
#include <stapin.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include "LineNumberTracker.H"
//#include "instlib.H"
//#include "control_manager.H"
#include "regvalue_utils.h"


using std::cerr;
using std::endl;
using std::hex;
using std::ios;
using std::string;


KNOB<string> KnobImage(KNOB_MODE_WRITEONCE, "pintool", "image", "", "Image of interest");
KNOB<string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Function of interest");
KNOB<string> KnobOutFile(KNOB_MODE_WRITEONCE, "pintool", "outfile", "IST.out.txt", "Output report in this file.");
KNOB<string> KnobMsgFile(KNOB_MODE_WRITEONCE, "pintool", "msgfile", "IST.msg.txt", "Output messages in this file.");
KNOB<bool> KnobShowMemOp(KNOB_MODE_WRITEONCE, "pintool", "showmemop", "1", "ouptput binary read/write messages");
KNOB<bool> KnobShowRegVal(KNOB_MODE_WRITEONCE, "pintool", "showregval", "0", "ouptput binary read/write messages");
std::string ImageOI;
std::string FuncOI;
std::ofstream outFile;
std::ofstream msgFile;

std::map<UINT32, LNT::LINEINFO *> line_map;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB< THREADID > KnobWatchThread(KNOB_MODE_WRITEONCE, "pintool", "watch_thread", "-1", "thread to watch, -1 for all");
KNOB< BOOL > KnobFlush(KNOB_MODE_WRITEONCE, "pintool", "flush", "0", "Flush output after every instruction");
KNOB< BOOL > KnobSymbols(KNOB_MODE_WRITEONCE, "pintool", "symbols", "0", "Include symbol information");
KNOB< BOOL > KnobLines(KNOB_MODE_WRITEONCE, "pintool", "lines", "0", "Include line number information");
KNOB< BOOL > KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "instruction", "0", "Trace instructions");
KNOB< BOOL > KnobTraceCalls(KNOB_MODE_WRITEONCE, "pintool", "call", "1", "Trace calls");
KNOB< BOOL > KnobTraceMemory(KNOB_MODE_WRITEONCE, "pintool", "memory", "0", "Trace memory");
KNOB< BOOL > KnobSilent(KNOB_MODE_WRITEONCE, "pintool", "silent", "0", "Do everything but write file (for debugging).");
KNOB< BOOL > KnobEarlyOut(KNOB_MODE_WRITEONCE, "pintool", "early_out", "0", "Exit after tracing the first region.");

/* ===================================================================== */
/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

//static std::ofstream out;

static INT32 enabled = 1;
static UINT32 Flineno = 0;
static string  Ffilename = "XYZ";
static size_t LastSymtabHash = -1;


int HashSymtab(stapin::Symbol_iterator* s)
{
   stapin::Symbol sym;
   int hash;
   string slist;
   while(stapin::get_next_symbol(s, &sym)){
    slist += sym.uniqueId;
    cerr << " symbol " << sym.name;
   }
    cerr << " slist " << slist << endl;
   hash= std::hash<std::string>{}(slist);
   return hash;
}


// Modifies global Flineno and Ffilename
VOID SourceForAddress(ADDRINT address, USIZE size)
{
  size_t locCount = 1;
  stapin::Source_loc  location; 
  auto res = stapin::get_source_locations(address, address+size, &location, locCount );
  ASSERTX(res);
  auto curSourceLoc = &location;
  Flineno = curSourceLoc->startLine;
  Ffilename = curSourceLoc->srcFileName;
}


VOID MonitorScope(CONTEXT* ctx, ADDRINT insaddr, USIZE inssize)
{
    SourceForAddress(insaddr, inssize);
    if(!stapin::set_context(ctx, stapin::E_context_update_method::FULL)){
        return;
    }
    auto symbol_iterator = stapin::get_symbols();
    if(nullptr == symbol_iterator){
        return;
    }
    size_t symtab_hash = HashSymtab(symbol_iterator);
     if(LastSymtabHash  != symtab_hash) outFile << "#***(stapin)* SCOPE Changed " << " AROUND " << Ffilename << ":" << std::dec << Flineno << std::endl;
    LastSymtabHash  = symtab_hash;
    stapin::close_symbol_iterator(symbol_iterator);
}
 
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


void print_symtabl_names_ids(stapin::Symbol_iterator* symbol_iterator ){
      if(nullptr == symbol_iterator){
        return;
      }
   stapin::Symbol symbol;
    while(stapin::get_next_symbol(symbol_iterator, &symbol)){
        msgFile <<"SYMBOL NAME: "<<symbol.name<<std::endl;
        msgFile <<"SYMBOL unique id: "<<symbol.uniqueId<<std::endl;
    }
     stapin::close_symbol_iterator(symbol_iterator);
}

static BOOL Emit(THREADID threadid)
{
    if (!enabled || KnobSilent || (KnobWatchThread != static_cast< THREADID >(-1) && KnobWatchThread != threadid)) return false;
    return true;
}

static VOID Flush()
{
    if (KnobFlush) outFile << std::flush;
}


/* ===================================================================== */

VOID EmitNoValues(THREADID threadid, CONTEXT* ctx, string* str)
{
    if (!Emit(threadid)) return;

    outFile << "  " << *str << endl;

    Flush();
}

VOID Emit1Values(THREADID threadid, CONTEXT* ctx, string* str, string* reg1str, ADDRINT reg1val, REG reg1)
{
    if (!Emit(threadid)) return;

  if(KnobShowRegVal)
   {
    outFile << "  " << *str << " | " << *reg1str << " = " << reg1val << std::endl;
    stapin::Symbol symbol; 
    if (stapin::get_symbol_by_reg(reg1, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg1) << " Symbol " << symbol.name << std::endl;
    }
  }
  else
    outFile << "  " << *str << std::endl;

  Flush();
}

VOID Emit2Values(THREADID threadid, CONTEXT* ctx, string* str, string* reg1str, ADDRINT reg1val, REG reg1, string* reg2str, ADDRINT reg2val, REG reg2)
{
    if (!Emit(threadid)) return;

  if(KnobShowRegVal)
  {
    outFile << "  " << *str << " | " << *reg1str << " = " << reg1val << ", " << *reg2str << " = " << reg2val << std::endl;

    stapin::Symbol symbol; 
    if (stapin::get_symbol_by_reg(reg1, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg1) << " Symbol " << symbol.name << std::endl;
    }
    if (stapin::get_symbol_by_reg(reg2, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg2) << " Symbol " << symbol.name << std::endl;
    }
  }
  else
    outFile << "  " << *str << std::endl;

  Flush();
}

VOID Emit3Values(THREADID threadid, CONTEXT* ctx, string* str, string* reg1str, ADDRINT reg1val, REG reg1, string* reg2str, ADDRINT reg2val, REG reg2,
                 string* reg3str, ADDRINT reg3val, REG reg3)
{
    if (!Emit(threadid)) return;

  if(KnobShowRegVal)
  {
    outFile << "  " << *str << " | " << *reg1str << " = " << reg1val << ", " << *reg2str << " = " << reg2val << ", " << *reg3str << " = " << reg3val << std::endl;
    stapin::Symbol symbol; 
    if (stapin::get_symbol_by_reg(reg1, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg1) << " Symbol " << symbol.name << std::endl;
    }
    if (stapin::get_symbol_by_reg(reg2, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg2) << " Symbol " << symbol.name << std::endl;
    }
    if (stapin::get_symbol_by_reg(reg3, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg3) << " Symbol " << symbol.name << std::endl;
    }
  }
  else
    outFile << "  " << *str << std::endl;

  Flush();
}

VOID Emit4Values(THREADID threadid, CONTEXT* ctx, string* str, string* reg1str, ADDRINT reg1val, REG reg1, string* reg2str, ADDRINT reg2val, REG reg2,
                 string* reg3str, ADDRINT reg3val, REG reg3, string* reg4str, ADDRINT reg4val, REG reg4)
{
    if (!Emit(threadid)) return;

  if(KnobShowRegVal)
  {
    outFile << " " << *str << " | " << *reg1str << " = " << reg1val << ", " << *reg2str << " = " << reg2val << ", " << *reg3str << " = "
        << reg3val << ", " << *reg4str << " = " << reg4val << std::endl;
    stapin::Symbol symbol; 
    if (stapin::get_symbol_by_reg(reg1, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg1) << " Symbol " << symbol.name << std::endl;
    }
    if (stapin::get_symbol_by_reg(reg2, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg2) << " Symbol " << symbol.name << std::endl;
    }
    if (stapin::get_symbol_by_reg(reg3, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg3) << " Symbol " << symbol.name << std::endl;
    }
    if (stapin::get_symbol_by_reg(reg4, &symbol))
    {
      outFile << "# REG " << REG_StringShort(reg4) << " Symbol " << symbol.name << std::endl;
    }
  }
  else
    outFile << "  " << *str << std::endl;

  Flush();
}

const UINT32 MaxEmitArgs = 4;

AFUNPTR emitFuns[] = {AFUNPTR(EmitNoValues), AFUNPTR(Emit1Values), AFUNPTR(Emit2Values), AFUNPTR(Emit3Values),
                      AFUNPTR(Emit4Values)};

/* ===================================================================== */

VOID EmitXMM(THREADID threadid, UINT32 regno, PINTOOL_REGISTER* xmm)
{
    if (!Emit(threadid)) return;
#if 0
    outFile << "\t\t\tXMM" << std::dec << regno << " := " << std::setfill('0') << hex;
    outFile.unsetf(ios::showbase);
    for (int i = 0; i < 16; i++)
    {
        if (i == 4 || i == 8 || i == 12) outFile << "_";
        outFile << std::setw(2) << (int)xmm->byte[15 - i]; // msb on the left as in registers
    }
    outFile << std::setfill(' ') << std::endl;
    outFile.setf(ios::showbase);
#endif
    Flush();
}   

VOID AddXMMEmit(INS ins, IPOINT point, REG xmm_dst)
{
    INS_InsertCall(ins, point, AFUNPTR(EmitXMM), IARG_THREAD_ID, IARG_UINT32, xmm_dst - REG_XMM0, IARG_REG_CONST_REFERENCE,
                   xmm_dst, IARG_END);
}

VOID AddEmit(INS ins, IPOINT point, string& traceString, UINT32 regCount, REG regs[])
{
    if (regCount > MaxEmitArgs) regCount = MaxEmitArgs;

    IARGLIST args = IARGLIST_Alloc();
    if(KnobShowRegVal)
    {
      for (UINT32 i = 0; i < regCount; i++)
      {
        IARGLIST_AddArguments(args, IARG_PTR, new string(REG_StringShort(regs[i])), IARG_REG_VALUE, regs[i], IARG_UINT32, regs[i], IARG_END);
      }
    }
    INS_InsertCall(ins, point, emitFuns[regCount], IARG_THREAD_ID, IARG_CONTEXT,  IARG_PTR, new string(traceString), IARG_IARGLIST, args,
                   IARG_END);
    IARGLIST_Free(args);
}

string FormatAddress(ADDRINT address, RTN rtn)
{
    //string s = StringFromAddrint(address);
    std::ostringstream oss;
    oss << "0x" << std::hex << (uintptr_t)address;
    string s = oss.str();

    if (KnobSymbols && RTN_Valid(rtn))
    {
        IMG img = SEC_Img(RTN_Sec(rtn));
        s += " ";
        if (IMG_Valid(img))
        {
            s += IMG_Name(img) + ":";
        }

        s += RTN_Name(rtn);

        ADDRINT delta = address - RTN_Address(rtn);
        if (delta != 0)
        {
            s += "+" + hexstr(delta, 4);
        }
    }

    if (KnobLines)
    {
        INT32 line;
        string file;

        PIN_GetSourceLocation(address, NULL, &line, &file);

        if (file != "")
        {
            s += " (" + file + ":" + decstr(line) + ")";
        }
    }
    return s;
}

static VOID* WriteEa[PIN_MAX_THREADS];

VOID CaptureWriteEa(THREADID threadid, VOID* addr) { WriteEa[threadid] = addr; }

VOID ShowN(UINT32 n, VOID* ea)
{
    outFile.unsetf(ios::showbase);
    // Print out the bytes in "big endian even though they are in memory little endian.
    // This is most natural for 8B and 16B quantities that show up most frequently.
    // The address pointed to
    outFile << std::setfill('0');
    UINT8 b[512];
    UINT8* x;
    if (n > 512)
        x = new UINT8[n];
    else
        x = b;
    PIN_SafeCopy(x, static_cast< UINT8* >(ea), n);
    for (UINT32 i = 0; i < n; i++)
    {
        outFile << std::setw(2) << static_cast< UINT32 >(x[n - i - 1]);
        if (((reinterpret_cast< ADDRINT >(ea) + n - i - 1) & 0x3) == 0 && i < n - 1) outFile << "_";
    }
    outFile << std::setfill(' ');
    outFile.setf(ios::showbase);
    if (n > 512) delete[] x;
}


VOID EmitRead(THREADID threadid, VOID* ea, UINT32 size)
{
    if (!Emit(threadid)) return;

 if( KnobShowMemOp )
 {
    outFile << "    Read ";

    switch (size)
    {
        case 0:
            outFile << "0 repeat count" << endl;
            break;

        case 1:
        {
            UINT8 x;
            PIN_SafeCopy(&x, static_cast< UINT8* >(ea), 1);
            outFile << static_cast< UINT32 >(x) << " = *(UINT8*)" << ea << endl;
        }
        break;

        case 2:
        {
            UINT16 x;
            PIN_SafeCopy(&x, static_cast< UINT16* >(ea), 2);
            outFile << x << " = *(UINT16*)" << ea << endl;
        }
        break;

        case 4:
        {
            UINT32 x;
            PIN_SafeCopy(&x, static_cast< UINT32* >(ea), 4);
            outFile << x << " = *(UINT32*)" << ea << endl;
        }
        break;

        case 8:
        {
            UINT64 x;
            PIN_SafeCopy(&x, static_cast< UINT64* >(ea), 8);
            outFile << x << " = *(UINT64*)" << ea << endl;
        }
        break;

        default:
            ShowN(size, ea);
            outFile << " = *(UINT" << std::dec << size * 8 << hex << ")" << ea << endl;
            break;
    }
 }
    stapin::Symbol symbol; 
    if (stapin::get_symbol_by_address((ADDRINT)ea, &symbol))
    {
       stapin::Symbol tsymbol;
      if(stapin::get_symbol_by_id(symbol.typeUniqueId, &tsymbol))
      {
        outFile << "#***(stapin)* READ Symbol " << symbol.name << ":'" << tsymbol.name << "' " << " @ " << Ffilename << ":" << std::dec << Flineno << std::endl;
      }
      else
      {
        outFile << "#***(stapin)* READ Symbol " << symbol.name << " @ " << Ffilename << ":" << std::dec << Flineno << std::endl;
      }
    }

    Flush();
}

VOID EmitWrite(THREADID threadid, UINT32 size)
{
    if (!Emit(threadid)) return;

    VOID* ea = WriteEa[threadid];
 if( KnobShowMemOp )
 {
    outFile << "    Write ";


    switch (size)
    {
        case 0:
            outFile << "0 repeat count" << endl;
            break;

        case 1:
        {
            UINT8 x;
            PIN_SafeCopy(&x, static_cast< UINT8* >(ea), 1);
            outFile << "*(UINT8*)" << ea << " = " << static_cast< UINT32 >(x) << endl;
        }
        break;

        case 2:
        {
            UINT16 x;
            PIN_SafeCopy(&x, static_cast< UINT16* >(ea), 2);
            outFile << "*(UINT16*)" << ea << " = " << x << endl;
        }
        break;

        case 4:
        {
            UINT32 x;
            PIN_SafeCopy(&x, static_cast< UINT32* >(ea), 4);
            outFile << "*(UINT32*)" << ea << " = " << x << endl;
        }
        break;

        case 8:
        {
            UINT64 x;
            PIN_SafeCopy(&x, static_cast< UINT64* >(ea), 8);
            outFile << "*(UINT64*)" << ea << " = " << x << endl;
        }
        break;

        default:
            outFile << "*(UINT" << std::dec << size * 8 << hex << ")" << ea << " = ";
            ShowN(size, ea);
            outFile << endl;
            break;
    }
 }
    stapin::Symbol symbol; 
    if (stapin::get_symbol_by_address((ADDRINT)ea, &symbol))
    {
       stapin::Symbol tsymbol;
      if(stapin::get_symbol_by_id(symbol.typeUniqueId, &tsymbol))
      {
        outFile << "#***(stapin)* WROTE Symbol " << symbol.name << ":'" << tsymbol.name << "' " << " @ " << Ffilename << ":" << std::dec << Flineno << std::endl;
      }
      else
      {
        outFile << "#***(stapin)* WROTE Symbol " << symbol.name << " @ " << Ffilename << ":" << std::dec << Flineno << std::endl;
      }
    }

    Flush();
}

VOID MemoryTrace(INS ins)
{

    if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CaptureWriteEa), IARG_THREAD_ID, IARG_MEMORYWRITE_EA, IARG_END);

        if (INS_IsValidForIpointAfter(ins))
        {
            INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(EmitWrite), IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
        }
        if (INS_IsValidForIpointTakenBranch(ins))
        {
            INS_InsertPredicatedCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(EmitWrite), IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE,
                                     IARG_END);
        }
    }

    if (INS_HasMemoryRead2(ins) && INS_IsStandardMemop(ins))
    {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(EmitRead), IARG_THREAD_ID, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
                                 IARG_END);
    }

    if (INS_IsMemoryRead(ins) && !INS_IsPrefetch(ins) && INS_IsStandardMemop(ins))
    {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(EmitRead), IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                                 IARG_END);
    }
}


VOID TraceScope(INS ins)
{
  INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(MonitorScope),  IARG_CONTEXT, IARG_ADDRINT, INS_Address(ins), IARG_UINT32, INS_Size(ins), IARG_END);
#if 0
  if (INS_IsValidForIpointAfter(ins))
    {
      INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(MonitorScope), IARG_END);
    }
  if (INS_IsValidForIpointTakenBranch(ins))
    {
      INS_InsertPredicatedCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(MonitorScope), IARG_END);
    }
#endif
}

VOID BeforeCall(THREADID threadid)
{
    if (!Emit(threadid)) return;
}

VOID CallTrace(INS ins)
{
    if (INS_IsCall(ins))
    {
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(BeforeCall), IARG_THREAD_ID, IARG_END);
    }
}

VOID InstructionTrace(TRACE trace, INS ins)
{
    //if (!KnobTraceInstructions) return;

    ADDRINT addr = INS_Address(ins);
    ASSERTX(addr);

    // Format the string at instrumentation time
    string traceString = "";
    string astring     = FormatAddress(INS_Address(ins), TRACE_Rtn(trace));
    for (INT32 length = astring.length(); length < 10; length++)
    {
        traceString += " ";
    }
    traceString = astring + traceString;

    traceString += " " + INS_Disassemble(ins);

    if(KnobShowRegVal)
    {
      for (INT32 length = traceString.length(); length < 80; length++)
      {
        traceString += " ";
      }
    }
    INT32 regCount = 0;
    REG regs[20];
    REG xmm_dst = REG_INVALID();

    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++)
    {
        REG x = REG_FullRegName(INS_RegW(ins, i));

        if (REG_is_gr(x)
#if defined(TARGET_IA32)
            || x == REG_EFLAGS
#elif defined(TARGET_IA32E)
            || x == REG_RFLAGS
#endif
        )
        {
            regs[regCount] = x;
            regCount++;
        }

        if (REG_is_xmm(x)) xmm_dst = x;
    }

    if (INS_IsValidForIpointAfter(ins))
    {
        AddEmit(ins, IPOINT_AFTER, traceString, regCount, regs);
    }
    if (INS_IsValidForIpointTakenBranch(ins))
    {
        AddEmit(ins, IPOINT_TAKEN_BRANCH, traceString, regCount, regs);
    }

    if (xmm_dst != REG_INVALID())
    {
        if (INS_IsValidForIpointAfter(ins)) AddXMMEmit(ins, IPOINT_AFTER, xmm_dst);
        if (INS_IsValidForIpointTakenBranch(ins)) AddXMMEmit(ins, IPOINT_TAKEN_BRANCH, xmm_dst);
    }
    MemoryTrace(ins);
    TraceScope(ins);
    //CallTrace(ins);
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
    delete[] locations;
}

VOID INSTrace(TRACE trace, VOID *v){
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
        for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
        {
         for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
         {
          if (Flineno==0)
          {
            SourceForAddress(INS_Address(ins), INS_Size(ins));
          }
          InstructionTrace(trace, ins);
          //ProcessIns(ins);
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

// Should we process this RTN?
BOOL ProcessRoutine(RTN rtn) 
{   
  string rtnname = RTN_Name(rtn);
  if (rtnname == FuncOI)
  {
    return true;
  } 
  return false;
}

static VOID AfterFuncOI()
{
  enabled = 0;
}

static VOID BeforeFuncOI(CONTEXT* ctx, VOID * v1)
{
  static bool done=false;
  if(!done)
  {  
     outFile << "#***(stapin)* Entering: "  << (char *) v1 << "():" << Ffilename << ":" << std::dec << Flineno << std::endl;
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
    done = true;
    stapin::close_symbol_iterator(symbol_iterator);
    symbol_iterator = stapin::get_symbols();
    size_t symtab_hash = HashSymtab(symbol_iterator);
    LastSymtabHash  = symtab_hash;
    stapin::close_symbol_iterator(symbol_iterator);
  } 
}


static VOID IMAGE_LOAD(IMG img, VOID* v)
{
      for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
      {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
           if(ProcessRoutine(rtn))
           {
              RTN_Open(rtn);
              RTN_InsertCall(rtn, IPOINT_BEFORE, 
              (AFUNPTR)BeforeFuncOI, 
              IARG_CONTEXT,
              IARG_PTR, RTN_Name(rtn).c_str(),
                IARG_END);
             RTN_InsertCall(rtn, IPOINT_AFTER, 
              (AFUNPTR)AfterFuncOI, 
                IARG_END);
            RTN_Close(rtn);
          }
        } // end for RTN
         //if (found) break; // out of SEC loop
      } // end for SEC
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
              msgFile << " INS add 0x" << std::hex << ins_addr << " RARE  Interference LineNumber: " << std::dec << outer_it->first << " and " << std::dec << inner_it->first << std::endl;
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
    IMG_AddInstrumentFunction(IMAGE_LOAD, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    TRACE_AddInstrumentFunction(INSTrace, 0);
    
    PIN_AddFiniFunction(Fini, 0);
    
    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
