#include <ebbrt/Debug.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#define __FUNC__ __FUNCTION__
#define __PFUNC__ __PRETTY_FUNCTION__

#define ACTION_CNT 100000
#define REPEAT_CNT 10

inline uint64_t
rdtsc(void) 
{
  uint32_t a,d;

  __asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d));
  return ((uint64_t)a) | (((uint64_t)d) << 32);
}

inline uint64_t
rdpmctsc(void)
{
  uint32_t a,d;
  uint32_t c=0;

  __asm__ __volatile__ ("rdpmc" : "=a" (a), "=d" (d) : "c" (c));

  return ((uint64_t)a) | (((uint64_t)d)<<32);
}

using namespace ebbrt;

class Counter {
  int _val;
public:
  Counter() : _val(0) {}
  void inc() { _val++; }
  void dec() { _val--; }
  int  val() { return _val; }
};

Counter GlobalCtr;

#define CtrWork(CTRCALLPREFIX)			\
  {						\
  int rc = 0;					\
  uint64_t start, end;				\
						\
  start = rdtsc();				\
  for (int i=0; i<ACTION_CNT; i++) {		\
    CTRCALLPREFIX.inc();			\
  }						\
  end = rdtsc();				\
  kprintf("%s: " #CTRCALLPREFIX "inc(): %"	\
	  PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
	  ACTION_CNT, (end - start));		\
						\
  start = rdtsc();				\
  for (int i=0; i<ACTION_CNT; i++) {		\
    CTRCALLPREFIX.dec();			\
  }						\
  end = rdtsc();				\
  kprintf("%s: " #CTRCALLPREFIX "dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  ACTION_CNT, (end - start));		\
						\
  start = rdtsc();				\
  for (int i=0; i<ACTION_CNT; i++) {		\
    rc += CTRCALLPREFIX.val();			\
  }						\
  end = rdtsc();				\
  assert(rc==0);				\
  kprintf("%s: " #CTRCALLPREFIX "val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  ACTION_CNT, (end - start));		\
  }


void
GlobalCounterTest() 
{
  CtrWork(GlobalCtr)
}

void
StackCounterTest() 
{
  Counter stackCtr;
  CtrWork(stackCtr)
}

void
HeapCounterTest()
{
  int rc = 0;
  uint64_t start, end;

  start = rdtsc();
  Counter *heapCtr = new Counter;
  end = rdtsc();
  assert(heapCtr!=NULL);

  kprintf("%s: new Counter: %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__,1, (end - start));

  start = rdtsc();
  for (int i=0; i<ACTION_CNT; i++) {
    heapCtr->inc();
  }
  end = rdtsc();
  kprintf("%s: heapCtr->inc(): %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, ACTION_CNT, (end - start));

  start = rdtsc();
  for (int i=0; i<ACTION_CNT; i++) {
    heapCtr->dec();
  }
  end = rdtsc();
  kprintf("%s: heapCtr->dec(): %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, ACTION_CNT, (end - start));

  start = rdtsc();
  for (int i=0; i<ACTION_CNT; i++) {
    rc += heapCtr->val();
  }
  end = rdtsc();
  assert(rc==0);

  kprintf("%s: heapCtr->val(): %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, ACTION_CNT, (end - start));

  start = rdtsc();
  delete heapCtr;
  end = rdtsc();
  kprintf("%s: delete heapCtr: %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, 1, (end - start));
}

void appmain(char *cmdline)
{
  kprintf("ubench: BEGIN: %p\n", cmdline);
  
  // Base line C++ method dispatch numbers 
  for (int i=0; i<REPEAT_CNT; i++) {
    GlobalCounterTest();
    StackCounterTest();
    HeapCounterTest();
  }

  // Base line C++ virtual method dispatch numbers

  // Ebb dispatch numbers


  // Base line Event spawn numbers


  // Multicore Numbers
  kprintf("ubench: END\n");
}

