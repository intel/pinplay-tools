/* BEGIN_LEGAL
# The MIT License (MIT)
#
# Copyright (c) 2022, National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
 END_LEGAL */
#include "pin.H"
#include "pin_lock.h"
#include "pinplay.H"

#include <assert.h>
#include <vector>

#include "cond.h"

#include <iostream>
#include <numeric>

#define FC_DEBUG 0

namespace flowcontrol {

#define str(s) xstr(s)
#define xstr(s) #s
//#define DEFAULT_QUANTUM 1000
#define DEFAULT_QUANTUM 0 // Causes flow-control to be disabled by default
#define DEFAULT_MAXTHREADS 16
#define DEFAULT_VERBOSE 0

KNOB<UINT64>knobQuantum(KNOB_MODE_WRITEONCE,
                       "pintool",
                       "flowcontrol:quantum",
                       str(DEFAULT_QUANTUM),
                       "Flow control quantum (instructions, default " str(DEFAULT_QUANTUM) ").");

KNOB<UINT64>knobMaxThreads(KNOB_MODE_WRITEONCE,
                       "pintool",
                       "flowcontrol:maxthreads",
                       str(DEFAULT_MAXTHREADS),
                       "Flow control maximum number of threads (default " str(DEFAULT_MAXTHREADS) ").");

KNOB<BOOL>knobVerbose(KNOB_MODE_WRITEONCE,
                      "pintool",
                      "flowcontrol:verbose",
                      str(DEFAULT_VERBOSE),
                      "Flow control maximum number of threads (default " str(DEFAULT_VERBOSE) ").");

class flowcontrol {

private:
  PINPLAY_ENGINE *pinplay_engine = NULL;
  std::vector<ConditionVariable*> m_core_cond;
  std::vector<bool> m_active;
  std::vector<bool> m_not_in_syscall;
  std::vector<bool> m_barrier_acquire_list;
  std::vector<uint64_t> m_flowcontrol_target;
  std::vector<uint64_t> m_insncount;
  std::vector<uint64_t> m_threads_to_finish;
  std::vector<uint64_t> m_threads_to_signal;
  uint64_t m_sync = 0;
  uint64_t m_acc_count = 0;
  int m_max_num_threads = 0;
  Lock lock;
#if FC_DEBUG >= 1
  Lock iolock;
#endif

public:

  // Call activate() to allow for initialization with static allocation
  flowcontrol() {}

  // Cleanup conditional variables
  ~flowcontrol() {
    for (int i = 0 ; i < m_max_num_threads ; i++) {
      if (m_core_cond[i]) {
        delete m_core_cond[i];
      }
    }
  }

  static VOID handleSyncCallback(BOOL enter, THREADID threadid, THREADID remote_threadid, UINT64 remote_icount, VOID* v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
    fc->syncCallback(enter, threadid, remote_threadid, remote_icount);
  }

  static VOID handleThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
    fc->threadStart(threadid, ctxt, flags);
  }

  static VOID threadFinishHelper(VOID *arg)
  {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(arg);
    {
      ScopedLock sl(fc->lock);
      for (auto i : fc->m_threads_to_finish) {
        fc->m_active[i] = false;
        fc->m_insncount[i] = false;
      }
      fc->m_threads_to_finish.clear();
    }
  }

