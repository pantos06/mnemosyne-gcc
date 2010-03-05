///-*-C++-*-//////////////////////////////////////////////////////////////////
//
// The Hoard Multiprocessor Memory Allocator
// www.hoard.org
//
// Author: Emery Berger, http://www.cs.utexas.edu/users/emery
//
// Copyright (c) 1998-2001, The University of Texas at Austin.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as
// published by the Free Software Foundation, http://www.fsf.org.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
//////////////////////////////////////////////////////////////////////////////


#include "arch-specific.h"

// How many iterations we spin waiting for a lock.
enum { SPIN_LIMIT = 100 };

// The values of a user-level lock.
enum { UNLOCKED = 0, LOCKED = 1 };

extern "C" {

#if USE_SPROC
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ulocks.h>
#endif

void hoardCreateThread (hoardThreadType& t,
			void *(*function) (void *),
			void * arg)
{
#if USE_SPROC
  typedef void (*sprocFunction) (void *);
  t = sproc ((sprocFunction) function, PR_SADDR, arg);
#else
  pthread_attr_t attr;
  pthread_attr_init (&attr);
#if defined(_AIX)
  // Bound (kernel-level) threads.
  pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
#endif
  pthread_create (&t, &attr, function, arg);
#endif
}

void hoardJoinThread (hoardThreadType& t)
{
#if USE_SPROC
  waitpid (t, 0, 0);
#else
  pthread_join (t, NULL);
#endif
}

#if 0 // DISABLED!
#if defined(__linux)
// This extern declaration is required for some versions of Linux.
extern "C" void pthread_setconcurrency (int n);
#endif
#endif

void hoardSetConcurrency (int n)
{
#if USE_SPROC
  usconfig (CONF_INITUSERS, n);
#elif defined(__SVR4) // Solaris
  thr_setconcurrency (n);
#else
  pthread_setconcurrency (n);
#endif
}


#if defined(__SVR4) // Solaris

// Solaris's two-level threads model gives us an edge here;
// we can hash on the LWP's id. This helps us in two ways:
// (1) there are likely to be far fewer LWP's than threads,
// (2) if there's a one-to-one correspondence between LWP's
//     and the number of processors (usually the case), then
//     the number of heaps used will be the same as the number
//     of processors (the optimal case).
// Here we rely on an undocumented call in libthread.so, which
// turns out to be MUCH cheaper than the documented _lwp_self(). Go figure.

extern "C" unsigned int lwp_self(void);
#endif

int hoardGetThreadID (void) {
#if USE_SPROC
  // This hairiness has the same effect as calling getpid(),
  // but it's MUCH faster since it avoids making a system call
  // and just accesses the sproc-local data directly.
  int pid = (int) PRDA->sys_prda.prda_sys.t_pid;
  return pid;
#else
#if defined(__linux)
  // Consecutive thread id's in Linux are 1024 apart.
  return (int) pthread_self() / 1024;
#endif
#if defined(__AIX)
  // Consecutive thread id's in AIX are 257 apart.
  return (int) pthread_self() / 257;
#endif
#if defined(__SVR4)
  return (int) lwp_self();
#endif
  return (int) pthread_self();
#endif
}


// Here's our own lock implementation (spin then yield). This is much
// cheaper than the ordinary mutex, at least on Linux and Solaris.

#if USER_LOCKS

#include <sched.h>

#if defined(__sgi)
#include <mutex.h>
#endif


// Atomically:
//   retval = *oldval;
//   *oldval = newval;
//   return retval;

#if defined(sparc) && !defined(__GNUC__)
extern "C" unsigned long InterlockedExchange (unsigned long * oldval,
					      unsigned long newval);
#else
unsigned long InterlockedExchange (unsigned long * oldval,
						 unsigned long newval)
{
#if defined(sparc)
  asm volatile ("swap [%1],%0"
		:"=r" (newval)
		:"r" (oldval), "0" (newval)
		: "memory");

#endif
#if defined(i386)
  asm volatile ("xchgl %0, %1"
		: "=r" (newval)
		: "m" (*oldval), "0" (newval)
		: "memory");
#endif
#if defined(__sgi)
  newval = test_and_set (oldval, newval);
#endif
#if defined(ppc)
  int ret;
  asm volatile ("sync;"
		"0:    lwarx %0,0,%1 ;"
		"      xor. %0,%3,%0;"
		"      bne 1f;"
		"      stwcx. %2,0,%1;"
		"      bne- 0b;"
		"1:    sync"
	: "=&r"(ret)
	: "r"(oldval), "r"(newval), "r"(*oldval)
	: "cr0", "memory");
#endif
#if !(defined(sparc) || defined(i386) || defined(__sgi) || defined(ppc))
#error "Hoard does not include support for user-level locks for this platform."
#endif
  return newval;
}
#endif

unsigned long hoardInterlockedExchange (unsigned long * oldval,
					unsigned long newval)
{
  return InterlockedExchange (oldval, newval);
}

void hoardLockInit (hoardLockType& mutex) {
  InterlockedExchange (&mutex, UNLOCKED);
}

#include <stdio.h>

void hoardLock (hoardLockType& mutex) {
  int spincount = 0;
  while (InterlockedExchange (&mutex, LOCKED) != UNLOCKED) {
    spincount++;
    if (spincount > 100) {
      hoardYield();
      spincount = 0;
    }
  }
}

void hoardUnlock (hoardLockType& mutex) {
	mutex = UNLOCKED;
//  InterlockedExchange (&mutex, UNLOCKED);
}

#else

// use non-user-level locks. 

#endif // USER_LOCKS


#if defined(__SVR4)
#include <thread.h>
#endif

void hoardYield (void)
{
#if defined(__SVR4)
  thr_yield();
#else
  sched_yield();
#endif
}


extern "C" void * dlmalloc (size_t);
extern "C" void dlfree (void *);


#if USER_LOCKS
static hoardLockType getMemoryLock = 0;
#else
static hoardLockType getMemoryLock = PTHREAD_MUTEX_INITIALIZER;
#endif

#include <stdio.h>

void * hoardGetMemory (long size) {
  hoardLock (getMemoryLock);
  void * ptr = dlmalloc (size);
  hoardUnlock (getMemoryLock);
  return ptr;
}


void hoardFreeMemory (void * ptr)
{
  hoardLock (getMemoryLock);
  dlfree (ptr);
  hoardUnlock (getMemoryLock);
}


int hoardGetPageSize (void)
{
  return (int) sysconf(_SC_PAGESIZE);
}


#if defined(linux)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#endif

#if defined(__sgi)
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#endif

int hoardGetNumProcessors (void)
{
#if !(defined(linux))
#if defined(__sgi)
  static int np = (int) sysmp(MP_NAPROCS);
  return np;
#else
  static int np = (int) sysconf(_SC_NPROCESSORS_ONLN);
  return np;
#endif
#else
  static int numProcessors = 0;

  if (numProcessors == 0) {
    // Ugly workaround.  Linux's sysconf indirectly calls malloc() (at
    // least on multiprocessors).  So we just read the info from the
    // proc file ourselves and count the occurrences of the word
    // "processor".
    
    // We only parse the first 32K of the CPU file.  By my estimates,
    // that should be more than enough for at least 64 processors.
    enum { MAX_PROCFILE_SIZE = 32768 };
    char line[MAX_PROCFILE_SIZE];
    int fd = open ("/proc/cpuinfo", O_RDONLY);
    //    assert (fd);
    read(fd, line, MAX_PROCFILE_SIZE);
    char * str = line;
    while (str) {
      str = strstr(str, "processor");
      if (str) {
	numProcessors++;
	str++;
      }
    }
    close (fd);
    //    assert (numProcessors > 0);
  }
  return numProcessors;
#endif
}

}
