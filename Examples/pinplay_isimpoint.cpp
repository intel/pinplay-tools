// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/* ===================================================================== */
/*! @file
 *  This file contains an ISA-portable PIN tool for counting dynamic instructions
 */

#include <map>
#include <iostream>
#include <fstream>
#include <string.h>

#include "pin.H"
#include "instlib.H"
#include "pinplay.H"
#  include "sde-init.H"
#  include "sde-control.H"
#include "sde-pinplay-supp.H"

using namespace std;
#include "time_warp.H"

#define MAX_THREADS 20
#define MAX_IMAGES 250

using namespace INSTLIB;

class MYIMG_INFO
{
    public:
        MYIMG_INFO(IMG img);
        INT32 Id() { return _imgId;}
        CHAR * Name() { return _name;}
        ADDRINT  LowAddress() { return _low_address;}
        static INT32 _currentImgId;
        ~MYIMG_INFO()
        {
          if(_name) free(_name);
        }
    private:
        CHAR * _name; 
        ADDRINT _low_address; 
        // static members
        INT32 _imgId;
};

MYIMG_INFO * img_info[MAX_IMAGES]; 

MYIMG_INFO::MYIMG_INFO(IMG img)
    : _imgId(_currentImgId++)
{
    _name = (CHAR *) calloc(strlen(IMG_Name(img).c_str())+1, 1);
    ASSERTX(_name);
    strcpy(_name,IMG_Name(img).c_str());
    _low_address = IMG_LowAddress(img);
}


UINT32 FindImgInfoId(IMG img)
{
    if (!IMG_Valid(img))
        return 0;
    
    ADDRINT low_address = IMG_LowAddress(img);
    
    for (UINT32 i = MYIMG_INFO::_currentImgId-1; i >=1; i--)
    {
        if(img_info[i]->LowAddress() == low_address)
            return i;
    }
    // cerr << "FindImgInfoId(0x" << hex << low_address << ")" <<   endl;
    return 0;
}

class MYBLOCK_KEY
{
    friend BOOL operator<(const MYBLOCK_KEY & p1, const MYBLOCK_KEY & p2);
        
  public:
    MYBLOCK_KEY(ADDRINT s, ADDRINT e, USIZE z) : _start(s),_end(e),_size(z) {};
    BOOL IsPoint() const { return (_start - _end) == 1;  }
    ADDRINT Start() const { return _start; }
    ADDRINT End() const { return _end; }
    USIZE Size() const { return _size; }
    BOOL Contains(ADDRINT addr) const;
    
  private:
    const ADDRINT _start;
    const ADDRINT _end;
    const USIZE _size;
};

class MYBLOCK
{
  public:
    MYBLOCK(const MYBLOCK_KEY & key, INT32 instructionCount, IMG i);
    INT32 StaticInstructionCount() const { return _staticInstructionCount; }
    VOID Execute(THREADID tid) { _sliceBlockCount[tid]++; }
    VOID EmitSliceEnd(THREADID tid) ;
    VOID EmitProgramEnd(const MYBLOCK_KEY & key, THREADID tid) const;
    INT64 GlobalBlockCount(THREADID tid) const { return _globalBlockCount[tid] + _sliceBlockCount[tid]; }
    UINT32 ImgId() const { return _imgId; }
    const MYBLOCK_KEY & Key() const { return _key; }
    
    
  private:
    INT32 Id() const { return _id; }
    INT32 SliceInstructionCount(THREADID tid) const { return _sliceBlockCount[tid] * _staticInstructionCount; }

    // static members
    static INT32 _currentId;

    const INT32 _staticInstructionCount;
    const INT32 _id;
    const MYBLOCK_KEY _key;

    INT32 _sliceBlockCount[MAX_THREADS];
    INT64 _globalBlockCount[MAX_THREADS];
    UINT32 _imgId;
};


typedef pair<MYBLOCK_KEY, MYBLOCK*> MYBLOCK_PAIR;
typedef multimap<MYBLOCK_KEY, MYBLOCK*> MYBLOCK_MAP;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

static MYBLOCK_MAP block_map;
static string commandLine;

class PROFILE
{
    public: 
    PROFILE()
    {
        first = true;
        GlobalInstructionCount = 0;
        SliceTimer = 0;
    }
    ofstream BbFile;
    INT64 GlobalInstructionCount;
    // The first time, we want a marker, but no T vector
    BOOL first;
    // Emit the first marker immediately
    INT32 SliceTimer;
};

// LOCALVAR INT32 maxThread = 0;
static PROFILE ** profiles;
// LOCALVAR INT32 firstPid = 0;