  static VOID handleThreadFinish(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
#if FC_DEBUG >= 1
    {
      ScopedLock sl(fc->iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
    }
#endif

    // might be under sync/pinplay paused
    //assert(fc->m_active[threadid]);

    {
      ScopedLock sl(fc->lock);
      fc->m_threads_to_finish.push_back(threadid);
    }
    PIN_SpawnInternalThread(flowcontrol::threadFinishHelper, v, 0, NULL);
  }

  static VOID handleTraceCallback(TRACE trace, VOID *v) {
    flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
    fc->traceCallback(trace);
  }

  static VOID countInsns(THREADID threadid, INT32 count, flowcontrol *fc) {
    // Hack for now
    // When threads start, they could start paused.
    // No way to detect this yet.
    // Instead, just set running if we are here.
    //fc->m_active[threadid] = true;
#if FC_DEBUG >= 5
    {
      ScopedLock sl(fc->iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] icount=" << (fc->m_insncount[threadid] + count) << "\n";
    }
#endif
    fc->m_insncount[threadid] += count;

	if (knobVerbose.Value() && threadid == 0) {
		if (std::accumulate(fc->m_insncount.begin(), fc->m_insncount.end(), 
					decltype(fc->m_insncount)::value_type(0)) >= fc->m_acc_count) {
			std::cerr << "[" <<  __FUNCTION__ << "] Per-thread instruction counts: [";
			for (int i=0; i < fc->m_max_num_threads; i++) {
				std::cerr << " " <<  fc->m_insncount[i];
			}
			std::cerr << " ]" << std::endl;
			fc->m_acc_count += fc->m_sync * std::accumulate(fc->m_active.begin(), fc->m_active.end(), 0);
		}
	}

    if (fc->m_insncount[threadid] >= fc->m_flowcontrol_target[threadid]) {
#if FC_DEBUG >= 1
      {
        ScopedLock sl(fc->iolock);
        std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Trying to sync [" << fc->m_insncount[threadid] << "]\n" << std::flush;
      }
#endif
      // Bug (?) with ScopedLock had it being destroyed before wait() below was being called
      //ScopedLock(fc->lock);
      fc->lock.acquire();
      fc->m_barrier_acquire_list[threadid] = true;
      // Synchronize threads
      if (!fc->isBarrierReached("countInsns/Sync", threadid)) {
        fc->m_core_cond[threadid]->wait(fc->lock);
#if FC_DEBUG >= 1
        {
          ScopedLock sl(fc->iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Woke up\n" << std::flush;
        }
#endif
      } else {
        fc->doRelease(threadid);
      }
      fc->lock.release();
      // Everyone reached the barrier. Let's progress to the next target
      fc->m_flowcontrol_target[threadid] += fc->m_sync; 
    }

    // Signal threads if needed. Can't do this in doRelease as we need to be able to acquire the lock
    fc->finishRelease();
  }

  // Hold lock
  bool isBarrierReached(std::string msg = "", THREADID threadid = -1) {
#if FC_DEBUG >= 3
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] msg: " << msg << "\n" << std::flush;
    }
#endif
    // Have all running threads reached the barrier yet?
    bool any_active = false;
    for (int i = 0 ; i < m_max_num_threads ; i++) {
      if (m_active[i] && (m_not_in_syscall[i]) && (!m_barrier_acquire_list[i])) {
        // Not yet. Wait a bit.
#if FC_DEBUG >= 2
        {
          ScopedLock sl(iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] active: " << m_active[i] << " & !barrier: " << !m_barrier_acquire_list[i] << " threadid= " << i << " is still active" << "\n" << std::flush;
        }
#endif
        any_active = true;
      }
    }
    if (any_active) {
      return false;
    }
    // Excellent. All valid entries are there now
    return true;
  }

  // Hold lock
  void doRelease(THREADID threadid = -1) {
#if FC_DEBUG >= 3
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] icount=" << (m_insncount[threadid]) << "\n" << std::flush;
    }
#endif
    std::vector<bool> bal(m_barrier_acquire_list);
    for(int i = 0 ; i < m_max_num_threads ; i++) {
      if (bal[i]) {
#if FC_DEBUG >= 3
        {
          ScopedLock sl(iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] clearing bal " << i << "\n" << std::flush;
        }
#endif
        m_barrier_acquire_list[i] = false;
      }
    }
    for(int i = 0 ; i < m_max_num_threads ; i++) {
      if (bal[i]) {
        if (static_cast<int>(threadid) != i) {
#if FC_DEBUG >= 3
          {
            ScopedLock sl(iolock);
            std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] signaling thread " << i << "\n" << std::flush;
          }
#endif
          m_threads_to_signal.push_back(i);
          //m_core_cond[i]->signal();
        }
      }
    }
  }

  // Do not hold the lock
  void finishRelease() {
    std::vector<uint64_t> m_lcl_tts(m_threads_to_signal);
    m_threads_to_signal.clear();

    for (auto tts : m_lcl_tts) {
      m_core_cond[tts]->signal();
    }
  }

  void activate(PINPLAY_ENGINE *_pinplay_engine = NULL) {

    m_sync = knobQuantum.Value();
    // Are we disabled?
    if (m_sync == 0) {
      return;
    }

#if FC_DEBUG >= 1
    lock.debug(true, &iolock);
#endif

    // Save our maximum thread count
    m_max_num_threads = knobMaxThreads.Value();
    m_acc_count = m_sync;

    // Initialize condition variables
    m_core_cond.resize(m_max_num_threads);
    for (int i = 0 ; i < m_max_num_threads ; i++) {
      m_core_cond[i] = new ConditionVariable();
    }

    // Initialize the flow control target
    m_flowcontrol_target.resize(m_max_num_threads, 0);

    // Initialize the instruction counts
    m_insncount.resize(m_max_num_threads, 0);

    // Initialize the flow control target
    m_active.resize(m_max_num_threads, 0);
    m_not_in_syscall.resize(m_max_num_threads, true);

    m_barrier_acquire_list.resize(m_max_num_threads, false);

    // Save the pinplay engine
    pinplay_engine = _pinplay_engine;
    if (pinplay_engine != NULL) {
      pinplay_engine->RegisterSyncCallback(flowcontrol::handleSyncCallback, this);
    }

    PIN_AddThreadStartFunction(flowcontrol::handleThreadStart, this);
    PIN_AddThreadFiniFunction(flowcontrol::handleThreadFinish, this);
    TRACE_AddInstrumentFunction(flowcontrol::handleTraceCallback, this);

    // Instead of using the syscall entry / exit functions, let's do this manually by looking for the syscall instruction below
    PIN_AddSyscallEntryFunction(syscallEntryCallback, this);
    PIN_AddSyscallExitFunction(syscallExitCallback, this);

    if (knobVerbose.Value()) {
      std::cerr << "[FLOWCONTROL] Enabled. Sync quantum: " << m_sync << " Max threads: " << m_max_num_threads << " Pinplay Engine: " << (pinplay_engine == NULL ? "Disabled" : "Enabled") << "\n";
    }

  }

  void syncCallback(BOOL enter, THREADID threadid, THREADID remote_threadid, UINT64 remote_icount) {
#if FC_DEBUG >= 1
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] enter=" << enter << " remote_threadid=" << remote_threadid << " remote_icount=" << remote_icount << "\n" << std::flush;
    }
