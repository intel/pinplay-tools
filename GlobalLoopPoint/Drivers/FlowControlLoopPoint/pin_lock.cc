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
#include <iostream>
#include "pin_lock.h"

PinLock::PinLock()
{
   PIN_InitLock(&_lock);
}

PinLock::~PinLock()
{
}

void PinLock::acquire()
{
   //PIN_GetLock(&_lock, 1);
   if (m_debug) {
       if (_iolock) {
           _iolock->acquire();
       }
       std::cerr << "[acquire:" << PIN_ThreadId() << "] Last held by " << _lock._owner << "\n" << std::flush;
       if (_iolock) {
           _iolock->release();
       }
   }
   PIN_GetLock(&_lock, PIN_ThreadId());
}

void PinLock::release()
{
   if (m_debug) {
       if (_iolock) {
           _iolock->acquire();
       }
       std::cerr << "[release:" << PIN_ThreadId() << "] Last held by " << _lock._owner << "\n" << std::flush;
       if (_iolock) {
           _iolock->release();
       }
   }
   if (_lock._owner == -1) {
       if (_iolock) {
           _iolock->acquire();
       }
       std::cerr << "[release:" << PIN_ThreadId() << "] ERROR " << _lock._owner << " Expected owner != -1\n" << std::flush;
       if (_iolock) {
           _iolock->release();
       }
   }
   // Debugging, is int32
   _lock._owner = -1;
   PIN_ReleaseLock(&_lock);
}

#if 1
LockImplementation* LockCreator_Default::create()
{
   return new PinLock();
}
LockImplementation* LockCreator_RwLock::create()
{
   return new PinLock();
}
#endif

LockImplementation* LockCreator_Spinlock::create()
{
    return new PinLock();
}