INT32 MYBLOCK::_currentId = 1;
INT32 MYIMG_INFO::_currentImgId = 1;


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "out", "specify bb file name");
KNOB<INT32>  KnobSliceSize(KNOB_MODE_WRITEONCE,  "pintool",
    "slice_size", "100000000", "slice size in instructions");
KNOB<BOOL>  KnobNoSymbolic(KNOB_MODE_WRITEONCE,  "pintool",
    "nosymbolic", "0", "Do not emit symbolic information for markers");


/* ===================================================================== */

VOID MYBLOCK::EmitSliceEnd(THREADID tid)
{
    if (_sliceBlockCount[tid] == 0)
        return;
    
    profiles[tid]->BbFile << ":" << dec << Id() << ":" << dec << SliceInstructionCount(tid) << " ";
    _globalBlockCount[tid] += _sliceBlockCount[tid];
    _sliceBlockCount[tid] = 0;
}


BOOL operator<(const MYBLOCK_KEY & p1, const MYBLOCK_KEY & p2)
{
    if (p1.IsPoint())
        return p1._start < p2._start;

    if (p2.IsPoint())
        return p1._end <= p2._start;
    
    if (p1._start == p2._start)
        return p1._end < p2._end;
    
    return p1._start < p2._start;
}

BOOL MYBLOCK_KEY::Contains(ADDRINT address) const
{
    if (address >= _start && address <= _end)
        return true;
    else
        return false;
}

/* ===================================================================== */

static VOID EmitSliceEnd(ADDRINT endMarker, UINT32 imgId, THREADID tid)
{
    
    INT64 markerCount = 0;
    
    profiles[tid]->BbFile << "# Slice at " << dec << profiles[tid]->GlobalInstructionCount << endl;
    
    if (!profiles[tid]->first)
        profiles[tid]->BbFile << "T" ;
    else
    {
    // Input merging will change the name of the input
        profiles[tid]->BbFile << "I: 0" << endl;
        profiles[tid]->BbFile << "P: " << dec << tid << endl;
        profiles[tid]->BbFile << "C: sum:dummy Command:" << commandLine << endl;
    }

    for (MYBLOCK_MAP::iterator bi = block_map.begin(); bi != block_map.end(); bi++)
    {
        MYBLOCK * block = bi->second;
        const MYBLOCK_KEY & key = bi->first;

        if (key.Contains(endMarker))
        {
            markerCount += block->GlobalBlockCount(tid);
        }
        
        if (!profiles[tid]->first)
            block->EmitSliceEnd(tid);
    }

    if (!profiles[tid]->first)
        profiles[tid]->BbFile << endl;

    profiles[tid]->first = false;
    
    if (KnobNoSymbolic)
    {
        profiles[tid]->BbFile << "M: " << hex << endMarker << " " << dec << markerCount << endl;
    }
    else
    {
        if(!imgId)
        {
            profiles[tid]->BbFile << "S: " << hex << endMarker << " " << dec << markerCount << " " << "no_image" << " " << hex  << 0 << endl;
        }
        else
        {
            profiles[tid]->BbFile << "S: " << hex << endMarker << " " << dec << markerCount << " " << img_info[imgId]->Name() << " " << hex  <<img_info[imgId]->LowAddress() << " + " << hex << endMarker-img_info[imgId]->LowAddress() << endl;
        }
    }
}

int CountBlock_If(MYBLOCK * block, THREADID tid)
{
    block->Execute(tid);

    profiles[tid]->SliceTimer -= block->StaticInstructionCount();

    return(profiles[tid]->SliceTimer < 0);
}

VOID CountBlock_Then(MYBLOCK * block, THREADID tid)
{
    profiles[tid]->GlobalInstructionCount += (KnobSliceSize - profiles[tid]->SliceTimer);
    EmitSliceEnd(block->Key().End(), block->ImgId(), tid);
    profiles[tid]->SliceTimer = KnobSliceSize;
}

MYBLOCK::MYBLOCK(const MYBLOCK_KEY & key, INT32 instructionCount, IMG img)
    :
    _staticInstructionCount(instructionCount),
    _id(_currentId++),
    _key(key)
{
    _imgId = FindImgInfoId(img);
    for (THREADID tid = 0; tid < MAX_THREADS; tid++)
    {
        _sliceBlockCount[tid] = 0;
        _globalBlockCount[tid] = 0;
    }
}

