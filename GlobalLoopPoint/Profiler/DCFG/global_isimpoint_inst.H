/*BEGIN_LEGAL 
BSD License 

Copyright (c)2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#ifndef GLOBAL_ISIMPOINT_INST_H
#define GLOBAL_ISIMPOINT_INST_H

#include "isimpoint_inst.H"
#include "atomic.hpp"
#include "filter.mod.H"

#define LOCALTYPE 
using namespace INSTLIB;

// a 64-bit(8 byte) counter with 56 bytes of padding so the entire struct fits
// in a 64-byte cache line on x86
struct GLOBAL_COUNTER64 {
    INT64 _count;
    UINT8 _pad[56];
};

// a 32-bit(4 byte) counter with 60 bytes of padding so the entire struct fits
// in a 64-byte cache line on x86
struct GLOBAL_COUNTER32 {
    INT32 _count;
    UINT8 _pad[60];
};

class GLOBALPROFILE;
class GLOBALISIMPOINT;
class GLOBALBLOCK;

LOCALTYPE typedef std::pair<BLOCK_KEY, GLOBALBLOCK *> GLOBALBLOCK_PAIR;
LOCALTYPE typedef std::map<INT32, GLOBAL_COUNTER64> BLOCK_COUNT_MAP_GLOBAL;
LOCALTYPE typedef std::map<BLOCK_KEY, GLOBALBLOCK*> GLOBALBLOCK_MAP;

class GLOBALBLOCK : public BLOCK
{
  public:
    VOID ExecuteGlobal(THREADID tid) 
      {ATOMIC::OPS::Increment<INT64>(&_sliceBlockCountGlobal._count, 1);
       _sliceBlockCountThreads[tid]++;
      }
    VOID ExecuteGlobal(THREADID tid, const GLOBALBLOCK* prev_block, 
          GLOBALISIMPOINT *gisimpoint);
    VOID EmitSliceEndGlobal(GLOBALPROFILE *gprofile);
    VOID EmitSliceEndThread(THREADID tid, GLOBALPROFILE *profile);
    VOID EmitProgramEndGlobal(const BLOCK_KEY & key, 
        GLOBALPROFILE * profile, const GLOBALISIMPOINT *isimpoint) const; 
    VOID EmitProgramEndThread(const BLOCK_KEY & key, THREADID tid,
        GLOBALPROFILE * profile, const GLOBALISIMPOINT *isimpoint) const;

    INT64 CumulativeBlockCountGlobal() const 
        { return _cumulativeBlockCountGlobal._count + _sliceBlockCountGlobal._count; }
    INT64 CumulativeBlockCountThread(THREADID tid) const
        { return _cumulativeBlockCountThreads[tid] +
           _sliceBlockCountThreads[tid]; }
    INT32 IdGlobal() const {return _idglobal;}
    GLOBALBLOCK(const BLOCK_KEY & key, INT32 instructionCount, INT32 id,
     INT32 imgId)
        : BLOCK(key, instructionCount, id, imgId)
    { 
      _sliceBlockCountGlobal._count = 0;
      _cumulativeBlockCountGlobal._count = 0;
      _idglobal = id;
      for (THREADID tid = 0; tid < PIN_MAX_THREADS; tid++)
      {   
        _sliceBlockCountThreads[tid] = 0;
        _cumulativeBlockCountThreads[tid] = 0;
      }   
    }
    
  private:
    INT64 SliceInstructionCountGlobal() const 
        { return _sliceBlockCountGlobal._count * StaticInstructionCount(); }
    INT64 SliceInstructionCountThread(THREADID tid) const
        { return _sliceBlockCountThreads[tid] * StaticInstructionCount(); }


    GLOBAL_COUNTER64 _sliceBlockCountGlobal; 
    GLOBAL_COUNTER64 _cumulativeBlockCountGlobal; 
    BLOCK_COUNT_MAP_GLOBAL _blockCountMapGlobal; 
    
    INT32 _idglobal;

    INT64 _sliceBlockCountThreads[PIN_MAX_THREADS];
    // times this block was executed in the current slice.
    INT64 _cumulativeBlockCountThreads[PIN_MAX_THREADS];
    // times this block was executed prior to the current slice.
    BLOCK_COUNT_MAP_GLOBAL _blockCountMapThreads[PIN_MAX_THREADS];
};

class GLOBALPROFILE : public PROFILE
{
    private:
    static const UINT32 BUFSIZE=200;

    public: 
    GLOBALPROFILE(INT64 slice_size, LDV_TYPE ldv_type)
      : PROFILE(slice_size, ldv_type)
    {
        SliceTimerGlobal._count = slice_size; 
        CumulativeInstructionCountGlobal._count = 0;
        UnfilteredInstructionCount._count = 0;
        CurrentSliceSizeGlobal._count = slice_size;
        last_gblock = NULL;
    }

    // This the global version
    VOID OpenFileGlobal(UINT32 pid, std::string output_file, BOOL enable_ldv)
    {
        if ( !BbFile.is_open() )
        {
            char gnum[500];
            if (pid)
            {
                sprintf(gnum, ".global.%u", (unsigned)pid);
            }
            else
            {
                sprintf(gnum, ".global");
            }
            std::string tname = gnum;
            BbFile.open((output_file+tname+".bb").c_str());
            BbFile.setf(std::ios::showbase);

        }
    }

    
    VOID ExecuteMemoryGlobal(ADDRINT address, PIN_LOCK *global_lock = NULL) 
    { 
      if(global_lock) PIN_GetLock(global_lock, 1);
      _ldvState.access (address & ADDRESS64_MASK); 
      if(global_lock) PIN_ReleaseLock(global_lock);
    }
    VOID ExecuteMemoryThread(ADDRINT address)
        { _ldvState.access (address & ADDRESS64_MASK); }

    GLOBAL_COUNTER64 CumulativeInstructionCountGlobal;
    GLOBAL_COUNTER64 UnfilteredInstructionCount;// global or per-thread
                                                // depending on context
    GLOBAL_COUNTER64 SliceTimerGlobal;
    GLOBAL_COUNTER64 CurrentSliceSizeGlobal;
    GLOBALBLOCK *last_gblock;
};

class GLOBALISIMPOINT : public ISIMPOINT
{
    GLOBALPROFILE * globalProfile;
    GLOBALPROFILE ** threadProfiles;
    UINT64 * spinEntryCount;
    UINT64 * spinExitCount;
    BOOL * spinActive;
    GLOBALBLOCK_MAP global_block_map;
    THREADID _currentIdGlobal;
    FILTER_MOD *_filterptr;

    BOOL _vectorPendingGlobal;

    // The start addresses of the slices
    // Needed for writing the block of the last slice
    std::set<ADDRINT> _slices_start_set;
    PIN_LOCK     _slicesLock; 
    PIN_LOCK     _globalProfileLock; 
    static PIN_RWMUTEX     _StopTheWorldLock;

  public:
   GLOBALISIMPOINT() : ISIMPOINT()
    {
      _currentIdGlobal = 1;
      threadProfiles = NULL;
      _filterptr = NULL;
      globalProfile = NULL;
      threadProfiles = NULL;
      spinEntryCount = NULL;
      spinExitCount = NULL;
      spinActive = NULL;
      _filterptr = NULL;
      _vectorPendingGlobal = false;
      PIN_InitLock(&_slicesLock); 
      PIN_InitLock(&_globalProfileLock); 
    }

    BOOL VectorPendingGlobal()
      { return _vectorPendingGlobal; }

    GLOBALBLOCK_MAP * GlobalBlockMapPtr()
    {
      return &global_block_map;
    }


    VOID EmitSliceStartInfoGlobal(ADDRINT endMarker, INT64 markerCount, 
      UINT32 imgId)
    {
        PIN_GetLock(&_slicesLock, 1);
        _slices_start_set.insert(endMarker);
        PIN_ReleaseLock(&_slicesLock);

        IMG_INFO *img_info = ImageManager()->GetImageInfo(imgId);
        if(!img_info)
        {
            globalProfile->BbFile << "M: " << std::hex << endMarker << " " <<
                std::dec << markerCount << " " << "no_image" << " " 
                << std::hex << 0 << std::endl;
            return;
        }
        globalProfile->BbFile << "S: " << std::hex << endMarker << " " <<
            std::dec << markerCount << " " << img_info->Name() << " " <<
            std::hex  <<img_info->LowAddress() << " + "; 
        globalProfile->BbFile << std::hex << endMarker-img_info->LowAddress(); 
        INT32 lineNumber;
        std::string fileName;
        PIN_LockClient();
        PIN_GetSourceLocation(endMarker, NULL, &lineNumber, &fileName);
        PIN_UnlockClient();
        if(lineNumber)
        {
            globalProfile->BbFile  << " # " << fileName << std::dec <<
            ":" << lineNumber << std::endl;
        }
        else
        {
            globalProfile->BbFile  << " # Unknown:0" << std::endl;
        }
    }

    VOID EmitSliceStartInfoThread(ADDRINT endMarker, INT64 markerCount, 
        UINT32 imgId, THREADID tid,
        ADDRINT globalendMarker, INT64 globalmarkerCount, UINT32 globalimgId)

    {
        PIN_GetLock(&_slicesLock, 1);
        _slices_start_set.insert(endMarker);
        PIN_ReleaseLock(&_slicesLock);

        IMG_INFO *img_info = ImageManager()->GetImageInfo(imgId);
        IMG_INFO *globalimg_info = ImageManager()->GetImageInfo(globalimgId);
        if(!img_info)
        {
            threadProfiles[tid]->BbFile << "GM: " << std::hex << globalendMarker << " " <<
                std::dec << globalmarkerCount << " " << "no_image" << " " 
                << std::hex << 0 << std::endl;
            threadProfiles[tid]->BbFile << "M: " << std::hex << endMarker << " " <<
                std::dec << markerCount << " " << "no_image" << " " 
                << std::hex << 0 << std::endl;
            return;
        }
        ASSERTX(globalimg_info);
        threadProfiles[tid]->BbFile << "GS: " << std::hex << globalendMarker << " " <<
            std::dec << globalmarkerCount << " " << globalimg_info->Name() << " " <<
            std::hex  <<globalimg_info->LowAddress() << " + "; 
        threadProfiles[tid]->BbFile << std::hex << globalendMarker-img_info->LowAddress(); 
        INT32 lineNumber;
        std::string fileName;
        PIN_LockClient();
        PIN_GetSourceLocation(globalendMarker, NULL, &lineNumber, &fileName);
        PIN_UnlockClient();
        if(lineNumber)
        {
            threadProfiles[tid]->BbFile  << " # " << fileName << std::dec <<
            ":" << lineNumber << std::endl;
        }
        else
        {
            threadProfiles[tid]->BbFile  << " # Unknown:0" << std::endl;
        }
        threadProfiles[tid]->BbFile << "S: " << std::hex << endMarker << " " <<
            std::dec << markerCount << " " << img_info->Name() << " " <<
            std::hex  <<img_info->LowAddress() << " + "; 
        threadProfiles[tid]->BbFile << std::hex << endMarker-img_info->LowAddress(); 
        PIN_LockClient();
        PIN_GetSourceLocation(endMarker, NULL, &lineNumber, &fileName);
        PIN_UnlockClient();
        if(lineNumber)
        {
            threadProfiles[tid]->BbFile  << " # " << fileName << std::dec <<
            ":" << lineNumber << std::endl;
        }
        else
        {
            threadProfiles[tid]->BbFile  << " # Unknown:0" << std::endl;
        }
    }
    
    VOID EmitSliceEndGlobal(ADDRINT endMarker, UINT32 imgId, THREADID tid,
            UINT32 markerCountOffset=0)
    {
        INT64 markerCountGlobal = markerCountOffset;
        INT64 markerCountThread[PIN_MAX_THREADS] = {markerCountOffset};
        
        if (globalProfile->first == true)
        {
            // Input merging will change the name of the input
            globalProfile->BbFile << "I: 0" << std::endl;
             // No "P:" record for global profile
            globalProfile->BbFile << "C: sum:dummy Command:" 
                << CommandLine() << std::endl;
            EmitSliceStartInfoGlobal(globalProfile->first_eip, 1,
                     globalProfile->first_eip_imgID);        
        }
        
        globalProfile->BbFile << "# Slice ending at global " << std::dec 
            << globalProfile->CumulativeInstructionCountGlobal._count 
            << std::endl;
        globalProfile->BbFile << "# Unfiltered count  " << std::dec 
            << globalProfile->UnfilteredInstructionCount._count 
            << std::endl;

            for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
            {   
              if(threadProfiles[tnum]->active)
              {
        if (threadProfiles[tnum]->first == true)
        {
            // Input merging will change the name of the input
            threadProfiles[tnum]->BbFile << "I: 0" << std::endl;
            threadProfiles[tnum]->BbFile << "P: " << std::dec << tnum << std::endl;
            threadProfiles[tnum]->BbFile << "C: sum:dummy Command:"
                << CommandLine() << std::endl;
            EmitSliceStartInfoThread(threadProfiles[tnum]->first_eip, 1,
                     threadProfiles[tnum]->first_eip_imgID, tnum,
                     globalProfile->first_eip, 1, globalProfile->first_eip_imgID
                     );
        }
              }
              threadProfiles[tnum]->BbFile << "# Slice ending at " << std::dec
                  << threadProfiles[tnum]->CumulativeInstructionCount 
                  << " global " << globalProfile->CumulativeInstructionCountGlobal._count
                  << std::endl;
              threadProfiles[tnum]->BbFile << "# Unfiltered count " << std::dec
                  << threadProfiles[tnum]->UnfilteredInstructionCount._count
                  << " global " << globalProfile->UnfilteredInstructionCount._count
                  << std::endl;
              if ( !threadProfiles[tnum]->first || KnobEmitFirstSlice )
                  threadProfiles[tnum]->BbFile << "T" ;
            }   

        
        if ( !globalProfile->first || KnobEmitFirstSlice )
            globalProfile->BbFile << "T" ;



        for (GLOBALBLOCK_MAP::const_iterator bi = (GlobalBlockMapPtr())->begin(); 
            bi !=  (GlobalBlockMapPtr())->end(); bi++)
        {
            GLOBALBLOCK * block = bi->second;
            const BLOCK_KEY & key = bi->first;
            
            if (key.Contains(endMarker))
            {
                markerCountGlobal += block->CumulativeBlockCountGlobal();
             for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
             {   
              if(threadProfiles[tnum]->active)
              {
                markerCountThread[tnum] += block->CumulativeBlockCountThread(tnum);
              }
             }   
            }
            
            if ( !globalProfile->first || KnobEmitFirstSlice )
                block->EmitSliceEndGlobal(globalProfile);
            
            for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
            {   
              if(threadProfiles[tnum]->active)
              {
                if ( !threadProfiles[tnum]->first || KnobEmitFirstSlice )
                    block->EmitSliceEndThread(tnum, threadProfiles[tnum]);
              }
            }   
        }

        if ( !globalProfile->first || KnobEmitFirstSlice )
            globalProfile->BbFile << std::endl;

        for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
        {   
          if(threadProfiles[tnum]->active)
          {
            if ( ! threadProfiles[tnum]->first || KnobEmitFirstSlice )
              threadProfiles[tnum]->BbFile << std::endl;
          }
        }   

        if ( globalProfile->active  && !globalProfile->last)
        {
        // This is the start marker for the next slice (hence skipping for 'last') 
            if (KnobNoSymbolic)
            {
                globalProfile->BbFile << "M: " << std::hex << endMarker 
                    << " " << std::dec << markerCountGlobal << std::endl;
            }
            else
            {
                EmitSliceStartInfoGlobal(endMarker, markerCountGlobal, imgId);
            }
        }

        for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
        {   
          if(threadProfiles[tnum]->active)
          {
            if ( threadProfiles[tnum]->active  && !threadProfiles[tnum]->last)
            {
                // This is the start marker for the next slice (hence skipping for 'last') 
                if (KnobNoSymbolic)
                {
                    threadProfiles[tnum]->BbFile << "M: " << std::hex << endMarker
                        << " " << std::dec << markerCountThread[tnum] << std::endl;
                }
                else
                {
                    EmitSliceStartInfoThread(endMarker, markerCountThread[tnum], imgId,
                        tnum,
                      endMarker, markerCountGlobal, imgId);
                }
            }
          }
          threadProfiles[tnum]->BbFile.flush(); 
          threadProfiles[tnum]->first = false;            
        }   
        globalProfile->BbFile.flush(); 
        globalProfile->first = false;            
    }

    // read-only accessor.
    THREADID getCurrentIdGlobal(THREADID tid) const {
        return _currentIdGlobal;
    }

    // increment _currentId and return incremented value.
    THREADID getNextCurrentIGlobald(THREADID tid) {
        ASSERTX(tid < PIN_MAX_THREADS);
        ASSERTX(KnobEmitPrevBlockCounts);
        return _currentIdGlobal++;
    }

    static VOID EnterSpinLoop(THREADID tid, 
            GLOBALISIMPOINT *gisimpoint)
    {
      gisimpoint->spinEntryCount[tid]++;
      gisimpoint->spinActive[tid] = TRUE;
    }

    static VOID ExitSpinLoop(THREADID tid, 
            GLOBALISIMPOINT *gisimpoint)
    {
      gisimpoint->spinExitCount[tid]++;
      gisimpoint->spinActive[tid] = FALSE;
    }

    static ADDRINT GetFirstIP_IfGlobal(THREADID tid, 
            GLOBALISIMPOINT *gisimpoint)
    {
        return !gisimpoint->globalProfile->first_eip || 
                !gisimpoint->threadProfiles[tid]->first_eip;
    }
    
    static VOID GetFirstIP_ThenGlobal(VOID * ip, THREADID tid, 
         GLOBALISIMPOINT *gisimpoint, UINT32 imgID)
    {
        if( !gisimpoint->globalProfile->first_eip )
        {
          gisimpoint->globalProfile->first_eip = reinterpret_cast<ADDRINT>(ip);
          gisimpoint->globalProfile->first_eip_imgID = imgID;
          PIN_RemoveInstrumentation();        
        }
        if(!gisimpoint->threadProfiles[tid]->first_eip)
        {
          gisimpoint->threadProfiles[tid]->first_eip = 
              reinterpret_cast<ADDRINT>(ip);
          gisimpoint->threadProfiles[tid]->first_eip_imgID = imgID;
          PIN_RemoveInstrumentation();        
        }
    }


    static VOID CountBlock_Unfiltered(GLOBALBLOCK * block, THREADID tid, 
       GLOBALISIMPOINT *gisimpoint)
    {
        PIN_RWMutexReadLock(&_StopTheWorldLock);
       
        ATOMIC::OPS::Increment<INT64>
                (&gisimpoint->globalProfile->UnfilteredInstructionCount._count, 
                block->StaticInstructionCount()); 

        gisimpoint->threadProfiles[tid]->UnfilteredInstructionCount._count += 
            block->StaticInstructionCount();

        PIN_RWMutexUnlock(&_StopTheWorldLock);
    }

    static ADDRINT CountBlock_IfGlobal(GLOBALBLOCK * block,THREADID tid, 
       GLOBALISIMPOINT *gisimpoint)
    {
        if(gisimpoint->spinActive[tid]) return 0;
        PIN_RWMutexReadLock(&_StopTheWorldLock);
        block->ExecuteGlobal(tid);
       
        INT64 oldCount =  ATOMIC::OPS::Increment<INT64>
                (&gisimpoint->globalProfile->SliceTimerGlobal._count, 
                -1*block->StaticInstructionCount()); 

        gisimpoint->globalProfile->last_gblock = block;

        gisimpoint->threadProfiles[tid]->SliceTimer -= 
            block->StaticInstructionCount();
        gisimpoint->threadProfiles[tid]->last_block = block;

        PIN_RWMutexUnlock(&_StopTheWorldLock);
        
        // We are triggering region end based on global icount 
        if(KnobThreadProgress)
        {
          return ( gisimpoint->threadProfiles[tid]->SliceTimer < (INT64)0);
        }
        else
        {
          return ( (oldCount - block->StaticInstructionCount()) < (INT64)0);
        }
    }

    static ADDRINT CountBlockAndTrackPrevious_IfGlobal(
           GLOBALBLOCK * block,THREADID tid,  GLOBALISIMPOINT *gisimpoint)
    {
        if(gisimpoint->spinActive[tid]) return 0;
        PIN_RWMutexReadLock(&_StopTheWorldLock);
        block->ExecuteGlobal(tid, gisimpoint->globalProfile
                    ->last_gblock, gisimpoint);
        
        INT64 oldCount =  ATOMIC::OPS::Increment<INT64>
                (&gisimpoint->globalProfile->SliceTimerGlobal._count, 
                -1*block->StaticInstructionCount()); 
        gisimpoint->globalProfile->last_gblock = block;

        gisimpoint->threadProfiles[tid]->SliceTimer -= 
            block->StaticInstructionCount();
        gisimpoint->threadProfiles[tid]->last_block = block;

        PIN_RWMutexUnlock(&_StopTheWorldLock);
        
        // We are triggering region end based on global icount 
        return ( (oldCount - block->StaticInstructionCount()) < (INT64)0);
    }    

    static VOID ResetSliceTimerGlobal(THREADID tid, GLOBALISIMPOINT *gisimpoint)
    {
        if(gisimpoint->globalProfile->length_queue.size())
        {
          ASSERT(0,"Global length queue NYT ");
        }
        else
        {
        ATOMIC::OPS::Increment<INT64>
                (&gisimpoint->globalProfile->CumulativeInstructionCountGlobal._count, 
                    (gisimpoint->globalProfile->CurrentSliceSizeGlobal._count - 
                        gisimpoint->globalProfile->SliceTimerGlobal._count));
          if(KnobThreadProgress)
          {
            gisimpoint->globalProfile->SliceTimerGlobal._count = 
                        gisimpoint->KnobSliceSize/KnobThreadProgress;
          }
          else
          {
            gisimpoint->globalProfile->SliceTimerGlobal._count = 
                        gisimpoint->KnobSliceSize;
          }
          gisimpoint->globalProfile->CurrentSliceSizeGlobal._count = 
            gisimpoint->globalProfile->SliceTimerGlobal._count;

            for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
            {   
              if(gisimpoint->threadProfiles[tnum]->active)
              {
                gisimpoint->threadProfiles[tnum]->CumulativeInstructionCount +=
                    (gisimpoint->threadProfiles[tnum]->CurrentSliceSize -
                        gisimpoint->threadProfiles[tnum]->SliceTimer);
                if(KnobThreadProgress)
                {
                  gisimpoint->threadProfiles[tnum]->SliceTimer =
                        gisimpoint->KnobSliceSize/KnobThreadProgress;
                }
                else
                {
                  gisimpoint->threadProfiles[tnum]->SliceTimer =
                        gisimpoint->KnobSliceSize;
                }
                gisimpoint->threadProfiles[tnum]->CurrentSliceSize =
                    gisimpoint->threadProfiles[tnum]->SliceTimer;
              }
            }   
        }
    }

    static ADDRINT  CheckDelayedVectorEmissionGlobal( THREADID tid,
            GLOBALISIMPOINT *gisimpoint)
    {
        return (gisimpoint->VectorPendingGlobal());
    }
    
    static VOID DelayedVectorEmissionGlobal(THREADID tid, GLOBALBLOCK * block,
         GLOBALISIMPOINT *gisimpoint)
    {
        // A slice ended but frequency vector
        // was not emitted. Do it now.
        gisimpoint->EmitVectorForFriendGlobal(block->Key().Start(), 
            block->ImgId(), tid, gisimpoint, /*markerOffset*/1);
        // This block is not yet executed and we are using its first
        // insAddr as a marker hence we provide an offset of 1.
    }

    static VOID CountBlock_ThenGlobal(GLOBALBLOCK * block, 
         THREADID tid, GLOBALISIMPOINT *gisimpoint)
    {
        if(!gisimpoint->KnobEmitVectors) 
        {
           // do not output frequency vector but set a flag indicating
           // vector output is pending. The vector output will be 
           // triggered by another class.
           gisimpoint->_vectorPendingGlobal = TRUE;
        }
        else if(gisimpoint->KnobDelayVectorEmission)
        {
           // do not output frequency vector but set a flag indicating
           // vector output is pending. The vector output will be 
           // done at the beginning of the next basic block.
           gisimpoint->_vectorPendingGlobal = TRUE;
        }
        else
        {
          PIN_RWMutexReadLock(&_StopTheWorldLock);
            gisimpoint->ResetSliceTimerGlobal(tid, gisimpoint);
            gisimpoint->EmitSliceEndGlobal(block->Key().End(), block->ImgId(),
                                 tid);
          PIN_RWMutexUnlock(&_StopTheWorldLock);
        }
    }

    VOID EmitVectorForFriendGlobal(ADDRINT marker, UINT32 imageid,
        THREADID tid, GLOBALISIMPOINT *gisimpoint, UINT32 markerCountOffset=0)
    {
        if(!gisimpoint->_vectorPendingGlobal) return; // could be a race condition
        PIN_GetLock(&_globalProfileLock, 1);
        if (gisimpoint->globalProfile->SliceTimerGlobal._count >=  0)
        {
          // some other thread did the outputting
          PIN_ReleaseLock(&_globalProfileLock);
          return;
        }
        PIN_RWMutexReadLock(&_StopTheWorldLock);
        gisimpoint->_vectorPendingGlobal = FALSE;
        gisimpoint->ResetSliceTimerGlobal(tid, gisimpoint);
        gisimpoint->EmitSliceEndGlobal(marker, imageid, markerCountOffset, tid);
        PIN_RWMutexUnlock(&_StopTheWorldLock);
        PIN_ReleaseLock(&_globalProfileLock);
    }

    // Lookup a block by its id.
    // Return block_map.end() if not found.
    GLOBALBLOCK_MAP::const_iterator LookupGlobalBlock(INT32 id) {
        GLOBALBLOCK_MAP::const_iterator bi = GlobalBlockMapPtr()->begin();
        for (; bi != GlobalBlockMapPtr()->end(); bi++)
        {
            if (bi->second->IdGlobal() == id)
                return bi;
        }
        return bi;
    }

    // Lookup a block by its BBL key.
    // Create a new one and return it if it doesn't already exist.
    GLOBALBLOCK * LookupGlobalBlock(BBL bbl)
    {
        BLOCK_KEY key(INS_Address(BBL_InsHead(bbl)), 
            INS_Address(BBL_InsTail(bbl)), BBL_Size(bbl));
        GLOBALBLOCK_MAP::const_iterator bi = GlobalBlockMapPtr()->find(key);
        
        if (bi == GlobalBlockMapPtr()->end())
        {
            // Block not there, add it
            RTN rtn = INS_Rtn(BBL_InsHead(bbl));
            SEC sec = SEC_Invalid();
            IMG img = IMG_Invalid();
            if(RTN_Valid(rtn))
                sec = RTN_Sec(rtn);
            if(SEC_Valid(sec))
                img = SEC_Img(sec);

            GLOBALBLOCK * gblock;
            if ( KnobEmitPrevBlockCounts )
            {
                gblock = new GLOBALBLOCK(key, BBL_NumIns(bbl), 0,
                    IMG_Id(img));
            }
            else
            {
                gblock = new GLOBALBLOCK(key, BBL_NumIns(bbl), _currentIdGlobal,
                    IMG_Id(img));
                _currentIdGlobal++;
            }
            GlobalBlockMapPtr()->insert(GLOBALBLOCK_PAIR(key, gblock));
            
            return gblock;
        }
        else
        {
            return bi->second;
        }
    }

    static VOID CountMemoryGlobal(ADDRINT address, GLOBALISIMPOINT *gisimpoint)
    {
        PIN_RWMutexReadLock(&_StopTheWorldLock);
        // passing  _globalProfileLock for locking 
        gisimpoint->globalProfile->ExecuteMemoryGlobal(address, 
             &gisimpoint->_globalProfileLock);
        PIN_RWMutexUnlock(&_StopTheWorldLock);
    }

    static VOID CountMemoryThread(ADDRINT address, THREADID tid, 
                  GLOBALISIMPOINT *gisimpoint)
    {
        PIN_RWMutexReadLock(&_StopTheWorldLock);
        gisimpoint->threadProfiles[tid]->ExecuteMemoryThread(address);
        PIN_RWMutexUnlock(&_StopTheWorldLock);
    }


    BOOL DoInsertGetFirstIpInstrumentationGlobal()
    {   
        UINT32 i;
        BOOL do_instrument = !globalProfile->first_eip;
    
        for ( i = 0; i < PIN_MAX_THREADS; i++ )
        {   
            if ( threadProfiles[i]->active )
            {   
                do_instrument |= !threadProfiles[i]->first_eip;
            }
        }
        return do_instrument;
    }


    static VOID  CheckSSC(TRACE trace, UINT32 h, GLOBALISIMPOINT * gisimpoint)
    {
      enum CALL_ORDER global_order = (CALL_ORDER)(CALL_ORDER_DEFAULT + 5);
      const UINT32 pattern_len = 8;
      const unsigned int movebx_size = 5;
      const unsigned int special_nop_size = 3;
      unsigned int ins_size = 0 ,next_ins_size = 0;
      EXCEPTION_INFO excep = EXCEPTION_INFO();
      INS ins,next_ins=INS_Invalid();
      //the template of ssc marker
      unsigned char ssc_marker[] = { 0xbb, 0x00, 0x00, 0x00, 0x00,
                                   0x64, 0x67, 0x90};
      for(int j=0;j<4;j++){
        //fill in the ssc value
        ssc_marker[1+j]= (h>>(j*8))&0xff;
      }

      for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
      {
        ins = BBL_InsHead(bbl);
        if (INS_Valid(ins))
        {
            ins_size = INS_Size(ins);
            next_ins = INS_Next(ins);
        }
        while (INS_Valid(next_ins))
        {
            next_ins_size = INS_Size(next_ins);
            if (ins_size + next_ins_size == pattern_len)
            {
                unsigned char* pc;
                pc = reinterpret_cast<unsigned char*>(INS_Address(ins));
                            
                unsigned char dst_buf[pattern_len];
                size_t copy_size = PIN_FetchCode(dst_buf, pc, pattern_len, &excep);
                if (copy_size == pattern_len &&  
                  memcmp(ssc_marker,dst_buf,pattern_len) == 0){
                    IPOINT afterpoint;
                    if( BBL_HasFallThrough(bbl))
                      afterpoint = IPOINT_AFTER;
                    else if(INS_IsValidForIpointTakenBranch(BBL_InsTail(bbl))) 
                      afterpoint = IPOINT_TAKEN_BRANCH;
                    else
                      ASSERT(0, "Unable to decide after BBL instrumentation point");
                     
                   if(h==KnobSpinStartSSC)
                   {
                     BBL_InsertCall(bbl, IPOINT_BEFORE,
                        (AFUNPTR)EnterSpinLoop, 
                        IARG_CALL_ORDER, global_order,
                        IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
                     //cerr << "(inside BBL )Found START SSC marker " << std::hex << h << " in Trace " << std::hex << TRACE_Address(trace) << endl;
                   }
                   if(h==KnobSpinEndSSC)
                   {
                     BBL_InsertCall(bbl, afterpoint,
                        (AFUNPTR)ExitSpinLoop, 
                        IARG_CALL_ORDER, global_order,
                        IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
                     //cerr << "(inside BBL )Found END SSC marker " << std::hex << h << " in Trace " << std::hex << TRACE_Address(trace) << endl;
                   }
                }
            }
            ins = next_ins;
            ins_size = next_ins_size;
            next_ins = INS_Next(next_ins);
        }
        
        //For the last instruction in the BBL, we want to check the next
        //instruction, the head of the next BBL.
        if (ins_size == movebx_size)
        {
            BBL next_bbl = BBL_Next(bbl);
            if (BBL_Valid(next_bbl))
            {
                next_ins = BBL_InsHead(next_bbl);
            }
            //If the head of the next BBL can potentailly be joint with the
            //current ins to an ssc_mark, or if this is the last instruction in
            //the trace.
            if (!INS_Valid(next_ins) || INS_Size(next_ins) == special_nop_size)
            {    
                unsigned char* pc;
                pc = reinterpret_cast<unsigned char*>(INS_Address(ins));
                            
                unsigned char dst_buf[pattern_len];
                size_t copy_size = PIN_FetchCode(dst_buf, pc, pattern_len, &excep);
                if (copy_size == pattern_len &&  
                  memcmp(ssc_marker,dst_buf,pattern_len) == 0){
                    IPOINT afterpoint;
                    if( BBL_HasFallThrough(bbl))
                      afterpoint = IPOINT_AFTER;
                    else if(INS_IsValidForIpointTakenBranch(BBL_InsTail(bbl))) 
                      afterpoint = IPOINT_TAKEN_BRANCH;
                    else
                      ASSERT(0, "Unable to decide after BBL instrumentation point");
                   if(h==KnobSpinStartSSC)
                   {
                     BBL_InsertCall(bbl, IPOINT_BEFORE,
                        (AFUNPTR)EnterSpinLoop, 
                        IARG_CALL_ORDER, global_order,
                        IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
                     //cerr << "(end-of BBL )Found START SSC marker " << std::hex << h << " in Trace " << std::hex << TRACE_Address(trace) << endl;
                   }
                   if(h==KnobSpinEndSSC)
                   {
                     BBL_InsertCall(bbl, afterpoint,
                        (AFUNPTR)ExitSpinLoop, 
                        IARG_CALL_ORDER, global_order,
                        IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
                     //cerr << "(end-of BBL )Found END SSC marker " << std::hex << h << " in Trace " << std::hex << TRACE_Address(trace) << endl;
                   }
                }
            }
         }
       }
    }

    static VOID InsertUnfilteredIcounting(TRACE trace,
                   GLOBALISIMPOINT * gisimpoint)
    {
        enum CALL_ORDER global_order = (CALL_ORDER)(CALL_ORDER_DEFAULT + 5);
        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl);
            bbl = BBL_Next(bbl))
        {
            // find the block in the map or add it if new.
            GLOBALBLOCK * block = gisimpoint->LookupGlobalBlock(bbl);

            INS_InsertCall(BBL_InsTail(bbl), IPOINT_BEFORE,
                (AFUNPTR)CountBlock_Unfiltered, IARG_PTR, block,  
                IARG_CALL_ORDER, global_order,
                IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
        }
    }

    static VOID GlobalTrace(TRACE trace, VOID *v)
    {
        GLOBALISIMPOINT * gisimpoint = reinterpret_cast<GLOBALISIMPOINT *>(v);
        //enum CALL_ORDER thread_order = CALL_ORDER_DEFAULT;
        enum CALL_ORDER global_order = (CALL_ORDER)(CALL_ORDER_DEFAULT + 5);
        
        ASSERTX(KnobGlobal);
        if(gisimpoint->_filterptr)
        {
          InsertUnfilteredIcounting(trace, gisimpoint);
          if(!gisimpoint->_filterptr->SelectTrace(trace)) return;
        }

        if ( gisimpoint->KnobSpinStartSSC &&
                gisimpoint->KnobSpinEndSSC )
        {
          gisimpoint->CheckSSC(trace, KnobSpinStartSSC, gisimpoint);
          gisimpoint->CheckSSC(trace, KnobSpinEndSSC, gisimpoint);
        }

        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl);
            bbl = BBL_Next(bbl))
        {
            // find the block in the map or add it if new.
            GLOBALBLOCK * block = gisimpoint->LookupGlobalBlock(bbl);
            
    
            // insert insturmentation to get the first IP. Every thread
            // will call PIN_RemoveInstrumentation upon creation. This
            // ensures that the thread will insert instrumentation to log
            // the first eip. Once the first eip is logged,
            // PIN_RemoveInstrumentation is called again to remove the
            // instrumentation again.
            if ( gisimpoint->KnobEmitFirstSlice &&
                gisimpoint->DoInsertGetFirstIpInstrumentationGlobal() )
            {
              INS_InsertIfCall(BBL_InsHead(bbl), IPOINT_BEFORE,
                (AFUNPTR)GetFirstIP_IfGlobal, 
                IARG_CALL_ORDER, global_order,
                IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
              INS_InsertThenCall(BBL_InsHead(bbl), IPOINT_BEFORE,
                (AFUNPTR)GetFirstIP_ThenGlobal, IARG_INST_PTR, 
                IARG_CALL_ORDER, global_order,
                IARG_THREAD_ID, IARG_PTR, gisimpoint, 
                IARG_UINT32, block->ImgId(),
                IARG_END);
            }

            if ( gisimpoint->KnobEmitPrevBlockCounts )
            {
              INS_InsertIfCall(BBL_InsTail(bbl), IPOINT_BEFORE,
                (AFUNPTR)CountBlockAndTrackPrevious_IfGlobal, IARG_PTR, block,
                IARG_CALL_ORDER, global_order,
                IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
            }
            else
            {
              INS_InsertIfCall(BBL_InsTail(bbl), IPOINT_BEFORE,
                (AFUNPTR)CountBlock_IfGlobal, IARG_PTR, block, 
                IARG_CALL_ORDER, global_order,
                IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
            }
            INS_InsertThenCall(BBL_InsTail(bbl), IPOINT_BEFORE,
              (AFUNPTR)CountBlock_ThenGlobal, IARG_PTR, block,
              IARG_CALL_ORDER, global_order,
              IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);

            if(gisimpoint->KnobEmitVectors && 
                    gisimpoint->KnobDelayVectorEmission) 
            {
              INS_InsertIfCall(BBL_InsHead(bbl), IPOINT_BEFORE,
                (AFUNPTR)CheckDelayedVectorEmissionGlobal, 
                IARG_CALL_ORDER, (CALL_ORDER)global_order + 1, // before CountBlock*()
                IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
              INS_InsertThenCall(BBL_InsHead(bbl), IPOINT_BEFORE,
                (AFUNPTR)DelayedVectorEmissionGlobal, 
                IARG_CALL_ORDER, (CALL_ORDER)global_order + 1, // before CountBlock*()
                IARG_THREAD_ID, IARG_PTR, block,
                IARG_PTR, gisimpoint, IARG_END);
            }

            if (gisimpoint->_ldv_type != LDV_TYPE_NONE )
            {
              for(INS ins = BBL_InsHead(bbl); ; ins = INS_Next(ins))
              {
                // We do not count AGEN instructions here in order to avoid instrumenting
                // Emulated instruction in PIN.
                // TBD - Support AGEN memory operands if needed
                BOOL agen = false;
#if defined(EMX_INIT)
                agen = EMU_ISA::IsAgen(ins);
#endif
                if ((INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) && !agen)
                {
                  for (UINT32 i = 0; i < INS_MemoryOperandCount(ins); i++)
                    INS_InsertCall(ins, IPOINT_BEFORE,
                      (AFUNPTR)CountMemoryThread, IARG_MEMORYOP_EA, i,
                      IARG_THREAD_ID, IARG_PTR, gisimpoint, IARG_END);
                  for (UINT32 i = 0; i < INS_MemoryOperandCount(ins); i++)
                    INS_InsertCall(ins, IPOINT_BEFORE,
                      (AFUNPTR)CountMemoryGlobal, IARG_MEMORYOP_EA, i,
                      IARG_PTR, gisimpoint, IARG_END);
                }
                if (ins == BBL_InsTail(bbl))
                      break;
             }
           }
        }
    }
    
    static VOID RoutineExitSpinCheck(THREADID tid, GLOBALISIMPOINT *gisimpoint, CHAR *rtn)
    {
#if 0
      if(gisimpoint->spinActive[tid])
      {
        cerr << " tid: " << std::dec << tid << " exiting RTN " << rtn 
             << " spin flag " << gisimpoint->spinActive[tid] 
             << " resetting flag " << endl;
        //gisimpoint->spinActive[tid] = false;
      }
#endif
    }
    
    static VOID RoutineEntrySpinCheck(THREADID tid, GLOBALISIMPOINT *gisimpoint, CHAR *rtn)
    {
#if 0
      if(gisimpoint->spinActive[tid])
      {
        cerr << " tid: " << std::dec << tid << " entering RTN " << rtn 
             << " spin flag " << gisimpoint->spinActive[tid] 
             << " resetting flag " << endl;
        //gisimpoint->spinActive[tid] = false;
      }
#endif
    }

    static VOID CheckSpinFlag(IMG img, GLOBALISIMPOINT *gisimpoint)
    {
      for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
      {
        //fprintf(stderr, "  sec %s\n", SEC_name(sec).c_str());
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
          RTN_Open(rtn);
                
          RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(RoutineEntrySpinCheck),
                 IARG_THREAD_ID, IARG_PTR, gisimpoint, 
                IARG_PTR, (VOID *)RTN_Name(rtn).c_str(),IARG_END);
          RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(RoutineExitSpinCheck),
                 IARG_THREAD_ID, IARG_PTR, gisimpoint, 
                IARG_PTR, (VOID *)RTN_Name(rtn).c_str(),IARG_END);

          RTN_Close(rtn);
        }
      }
    }

    static VOID GlobalImage(IMG img, VOID * v)
    {
      GLOBALISIMPOINT * gisimpoint = reinterpret_cast<GLOBALISIMPOINT *>(v);
        
      if(KnobGlobal)
      {
        gisimpoint->globalProfile->OpenFileGlobal(gisimpoint->Pid,
          gisimpoint->KnobOutputFile.Value(), 
          gisimpoint->_ldv_type != LDV_TYPE_NONE);
        gisimpoint->globalProfile->BbFile << "G: " << IMG_Name(img)
            << " LowAddress: " << std::hex  << IMG_LowAddress(img)
            << " LoadOffset: " << std::hex << IMG_LoadOffset(img) << std::endl;
        gisimpoint->globalProfile->BbFile.flush(); 
        gisimpoint->threadProfiles[0]->OpenFile(0, gisimpoint->Pid,
            gisimpoint->KnobOutputFile.Value(), 
            gisimpoint->_ldv_type != LDV_TYPE_NONE);
        gisimpoint->ImageManager()->AddImage(img);
        gisimpoint->threadProfiles[0]->BbFile << "G: " << IMG_Name(img)
            << " LowAddress: " << std::hex  << IMG_LowAddress(img)
            << " LoadOffset: " << std::hex << IMG_LoadOffset(img) << std::endl;
        if ( gisimpoint->KnobSpinStartSSC &&
                gisimpoint->KnobSpinEndSSC )
        {
          //if (IMG_Name(img).find("libiomp5") != string::npos)
          if(IMG_IsMainExecutable(img))
          {
            CheckSpinFlag(img, gisimpoint);
          }
        }
       }
       else
       {
         gisimpoint->profiles[0]->OpenFile(0, gisimpoint->Pid,
             gisimpoint->KnobOutputFile.Value(), 
             gisimpoint->_ldv_type != LDV_TYPE_NONE);
         gisimpoint->ImageManager()->AddImage(img);
         gisimpoint->profiles[0]->BbFile << "G: " << IMG_Name(img)
             << " LowAddress: " << std::hex  << IMG_LowAddress(img)
             << " LoadOffset: " << std::hex << IMG_LoadOffset(img) << std::endl;
      }
    }


    static VOID ProcessFini(INT32 code, VOID *v)
    {
        GLOBALISIMPOINT * gisimpoint = reinterpret_cast<GLOBALISIMPOINT *>(v);
        
        if ( gisimpoint->KnobEmitLastSlice &&
            gisimpoint->globalProfile->SliceTimerGlobal._count != 
                gisimpoint->globalProfile->CurrentSliceSizeGlobal._count )
        {
          BLOCK * block = gisimpoint->globalProfile->last_gblock;
          if(gisimpoint->KnobEmitVectors) 
          {
            for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
            {   
              if(gisimpoint->threadProfiles[tnum]->active)
              {
            if (gisimpoint->threadProfiles[tnum]->SliceTimer != 
                gisimpoint->threadProfiles[tnum]->CurrentSliceSize )
            {
              if(gisimpoint->KnobEmitVectors) 
              {
                gisimpoint->threadProfiles[tnum]->last = true; // this is the last slice
                // the calls ResetSliceTimerGlobal() and EmitSliceEndGlobal()
                // later will  handle all threads as well
              }
            }
              }
            }   
            gisimpoint->globalProfile->last = true; // this is the last slice
            // the calls ResetSliceTimerGlobal() and EmitSliceEndGlobal()
            // below handle all active threadProfiles as well
            gisimpoint->ResetSliceTimerGlobal(/*tid*/0, gisimpoint);
            gisimpoint->EmitSliceEndGlobal(block->Key().End(), block->ImgId(),
             /*tid*/0);
          }
        }
        gisimpoint->globalProfile->active = false;    
        gisimpoint->EmitProgramEndGlobal(gisimpoint);
        gisimpoint->globalProfile->BbFile << "End of bb" << std::endl;
        gisimpoint->globalProfile->BbFile.close();
        for (THREADID tnum = 0; tnum < PIN_MAX_THREADS; tnum++)
        {   
          if(gisimpoint->threadProfiles[tnum]->active)
          {
            gisimpoint->threadProfiles[tnum]->BbFile 
              << "#Start SSC marker " << std::hex << gisimpoint->KnobSpinStartSSC
               << " count " << std::dec << gisimpoint->spinEntryCount[tnum] << endl;
            gisimpoint->threadProfiles[tnum]->BbFile 
              << "#End SSC marker " << std::hex << gisimpoint->KnobSpinEndSSC
               << " count " << std::dec << gisimpoint->spinExitCount[tnum] << endl;
            gisimpoint->threadProfiles[tnum]->active = false;    
            gisimpoint->EmitProgramEndThread(tnum, gisimpoint);
            gisimpoint->threadProfiles[tnum]->BbFile << "End of bb" << std::endl;
            gisimpoint->threadProfiles[tnum]->BbFile.close();
          }
        }   
    }

    static VOID GlobalThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) 
    {   
        GLOBALISIMPOINT * gisimpoint = reinterpret_cast<GLOBALISIMPOINT *>(v);
    
        ASSERTX(tid < PIN_MAX_THREADS);
        if(KnobGlobal)
        {
          gisimpoint->threadProfiles[tid]->OpenFile(tid, gisimpoint->Pid,
              gisimpoint->KnobOutputFile.Value(),
              gisimpoint->_ldv_type != LDV_TYPE_NONE);
          gisimpoint->threadProfiles[tid]->active = true;
          if(tid==0) gisimpoint->globalProfile->active = true;
          PIN_RemoveInstrumentation();    
        }
        else
        {
          gisimpoint->profiles[tid]->OpenFile(tid, gisimpoint->Pid,
              gisimpoint->KnobOutputFile.Value(),
              gisimpoint->_ldv_type != LDV_TYPE_NONE);
          gisimpoint->profiles[tid]->active = true;
          PIN_RemoveInstrumentation();    
        }
    }   

    static VOID GlobalThreadFini(UINT32 tid, const CONTEXT *ctxt, INT32 code, VOID *v)
    {
        GLOBALISIMPOINT * gisimpoint = reinterpret_cast<GLOBALISIMPOINT *>(v);
        
        if(!KnobGlobal) // ProcessFini() will handle the KnobGlobal==true case
        {
          if ( gisimpoint->KnobEmitLastSlice &&
            gisimpoint->profiles[tid]->SliceTimer != 
                gisimpoint->profiles[tid]->CurrentSliceSize )
          {
            BLOCK * block = gisimpoint->profiles[tid]->last_block;
            if(gisimpoint->KnobEmitVectors) 
            {
              gisimpoint->profiles[tid]->last = true; // this is the last slice
              gisimpoint->ResetSliceTimer(tid, gisimpoint);
              gisimpoint->EmitSliceEnd(block->Key().End(), block->ImgId(), tid);
            }
          }
          gisimpoint->profiles[tid]->active = false;    
          gisimpoint->EmitProgramEnd(tid, gisimpoint);
          gisimpoint->profiles[tid]->BbFile << "End of bb" << std::endl;
          gisimpoint->profiles[tid]->BbFile.close();
        }
    }
    
    
    VOID GlobalAddInstrumentation(int argc, char *argv[])
    {
        GetCommand(argc, argv);
        
        if(KnobGlobal)
        {
          cerr << "-thread_progress " << KnobThreadProgress;
          if(KnobThreadProgress)
          {
            cerr << " using slicesize " << KnobSliceSize << 
              "/" << KnobThreadProgress << " == " << KnobSliceSize/KnobThreadProgress << endl;
            globalProfile = new GLOBALPROFILE(KnobSliceSize/KnobThreadProgress,
                _ldv_type);
          }
          else
          {
            cerr << " using slicesize " << KnobSliceSize << endl;
            globalProfile = new GLOBALPROFILE(KnobSliceSize, _ldv_type);
          }
          threadProfiles = new GLOBALPROFILE* [PIN_MAX_THREADS];
          memset(threadProfiles, 0, PIN_MAX_THREADS * sizeof(threadProfiles[0]));
          spinEntryCount = new UINT64 [PIN_MAX_THREADS];
          memset(spinEntryCount, 0, PIN_MAX_THREADS * sizeof(spinEntryCount[0]));
          spinExitCount = new UINT64 [PIN_MAX_THREADS];
          memset(spinExitCount, 0, PIN_MAX_THREADS * sizeof(spinExitCount[0]));
          spinActive = new BOOL [PIN_MAX_THREADS];
          memset(spinActive, 0, PIN_MAX_THREADS * sizeof(spinActive[0]));
        }
        else
        {
          cerr << " (no global profiling) using slicesize " << KnobSliceSize << endl;
          globalProfile = NULL;
          profiles = new PROFILE* [PIN_MAX_THREADS];
          memset(profiles, 0, PIN_MAX_THREADS * sizeof(profiles[0]));
        }
        
        if (KnobPid)
        {
            Pid = getpid();
        }
        
        PIN_AddThreadStartFunction(GlobalThreadStart, this);
        PIN_AddThreadFiniFunction(GlobalThreadFini, this);
        if(KnobGlobal) PIN_AddFiniFunction(ProcessFini, this);
        
        for (THREADID tid = 0; tid < PIN_MAX_THREADS; tid++)
        {
          if(KnobGlobal)
          {
            if(KnobThreadProgress)
            {
              threadProfiles[tid] = new GLOBALPROFILE(KnobSliceSize/KnobThreadProgress,
                  _ldv_type);
            }
            else
            {
              threadProfiles[tid] = new GLOBALPROFILE(KnobSliceSize, _ldv_type);
            }
          }
          else
          {
            profiles[tid] = new PROFILE(KnobSliceSize, _ldv_type);
          }
        }

        if(KnobGlobal)
        {
         // Length files not supported with global profiling yet
          UINT32 num_length_files = KnobLengthFile.NumberOfValues();
          ASSERT(num_length_files ==0, "Length file not supported with global profiling yet.");
        }
        else
        {
          UINT32 num_length_files = KnobLengthFile.NumberOfValues();
          ASSERTX(num_length_files < PIN_MAX_THREADS);
          for (UINT32 i = 0; i < num_length_files; i++)
          {
            std::string val = KnobLengthFile.Value(i);
            std::string fn;
            UINT32 tid;
            BOOL tidfound  = ParseFilenameTid(val, &fn, &tid);
            if ( !tidfound)
            {
              // skipping 'tidN' suffix ok only if one -lengthfile is
              // provided and then tid 0 is assumed.
              ASSERT(num_length_files==1, 
                "missing 'tidN' suffix to lengthfile:"+val);
                    tid = 0;
                }
                profiles[tid]->ReadLengthFile((THREADID)tid, fn);
         }
        }
        
#if defined(TARGET_MAC)
        // On Mac, ImageLoad() works only after we call PIN_InitSymbols().
        PIN_InitSymbols();
#endif
        
      if(KnobGlobal)
      {
        TRACE_AddInstrumentFunction(GlobalTrace, this);
      }
      else
      {
        TRACE_AddInstrumentFunction(Trace, this);
      }
     IMG_AddInstrumentFunction(GlobalImage, this);    
    }

    VOID activate(int argc, char** argv, FILTER_MOD *filter, VOID *spinloop=NULL)
    {
        _filterptr = filter;
        if (isimpoint_knob)
        {
            if (KnobLDVType.Value() == "none")
                _ldv_type = LDV_TYPE_NONE;
            else if (KnobLDVType.Value() == "approx")
                _ldv_type = LDV_TYPE_APPROXIMATE;
            else if (KnobLDVType.Value() == "exact")
                _ldv_type = LDV_TYPE_EXACT;
            else
                ASSERT(0,"Invalid ldv_type: "+KnobLDVType.Value());
            GlobalAddInstrumentation(argc, argv);
        }
    }
    
    VOID EmitProgramEndGlobal(const GLOBALISIMPOINT * gisimpoint)
    {
        ASSERT(KnobGlobal, "-global_profile is disabled!");
        globalProfile->BbFile << "Dynamic instruction count "
             << std::dec << globalProfile->CumulativeInstructionCountGlobal._count 
                 << std::endl;
        globalProfile->BbFile << "Dynamic unfiltered instruction count "
             << std::dec << globalProfile->UnfilteredInstructionCount._count 
                 << std::endl;
        globalProfile->BbFile << "# Filter knobs: "
             << std::dec << gisimpoint->_filterptr->FilterKnobString()
                 << std::endl;
        if(KnobThreadProgress)
        {
          globalProfile->BbFile << "SliceSize: " << std::dec << KnobSliceSize/KnobThreadProgress << std::endl;
        }
        else
        {
          globalProfile->BbFile << "SliceSize: " << std::dec << KnobSliceSize << std::endl;
        }
        if ( KnobEmitPrevBlockCounts )
        {
          ASSERT(0,"KnobEmitPrevBlockCounts in EmitProgramEndGlobal() NYT "); 
        }
        else
        {
            for (GLOBALBLOCK_MAP::const_iterator bi = (GlobalBlockMapPtr())->begin(); 
            bi !=  (GlobalBlockMapPtr())->end(); bi++)
            {
                bi->second->EmitProgramEndGlobal(bi->first, globalProfile,
                     gisimpoint);
            }
        }
    }

    VOID EmitProgramEndThread(THREADID tid, const GLOBALISIMPOINT * gisimpoint)
    {
        ASSERTX(KnobGlobal);
        threadProfiles[tid]->BbFile << "Dynamic instruction count "
             << std::dec << threadProfiles[tid]->CumulativeInstructionCount << std::endl;
        threadProfiles[tid]->BbFile << "Dynamic unfiltered instruction count "
             << std::dec << threadProfiles[tid]->UnfilteredInstructionCount._count << std::endl;
          if(KnobThreadProgress)
          {
            threadProfiles[tid]->BbFile << "SliceSize: " << std::dec << KnobSliceSize/KnobThreadProgress << std::endl;
          }
          else
          {
            threadProfiles[tid]->BbFile << "SliceSize: " << std::dec << KnobSliceSize << std::endl;
          }
        if ( KnobEmitPrevBlockCounts )
        {
            // Emit blocks in the order that they were first executed.
            for (UINT32 id = 1; id < getCurrentId(tid); id++) {
                GLOBALBLOCK_MAP::const_iterator bi = LookupGlobalBlock(id);
                if (bi != GlobalBlockMapPtr()->end())
                    bi->second->EmitProgramEndThread(bi->first, tid, threadProfiles[tid],
                        gisimpoint);
            }
        }
        else
        {
            for (GLOBALBLOCK_MAP::const_iterator bi = GlobalBlockMapPtr()->begin();
                 bi != GlobalBlockMapPtr()->end(); bi++)
            {
                bi->second->EmitProgramEndThread(bi->first, tid, threadProfiles[tid],
                     gisimpoint);
            }
        }
    }
    static KNOB<BOOL>  KnobGlobal;
    static KNOB<INT32>  KnobThreadProgress;
    static KNOB<UINT32>  KnobSpinStartSSC;
    static KNOB<UINT32>  KnobSpinEndSSC;
};
#endif
// add in global_isimpoint_inst.cpp
#if 0
KNOB<UINT32> GLOBALISIMPOINT::KnobSpinStartSSC(KNOB_MODE_WRITEONCE,  
    "pintool:isimpoint",
    "spin_start_SSC", "0", "SSC marker (0x...) for the start of spin loop to be skipped ");
KNOB<UINT32> GLOBALISIMPOINT::KnobSpinEndSSC(KNOB_MODE_WRITEONCE,  
    "pintool:isimpoint",
    "spin_end_SSC", "0", "SSC marker (0x...) for the end of spin loop to be skipped ");
#endif
