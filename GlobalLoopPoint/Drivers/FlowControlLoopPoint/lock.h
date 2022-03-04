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
#ifndef LOCK_H
#define LOCK_H

//#define TIME_LOCKS

#ifdef TIME_LOCKS
#include "itostr.h"
#include "timer.h"
#endif


/* Lock Implementation and Creator class (allows ::create to be overridden from inside /pin) */

class LockImplementation
{
public:
   LockImplementation() {}
   virtual ~LockImplementation() {}

   virtual void acquire() = 0;
   virtual void release() = 0;
   virtual void acquire_read() { acquire(); }
   virtual void release_read() { release(); }

   virtual void debug(bool enable, void *v) = 0;
};

class LockCreator
{
   public:
      static LockImplementation* create();
};


/* Actual Lock class to use in other objects */

class BaseLock
{
   public:
      virtual void acquire() = 0;
      virtual void release() = 0;
      virtual void acquire_read() = 0;
      virtual void release_read() = 0;
      virtual void debug(bool enable, void *v) = 0;
};

template <class T_LockCreator> class TLock : public BaseLock
{
public:
   TLock()
   {
      _lock = T_LockCreator::create();
      #ifdef TIME_LOCKS
      _timer = TotalTimer::getTimerByStacktrace("lock@" + itostr(this));
      #endif
   }

   virtual ~TLock()
   {
      delete _lock;
      #ifdef TIME_LOCKS
      delete _timer;
      #endif
   }

// to avoid "Freeing freed memory due to missing operator"
// If it's not expected that class instances are copied, the operator= 
// should be declared as private. In this case, if an attempt to copy is 
// made, the compiler produces an error.
private: 
   TLock& operator=(const TLock&){
     return *this;
   }

public:
   void acquire()
   {
      #ifdef TIME_LOCKS
      ScopedTimer tt(*_timer);
      #endif
      _lock->acquire();
      //std::cerr << "  >>> Acquired Lock\n";
   }

   void acquire_read()
   {
      #ifdef TIME_LOCKS
      ScopedTimer tt(*_timer);
      #endif
      _lock->acquire_read();
   }

   void release()
   {
      //std::cerr << "  <<< About to Release Lock Lock\n";
      _lock->release();
   }

   void release_read()
   {
      _lock->release_read();
   }

   virtual void debug(bool enable, void *v) {
      _lock->debug(enable, v);
   }

private:
   LockImplementation* _lock;
   #ifdef TIME_LOCKS
   TotalTimer* _timer;
   #endif
};


class LockCreator_Default : public LockCreator
{
   public:
      static LockImplementation* create();
};
class LockCreator_RwLock : public LockCreator
{
   public:
      static LockImplementation* create();
};
class LockCreator_Spinlock : public LockCreator
{
   public:
      static LockImplementation* create();
};
class LockCreator_NullLock : public LockCreator
{
   public:
      static LockImplementation* create();
};

typedef TLock<LockCreator_Default> Lock;
typedef TLock<LockCreator_RwLock> RwLock;
typedef TLock<LockCreator_Spinlock> SpinLock;
typedef TLock<LockCreator_NullLock> NullLock;


/* Helper class: hold a lock for the scope of this object */

class ScopedLock
{
   BaseLock &_lock;

public:
   ScopedLock(BaseLock &lock)
      : _lock(lock)
   {
      _lock.acquire();
   }

   ~ScopedLock()
   {
      _lock.release();
   }
};


class ScopedReadLock
{
   BaseLock &_lock;

public:
   ScopedReadLock(BaseLock &lock)
      : _lock(lock)
   {
      _lock.acquire_read();
   }

   ~ScopedReadLock()
   {
      _lock.release_read();
   }
};


#endif // LOCK_H
