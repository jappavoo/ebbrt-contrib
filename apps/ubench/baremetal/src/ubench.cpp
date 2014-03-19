#include <ebbrt/Debug.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <argv.h>

#include <unistd.h>
#include <cctype>

#include "../../src/Unix.h"

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
  ebbrt::kprintf("%s: " #CTRCALLPREFIX "inc(): %"	\
		 PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
		 ACTION_CNT, (end - start));		\
  							\
  start = rdtsc();				\
  for (int i=0; i<ACTION_CNT; i++) {		\
    CTRCALLPREFIX.dec();			\
  }						\
  end = rdtsc();				\
  ebbrt::kprintf("%s: " #CTRCALLPREFIX "dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  ACTION_CNT, (end - start));		\
						\
  start = rdtsc();				\
  for (int i=0; i<ACTION_CNT; i++) {		\
    rc += CTRCALLPREFIX.val();			\
  }						\
  end = rdtsc();				\
  assert(rc==0);				\
  ebbrt::kprintf("%s: " #CTRCALLPREFIX "val(): %"	\
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

  ebbrt::kprintf("%s: new Counter: %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__,1, (end - start));

  start = rdtsc();
  for (int i=0; i<ACTION_CNT; i++) {
    heapCtr->inc();
  }
  end = rdtsc();
  ebbrt::kprintf("%s: heapCtr->inc(): %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, ACTION_CNT, (end - start));

  start = rdtsc();
  for (int i=0; i<ACTION_CNT; i++) {
    heapCtr->dec();
  }
  end = rdtsc();
  ebbrt::kprintf("%s: heapCtr->dec(): %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, ACTION_CNT, (end - start));

  start = rdtsc();
  for (int i=0; i<ACTION_CNT; i++) {
    rc += heapCtr->val();
  }
  end = rdtsc();
  assert(rc==0);

  ebbrt::kprintf("%s: heapCtr->val(): %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, ACTION_CNT, (end - start));

  start = rdtsc();
  delete heapCtr;
  end = rdtsc();
  ebbrt::kprintf("%s: delete heapCtr: %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__, 1, (end - start));
}

int processargs(int argc, char **argv) 
{
  int aflag = 0;
  int Cflag = 0;
  int eflag = 0;
  int Eflag = 0;
  int index;
  int c;
  
  opterr = 0;

  while ((c = getopt (argc, argv, "haCeE")) != -1) {
    switch (c)
      {
      case 'h':
	kprintf("%s: [-h] [-a] [-C] [-e] [-E]\n"
		" EbbRT micro benchmarks\n"
		" -h : help\n"
		" -a : run all tests\n"
		" -C : run basic C++ tests\n"
		" -e : run basic Ebb tests\n"
		" -E : run Event tests\n", argv[0]);
	return -1;
      case 'a':
	aflag = 1;
	break;
      case 'C':
	Cflag = 1;
	break;
      case 'e':
	eflag = 1;
	break;
      case 'E':
	Eflag = 1;
	break;
      case '?':
	if (isprint (optopt)) {
	  ebbrt::kprintf("Unknown option `-%c'.\n", optopt);
	} else {
	  ebbrt::kprintf("Unknown option character `\\x%x'.\n",
			optopt);
	}
	return 1;
      default:
	return -1;
      }
  }  
  ebbrt::kprintf ("aflag = %d, Cflag = %d, eflag = %d, Eflag=%d\n",
		  aflag, Cflag, eflag, Eflag);
  
  for (index = optind; index < argc; index++)
    ebbrt::kprintf ("Non-option argument %s\n", argv[index]);
  return 0;
}

void AppMain()
{
  int argc;
  char **argv;

  UNIX::Init();

  char *cmdline = NULL;

  ebbrt::kprintf("ubench: BEGIN: %s\n", cmdline);

  if (cmdline) {
    string_to_argv(cmdline, &argc, &argv);
    for (int i=0; i<argc; i++) {
      ebbrt::kprintf("argc[%d]=%s\n", i, argv[i]);
    }
    if (processargs(argc, argv) == -1) return;
  }

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
  ebbrt::kprintf("ubench: END\n");
  return;
}

