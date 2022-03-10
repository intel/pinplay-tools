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
#include "global_isimpoint_inst.H"


VOID GLOBALBLOCK::ExecuteGlobal(THREADID tid, const GLOBALBLOCK* prev_block,
   GLOBALISIMPOINT *gisimpoint)
{
    ATOMIC::OPS::Increment<INT64>
                (& _sliceBlockCountGlobal._count, 1); 
    _sliceBlockCountThreads[tid]++;
    if (IdGlobal() == 0)
      ASSERT(0,"IdGlobal()==0 in ExecuteGlobal() NYT "); 

    // Keep track of previous blocks and their counts only if we 
    // will be outputting them later.
    if (gisimpoint->KnobEmitPrevBlockCounts) {

        // The block "previous to" the first block is denoted by
        // the special ID zero (0).
        // It should always have a count of one (1).
        UINT32 prevBlockId = prev_block ? prev_block->IdGlobal() : 0;

        // Automagically add hash keys for this tid and prevBlockID 
        // as needed and increment the counter.
        ATOMIC::OPS::Increment<INT64>
                (&(_blockCountMapGlobal[prevBlockId])._count, 1); 
        ATOMIC::OPS::Increment<INT64>
                (&(_blockCountMapThreads[tid][prevBlockId])._count, 1); 
    }
}

VOID GLOBALBLOCK::EmitSliceEndGlobal(GLOBALPROFILE *gprofile)
{
    if (_sliceBlockCountGlobal._count == 0)
        return;
    
    gprofile->BbFile << ":" << std::dec << IdGlobal() << ":" << std::dec 
        << SliceInstructionCountGlobal() << " ";
    ATOMIC::OPS::Increment<INT64>
                (&_cumulativeBlockCountGlobal._count, 
                _sliceBlockCountGlobal._count); 
    _sliceBlockCountGlobal._count = 0;
}

VOID GLOBALBLOCK::EmitSliceEndThread(THREADID tid, GLOBALPROFILE *profile)
{
    if (_sliceBlockCountThreads[tid] == 0)
        return;

    profile->BbFile << ":" << std::dec << IdGlobal() << ":" << std::dec
        << SliceInstructionCountThread(tid) << " ";
    _cumulativeBlockCountThreads[tid] += _sliceBlockCountThreads[tid];
    _sliceBlockCountThreads[tid] = 0;
}


VOID GLOBALBLOCK::EmitProgramEndGlobal(const BLOCK_KEY & key, 
    GLOBALPROFILE *gprofile, const GLOBALISIMPOINT *gisimpoint) const
{
    // If this block has the start address of the slice we need to emit it
    // even if it was not executed.
    BOOL force_emit = gisimpoint->FoundInStartSlices(key.Start());
    if (_cumulativeBlockCountGlobal._count == 0 && !force_emit)
        return;
    
    gprofile->BbFile << "Block id: " << std::dec << IdGlobal() << " " << std::hex 
        << key.Start() << ":" << key.End() << std::dec
        << " static instructions: " << StaticInstructionCount()
        << " block count: " << _cumulativeBlockCountGlobal._count
        << " block size: " << key.Size();

    // Output previous blocks and their counts only if enabled.
    // Example: previous-block counts: ( 3:1 5:13 7:3 )
    if (gisimpoint->KnobEmitPrevBlockCounts) {
        gprofile->BbFile << " previous-block counts: ( ";

        // output block-id:block-count pairs.
        for (BLOCK_COUNT_MAP_GLOBAL::const_iterator bci = 
              _blockCountMapGlobal.begin();
             bci != _blockCountMapGlobal.end();
             bci++) {
            gprofile->BbFile << bci->first << ':' << bci->second._count << ' ';
        }
        gprofile->BbFile << ')';
    }
    gprofile->BbFile << std::endl;
}

VOID GLOBALBLOCK::EmitProgramEndThread(const BLOCK_KEY & key, THREADID tid, 
    GLOBALPROFILE *gprofile, const GLOBALISIMPOINT *gisimpoint) const
{
    // If this block has the start address of the slice we need to emit it
    // even if it was not executed.
    BOOL force_emit = gisimpoint->FoundInStartSlices(key.Start());
    if (_cumulativeBlockCountThreads[tid] == 0 && !force_emit)
        return;
    
    gprofile->BbFile << "Block id: " << std::dec << IdGlobal() << " " << std::hex 
        << key.Start() << ":" << key.End() << std::dec
        << " static instructions: " << StaticInstructionCount()
        << " block count: " << _cumulativeBlockCountThreads[tid]
        << " block size: " << key.Size();

    // Output previous blocks and their counts only if enabled.
    // Example: previous-block counts: ( 3:1 5:13 7:3 )
    if (gisimpoint->KnobEmitPrevBlockCounts) {
        gprofile->BbFile << " previous-block counts: ( ";

        // output block-id:block-count pairs.
        for (BLOCK_COUNT_MAP_GLOBAL::const_iterator bci = _blockCountMapThreads[tid].begin();
             bci != _blockCountMapThreads[tid].end();
             bci++) {
            gprofile->BbFile << bci->first << ':' << bci->second._count << ' ';
        }
        gprofile->BbFile << ')';
    }
    gprofile->BbFile << std::endl;
}

// Static knobs
KNOB<BOOL> GLOBALISIMPOINT::KnobGlobal (KNOB_MODE_WRITEONCE,  
    "pintool:isimpoint",
    "global_profile", "0", "Create a global bbv file");
KNOB<INT32> GLOBALISIMPOINT::KnobThreadProgress(KNOB_MODE_WRITEONCE,  
    "pintool:isimpoint",
    "thread_progress", "0", "N: number of threads: end global slice whenever any thread reaches 1/N th slice-size.");
KNOB<UINT32> GLOBALISIMPOINT::KnobSpinStartSSC(KNOB_MODE_WRITEONCE,  
    "pintool:isimpoint",
    "spin_start_SSC", "0", "SSC marker (0x...) for the start of spin loop to be skipped ");
KNOB<UINT32> GLOBALISIMPOINT::KnobSpinEndSSC(KNOB_MODE_WRITEONCE,  
    "pintool:isimpoint",
    "spin_end_SSC", "0", "SSC marker (0x...) for the end of spin loop to be skipped ");
PIN_RWMUTEX GLOBALISIMPOINT::_StopTheWorldLock;
