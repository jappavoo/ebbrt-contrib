///-*-C++-*-//////////////////////////////////////////////////////////////////
//
// Hoard: A Fast, Scalable, and Memory-Efficient Allocator
//        for Shared-Memory Multiprocessors
// Contact author: Emery Berger, http://www.cs.utexas.edu/users/emery
//
// Copyright (c) 1998-2000, The University of Texas at Austin.
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

// CONVERTED FOR USE IN EBBRT

/**
 * @file threadtest.cpp
 *
 * This program does nothing but generate a number of kernel threads
 * that allocate and free memory, with a variable
 * amount of "work" (i.e. cycle wasting) in between.
*/

#define __EBBRT__

#ifndef __EBBRT__
#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
using namespace std::chrono;

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#else
#include <stdlib.h>
#include <ebbrt/Cpu.h>
#include <ebbrt/SpinBarrier.h>
#include <ebbrt/Debug.h>
#include <ebbrt/EventManager.h>

#define MY_PRINT ebbrt::kprintf

inline uint64_t
rdtsc(void) 
{
  uint32_t a,d;

  __asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d));
  return ((uint64_t)a) | (((uint64_t)d) << 32);
}


void worker();

ebbrt::SpinBarrier *wbar = 0;
void
Work() 
{
  uint64_t start, end;
  wbar->Wait();
  start = rdtsc();
  worker();
  end = rdtsc();
  wbar->Wait();
  MY_PRINT("%d: Work Time %lld\n", ebbrt::Cpu::GetMine(), end - start);
}
#endif


int niterations = 50;	// Default number of iterations.
int nobjects = 30000;  // Default number of objects.
int nthreads = 1;	// Default number of threads.
int work = 0;		// Default number of loop iterations.
int size = 1;


class Foo {
public:
  Foo (void)
    : x (14),
      y (29)
    {}

  int x;
  int y;
};



void worker (void)
{
  int i, j;
  Foo ** a;
  a = new Foo * [nobjects / nthreads];

  for (j = 0; j < niterations; j++) {

    for (i = 0; i < (nobjects / nthreads); i ++) {
      a[i] = new Foo[size];
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
      assert (a[i]);
    }
    
    for (i = 0; i < (nobjects / nthreads); i ++) {
      delete[] a[i];
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
    }
  }

  delete [] a;
}

int hoard_threadtest (int argc, char * argv[])
{
#ifndef __EBBRT__
  thread ** threads;
#endif
  
  if (argc >= 2) {
    nthreads = atoi(argv[1]);
  }

  if (argc >= 3) {
    niterations = atoi(argv[2]);
  }

  if (argc >= 4) {
    nobjects = atoi(argv[3]);
  }

  if (argc >= 5) {
    work = atoi(argv[4]);
  }

  if (argc >= 6) {
    size = atoi(argv[5]);
  }


#ifndef __EBBRT__
  printf ("Running threadtest for %d threads, %d iterations, %d objects, %d work and %d size...\n", nthreads, niterations, nobjects, work, size);

  threads = new thread*[nthreads];

  high_resolution_clock t;
  auto start = t.now();

  int i;
  for (i = 0; i < nthreads; i++) {
    threads[i] = new thread(worker);
  }


  for (i = 0; i < nthreads; i++) {
    threads[i]->join();
  }

  auto stop = t.now();

  auto elapsed = duration_cast<duration<double>>(stop - start);
  cout << "Time elapsed = " << elapsed.count() << endl;
  delete [] threads;
#else
  if ((unsigned int)nthreads > ebbrt::Cpu::Count())  nthreads = ebbrt::Cpu::Count();

  wbar = new ebbrt::SpinBarrier(nthreads);
  auto start = rdtsc();
  MY_PRINT ("Running threadtest for %d threads, %d iterations, %d objects, %d work and %d size...\n", nthreads, niterations, nobjects, work, size);

for (size_t i = 1; i < (unsigned int)nthreads; ++i)  {
     ebbrt::event_manager->SpawnRemote(Work, i);
  }
  Work();
  MY_PRINT("Time elapsed = %lld\n",  rdtsc() - start);
#endif


  return 0;
}
