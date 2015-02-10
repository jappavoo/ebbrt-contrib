#ifndef __EBBRT_BM__
// HOSTED
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <cctype>
#include <stdio.h>

#include <boost/filesystem.hpp>
#include <boost/container/static_vector.hpp>
#include <ebbrt/Context.h>
#include <ebbrt/ContextActivation.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/NodeAllocator.h>

#include "Cpu.h"

#define MY_PRINT printf

#else

// BAREMETAL
#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <argv.h>
#include <unistd.h>
#include <strings.h>
#include <cctype>

#define MY_PRINT ebbrt::kprintf

void  bootimgargs_test(struct Arguments *);
void  stdin_test(struct Arguments *);
void  filein_test(struct Arguments *);
#endif

#include <ebbrt/SpinBarrier.h>
 
#include "Unix.h"
#include "ubenchCommon.h"


#define __FUNC__ __FUNCTION__
#define __PFUNC__ __PRETTY_FUNCTION__

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



#define CtrWork(CTR,CNT)			\
  {						\
  int rc = 0;					\
  uint64_t start, end;				\
						\
  start = rdtsc();				\
  for (int i=0; i<cnt; i++) {			\
    CTR.inc();					\
  }						\
  end = rdtsc();				\
  MY_PRINT("%s: " #CTR ".inc(): %"	\
		 PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
		 CNT, (end - start));		\
  							\
  start = rdtsc();				\
  for (int i=0; i<CNT; i++) {		\
    CTR.dec();			\
  }						\
  end = rdtsc();				\
  MY_PRINT("%s: " #CTR ".dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  CNT, (end - start));		\
						\
  start = rdtsc();				\
  for (int i=0; i<CNT; i++) {		\
    rc += CTR.val();			\
  }						\
  end = rdtsc();				\
  assert(rc==0);				\
  MY_PRINT("%s: " #CTR ".val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  CNT, (end - start));		\
  }

#define CtrRefWork(CTR,CNT)			\
  {						\
  int rc = 0;					\
  uint64_t start, end;				\
						\
  start = rdtsc();				\
  for (int i=0; i<CNT; i++) {		\
    CTR->inc();			\
  }						\
  end = rdtsc();				\
  MY_PRINT("%s: " #CTR "->inc(): %"	\
		 PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
		 CNT, (end - start));		\
  							\
  start = rdtsc();				\
  for (int i=0; i<CNT; i++) {		\
    CTR->dec();			\
  }						\
  end = rdtsc();				\
  MY_PRINT("%s: " #CTR "->dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  CNT, (end - start));		\
						\
  start = rdtsc();				\
  for (int i=0; i<CNT; i++) {		\
    rc += CTR->val();			\
  }						\
  end = rdtsc();				\
  assert(rc==0);				\
  MY_PRINT("%s: " #CTR "->val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	  CNT, (end - start));		\
  }


void
GlobalCounterTest(int cnt) 
{
  CtrWork(GlobalCtr,cnt)
}

void
StackCounterTest(int cnt) 
{
  Counter stackCtr;
  CtrWork(stackCtr,cnt)
}

void
HeapCounterTest(int cnt)
{
  uint64_t start, end;

  start = rdtsc();
  Counter *heapCtr = new Counter;
  end = rdtsc();
  assert(heapCtr!=NULL);

  MY_PRINT("%s: new Counter: %" PRId32 " %" PRIu64 "\n", 
	  __PFUNC__,1, (end - start));

  CtrRefWork(heapCtr,cnt);
}

void 
cpp_test(struct Arguments *args)
{
  if (!args->tests.cpp) return;
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  
  MY_PRINT("_UBENCH_CPP_TEST_: START\n");
  // Base line C++ method dispatch numbers 
  for (int i=0; i<rcnt; i++) {
    GlobalCounterTest(acnt);
    StackCounterTest(acnt);
    HeapCounterTest(acnt);
  }
  MY_PRINT("_UBENCH_CPP_TEST_: END\n");
}

void event_test(struct Arguments *args)
{
 // Base line Event spawn numbers
  if (!args->tests.event)  return;

  MY_PRINT("_UBENCH_CPP_TEST_: START\n");
  MY_PRINT("_UBENCH_CPP_TEST_: END\n");
}

void ebb_test(struct Arguments *args) 
{
  if (!args->tests.ebb) return;

  MY_PRINT("_UBENCH_EBB_TEST_: START\n");
  MY_PRINT("_UBENCH_EBB_TEST_: END\n");

}

extern void hoard_threadtest(int,char **);

void malloc_test(struct Arguments *args)
{
  if (!args->tests.malloc) return;

  MY_PRINT("_UBENCH_MALLOC_TEST_: START\n");

  hoard_threadtest(UNIX::cmd_line_args->argc()-optind,
		   &(UNIX::cmd_line_args->data()[optind]));

  MY_PRINT("_UBENCH_MALLOC_TEST_: END\n");
}

void cmdline_test(struct Arguments *args) 
{
#ifdef _UBENCH_CMD_LINE_ARGS_TEST_
if (!args->tests.cmdline) return;
  MY_PRINT("_UBENCH_CMDLINE_TEST_: Start\n");
  for (int i=0; i<UNIX::cmd_line_args->argc(); i++) {
    MY_PRINT("UNIX::cmd_line_args->argv(%d)=%s\n",i,
		   UNIX::cmd_line_args->argv(i));
  }
  for (int i=optind; i < UNIX::cmd_line_args->argc(); i++) {
    MY_PRINT("non option arguments %s\n", UNIX::cmd_line_args->argv(i));
  }
  MY_PRINT("_UBENCH_CMDLINE_TEST_: END\n");
#endif
}

void env_test(struct Arguments *args) {
#ifdef _UBENCH_ENVIRONMENT_TEST_
if (!args->tests.env)  return;
  MY_PRINT("_UBENCH_ENVIRONMENT_TEST_: Start\n");
  for (int i=0; UNIX::environment->environ()[i]!=NULL; i++) {
    MY_PRINT("%d: ev=%s\n", i, UNIX::environment->environ()[i]);
  }
  MY_PRINT("getenv(\"hello\")=%s\n", UNIX::environment->getenv("hello"));
  MY_PRINT("_UBENCH_ENVIRONMENT_TEST_: End\n");
#endif
}

void
spawn_test(struct Arguments *args)
{
  static ebbrt::SpinBarrier bar(ebbrt::Cpu::Count());
  static std::atomic<size_t> scnt;
  size_t n = ebbrt::Cpu::Count();

  if (!args->tests.spawn)  return;
  MY_PRINT("_UBENCH_SPAWN_TEST_: Start\n");

  for (size_t i=0; i<n; i++) {
#ifndef __EBBRT_BM__
    ebbrt::Cpu *cpu = ebbrt::Cpu::GetByIndex(i);
    ebbrt::Context *ctxt = cpu->get_context();
    ebbrt::event_manager->Spawn([n]() {
      MY_PRINT("HELLO: spawned: %zd %zd: %s:%d\n", size_t(ebbrt::Cpu::GetMine()), 
	     size_t(*ebbrt::active_context),
	     __FILE__, __LINE__);
      bar.Wait();
      MY_PRINT("GOODBYE: spawned: %zd %zd: %s:%d\n", size_t(ebbrt::Cpu::GetMine()), 
	     size_t(*ebbrt::active_context),
	     __FILE__, __LINE__);
      scnt++;
      if (scnt==n)   MY_PRINT("_UBENCH_SPAWN_TEST_: END\n");
      }, ctxt);
#else
    ebbrt::event_manager->SpawnRemote([n]() {
      MY_PRINT("HELLO: spawned: %d\n", size_t(ebbrt::Cpu::GetMine()));
      bar.Wait();
      MY_PRINT("GOODBYE: spawned:%d\n", size_t(ebbrt::Cpu::GetMine()));
      scnt++;
      if (scnt==n)   MY_PRINT("_UBENCH_SPAWN_TEST_: END\n");
      }, i);
#endif
  }
}

int 
process_args(int argc, char **argv, struct Arguments *args) 
{
  int c;
  
  bzero(args, sizeof(struct Arguments));

  args->repeatCnt = REPEAT_CNT;
  args->actionCnt = ACTION_CNT;

  opterr = 0;

  while ((c = getopt (argc, argv, "hBCEIF:cevms")) != -1) {
    switch (c)
      { 
      case 'h':
	MY_PRINT("%s: [-h] [-B] [-C] [-E] [-I] [-F file] [-c] [-e] [-v] [-m] [-s]\n"
		" EbbRT micro benchmarks\n"
		" -h : help\n"
                " -B : run tests on Backend\n"
                " -C : Command line test\n"
                " -E : Environment test\n"
		" -I : Standard Input test\n"
		" -F file : File Input test\n"
		" -c : run basic C++ tests\n"
		" -e : run basic Ebb tests\n"
		" -v : run Event tests\n"
		" -m : malloc tests\n"
   	        " -s : spawn test\n", argv[0]);
	return -1;
      case 'B':
	args->backend = 1;
	break;
      case 'C':
	args->tests.cmdline = 1;
	break;
      case 'E':
	args->tests.env = 1;
	break;
      case 'F':
	args->tests.filein = 1;
	break;
      case 'I':
	args->tests.standardin = 1;
	break;
      case 'm':
	args->tests.malloc = 1;
	break;
      case 'c':
	args->tests.cpp = 1;
	break;
      case 'e':
	args->tests.ebb = 1;
	break;
      case 'v':
	args->tests.event = 1;
	break;
      case 's':
	args->tests.spawn = 1;
	break;
      case '?':
	if (isprint (optopt)) {
	  MY_PRINT("Unknown option `-%c'.\n", optopt);
	} else {
	  MY_PRINT("Unknown option character `\\x%x'.\n",
			optopt);
	}
	return 1;
      default:
	return -1;
      }
  }  
  return 0;
}


void AppMain()
{
  struct Arguments args;

#ifdef __EBBRT_BM__
  UNIX::Init();
#else 
  UNIX::Init(margs.argc, (const char **)margs.argv);
#endif

  process_args(UNIX::cmd_line_args->argc(), UNIX::cmd_line_args->data(), &args);

  cmdline_test(&args);
  env_test(&args);

#ifdef __EBBRT_BM__
  bootimgargs_test(&args);
  stdin_test(&args);
  filein_test(&args);
#endif

#ifdef _UBENCH_BENCHMARKS_
  MY_PRINT("_UBENCH_BENCHMARKS_: Start\n");
  cpp_test(&args);
  ebb_test(&args);
  event_test(&args);
  malloc_test(&args);
  spawn_test(&args);
  MY_PRINT("_UBENCH_BENCHMARKS_: End\n");
#endif

#ifndef __EBBRT_BM__
  //  frontendio_test();
  if (args.backend)  {
    auto bindir = 
      boost::filesystem::system_complete(UNIX::cmd_line_args->argv(0)).parent_path() /
      "bm/ubench.elf32";
    ebbrt::node_allocator->AllocateNode(bindir.string());
  }
#endif

  return;
}