#endif

    {
      ScopedLock sl(lock);

      // If there is already all threads (except this one) in the barrier, be sure to wake everyone up
      if (enter) {
	// enter
	assert(m_active[threadid]);
	m_active[threadid] = false; 

	// We are no longer active. Check for barriers before we leave
	if (isBarrierReached("syncCallback", threadid)) {
	  doRelease(threadid);
	}

      } else {
	// exit
	assert(!m_active[threadid]);
	m_active[threadid] = true;
      }
    }

    // Signal threads if needed. Can't do this in doRelease as we need to be able to acquire the lock
    finishRelease();

  }

  void threadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags) {
#if FC_DEBUG >= 1
    {
      ScopedLock sl(iolock);
      std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
    }
#endif

    assert(!m_active[threadid]);

    m_flowcontrol_target[threadid] = m_sync;
    m_active[threadid] = true;
    m_not_in_syscall[threadid] = true;

  }

  static VOID syscallEntryCallbackHelper(THREADID threadid, VOID *v)
  {
      flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
#if FC_DEBUG >= 1
      {
        ScopedLock sl(fc->iolock);
        std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
      }
#endif

      {
        ScopedLock sl(fc->lock);
        // Potential issue with sync and syscalls together. We might need to split the active variable
        assert(fc->m_not_in_syscall[threadid]);
        /*if (!fc->m_not_in_syscall[threadid]) {
          {
            ScopedLock sl(fc->iolock);
            std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] WARNING: Value of m_not_in_syscall is false already.\n" << std::flush;
          }
        }*/
        fc->m_not_in_syscall[threadid] = false; 

        // We are no longer active. Check for barriers before we leave
        if (fc->isBarrierReached("syncCallback", threadid)) {
          fc->doRelease(threadid);
        }
      }

      // Signal threads if needed. Can't do this in doRelease as we need to be able to acquire the lock
      fc->finishRelease();

  }

  static VOID syscallEntryCallback(THREADID threadid, CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard, VOID *v)
  {
    syscallEntryCallbackHelper(threadid, v);
  }

  static VOID syscallExitCallbackHelper(THREADID threadid, VOID *v)
  {
      flowcontrol *fc = reinterpret_cast<flowcontrol*>(v);
      assert(!fc->m_not_in_syscall[threadid]);
      /*if(fc->m_not_in_syscall[threadid]) {
        {
          ScopedLock sl(fc->iolock);
          std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] WARNING: Value of m_not_in_syscall is true already.\n" << std::flush;
        }
      }*/
      {
        ScopedLock sl(fc->lock);
        fc->m_not_in_syscall[threadid] = true;
      }
#if FC_DEBUG >= 1
      {
        ScopedLock sl(fc->iolock);
        std::cerr << "[" <<  __FUNCTION__ << ":" << threadid << "] Called\n" << std::flush;
      }
#endif
  }

  static VOID syscallExitCallback(THREADID threadid, CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard, VOID *v)
  {
    syscallExitCallbackHelper(threadid, v);
  }

  void traceCallback(TRACE trace) {
    BBL bbl_head = TRACE_BblHead(trace);
    for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

      bool syscalls_in_bb = false;
      // Commenting out syscall check as we encounter runtime issues with it
      // Are there any syscalls here?
      for(INS ins = BBL_InsHead(bbl); INS_Valid(ins) ; ins = INS_Next(ins))
      {
        if (INS_IsSyscall(ins))
        {
          syscalls_in_bb = true;
          break;
        }
      }

      // Setup callbacks
      if (syscalls_in_bb)
      {
        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins) ; ins = INS_Next(ins))
        {
          if (INS_IsSyscall(ins))
          {
            //INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(flowcontrol::syscallEntryCallbackHelper), IARG_THREAD_ID, IARG_ADDRINT, this, IARG_END);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)flowcontrol::countInsns, IARG_THREAD_ID, IARG_UINT32, (UINT32) 1, IARG_ADDRINT, this, IARG_END);
            // The code below will cause a Pin error
            // E: flowcontrol.h:449 Inserting IPOINT_AFTER with invalid instruction (   178 0x00007f8fbfb124b7 syscall ). See INS_IsValidForIpointAfter()
            //INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(flowcontrol::syscallExitCallbackHelper), IARG_THREAD_ID, IARG_ADDRINT, this, IARG_END);
          }
          else
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)flowcontrol::countInsns, IARG_THREAD_ID, IARG_UINT32, (UINT32) 1, IARG_ADDRINT, this, IARG_END);
          }
        }
      }
      else
      {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)flowcontrol::countInsns, IARG_THREAD_ID, IARG_UINT32, BBL_NumIns(bbl), IARG_ADDRINT, this, IARG_END);
      }
    }
  }

};

}