static MYBLOCK * LookupBlock(BBL bbl)
{
    MYBLOCK_KEY key(INS_Address(BBL_InsHead(bbl)), INS_Address(BBL_InsTail(bbl)), BBL_Size(bbl));
    MYBLOCK_MAP::const_iterator bi = block_map.find(key);
    
    if (bi == block_map.end())
    {
        // Block not there, add it
        RTN rtn = INS_Rtn(BBL_InsHead(bbl));
        SEC sec = SEC_Invalid();
        IMG img = IMG_Invalid();
        if(RTN_Valid(rtn))
            sec = RTN_Sec(rtn);
        if(SEC_Valid(sec))
            img = SEC_Img(sec);
        MYBLOCK * block = new MYBLOCK(key, BBL_NumIns(bbl), img);
        block_map.insert(MYBLOCK_PAIR(key, block));

        return block;
    }
    else
    {
        return bi->second;
    }
}

/* ===================================================================== */

VOID Trace(TRACE trace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        MYBLOCK * block = LookupBlock(bbl);
        INS_InsertIfCall(BBL_InsTail(bbl), IPOINT_BEFORE, (AFUNPTR)CountBlock_If, IARG_PTR, block, IARG_THREAD_ID, IARG_END);
        INS_InsertThenCall(BBL_InsTail(bbl), IPOINT_BEFORE, (AFUNPTR)CountBlock_Then, IARG_PTR, block, IARG_THREAD_ID, IARG_END);
    }
}

/* ===================================================================== */

VOID Image(IMG img, VOID * v)
{
    ASSERTX(MYIMG_INFO::_currentImgId < (MAX_IMAGES - 1));
    img_info[MYIMG_INFO::_currentImgId] = new MYIMG_INFO(img); 
    profiles[0]->BbFile << "G: " << IMG_Name(img) << " LowAddress: " << hex  << IMG_LowAddress(img) << " LoadOffset: " << hex << IMG_LoadOffset(img) << endl;
}


VOID MYBLOCK::EmitProgramEnd(const MYBLOCK_KEY & key, THREADID tid) const
{
    if (_globalBlockCount[tid] == 0)
        return;
    
    profiles[tid]->BbFile << "Block id: " << dec << _id << " " << hex << key.Start() << ":" << key.End() << dec
           << " static instructions: " << _staticInstructionCount
           << " block count: " << _globalBlockCount[tid]
           << " block size: " << key.Size()
           << endl;
}

static VOID EmitProgramEnd(THREADID tid)
{
    profiles[tid]->BbFile << "Dynamic instruction count " << dec << profiles[tid]->GlobalInstructionCount << endl;
    profiles[tid]->BbFile << "SliceSize: " << dec << KnobSliceSize << endl;
    for (MYBLOCK_MAP::const_iterator bi = block_map.begin(); bi != block_map.end(); bi++)
    {
        bi->second->EmitProgramEnd(bi->first, tid);
    }
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
    for (THREADID tid = 0; tid < MAX_THREADS; tid++)
    {
        EmitProgramEnd(tid);
        profiles[tid]->BbFile << "End of bb" << endl;
        profiles[tid]->BbFile.close();
    }
}

VOID ThreadBegin(THREADID tid, CONTEXT * ctxt, int flags, VOID *v)
{
    char num[100];
    sprintf(num, ".T.%d.bb", tid);
    string tname = num;
    ASSERTX(tid < MAX_THREADS);
    profiles[tid]->BbFile.open((KnobOutputFile.Value()+tname).c_str());
    profiles[tid]->BbFile.setf(ios::showbase);
}

VOID ThreadEnd(UINT32 threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
}


VOID GetCommand(int argc, char *argv[])
{
    for (INT32 i = 0; i < argc; i++)
    {
            commandLine += " ";
            commandLine += argv[i];
    }
}
/* ===================================================================== */

int main(int argc, char *argv[])
{
    sde_pin_init(argc,argv);
    sde_init();
    PIN_InitSymbols();
    GetCommand(argc, argv);

    //maxThread = MaxThreadsKnob.ValueInt64();
    profiles = new PROFILE*[MAX_THREADS];
    memset(profiles, 0, MAX_THREADS * sizeof(profiles[0]));

    PIN_AddThreadStartFunction(ThreadBegin, 0);
    PIN_AddThreadFiniFunction(ThreadEnd, 0);


    for (THREADID tid = 0; tid < MAX_THREADS; tid++)
    {
        profiles[tid] = new PROFILE();
    }
    profiles[0]->BbFile.open((KnobOutputFile.Value()+".T.0.bb").c_str());
    profiles[0]->BbFile.setf(ios::showbase);

    TRACE_AddInstrumentFunction(Trace, 0);
    IMG_AddInstrumentFunction(Image, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
