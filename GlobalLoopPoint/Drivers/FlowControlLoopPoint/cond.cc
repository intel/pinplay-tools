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
#include "cond.h"
//#include "os_compat.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <asm/errno.h> // For EINTR on older kernels
#include <limits.h>

ConditionVariable::ConditionVariable()
   : m_futx(0)
{
   #ifdef TIME_LOCKS
   _timer = TotalTimer::getTimerByStacktrace("cond@" + itostr(this));
   #endif
}

ConditionVariable::~ConditionVariable()
{
   #ifdef TIME_LOCKS
   delete _timer;
   #endif
}

void ConditionVariable::wait(Lock& lock, uint64_t timeout_ns)
{
   m_lock.acquire();

   // Wait
   m_futx = 0;

   m_lock.release();

   lock.release();

   #ifdef TIME_LOCKS
   ScopedTimer tt(*_timer);
   #endif

   struct timespec timeout = { time_t(timeout_ns / 1000000000), time_t(timeout_ns % 1000000000) };

   int res;
   do {
      res = syscall(SYS_futex, (void*) &m_futx, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0, timeout_ns > 0 ? &timeout : NULL, NULL, 0);
      // Restart futex_wait if it was interrupted by a signal
   }
   while (res == -EINTR);

   lock.acquire();
}

void ConditionVariable::signal()
{
   m_lock.acquire();

   m_futx = 1;

   syscall(SYS_futex, (void*) &m_futx, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, NULL, NULL, 0);

   m_lock.release();
}

void ConditionVariable::broadcast()
{
   m_lock.acquire();

   m_futx = 1;

   syscall(SYS_futex, (void*) &m_futx, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, INT_MAX, NULL, NULL, 0);

   m_lock.release();
}
