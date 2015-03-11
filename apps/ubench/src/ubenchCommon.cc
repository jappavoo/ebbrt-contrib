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

#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>

#include "Cpu.h"

#define MY_PRINT printf

typedef std::chrono::high_resolution_clock myclock;

#else

// BAREMETAL
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <argv.h>
#include <unistd.h>
#include <strings.h>
#include <cctype>

#include <ebbrt/Clock.h>
#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>

#define MY_PRINT ebbrt::kprintf

typedef ebbrt::clock::Wall myclock;

void  bootimgargs_test(struct Arguments *);
#endif

#include <ebbrt/SpinBarrier.h>
 
#include "Unix.h"
#include "ubenchCommon.h"

#define __FUNC__ __FUNCTION__
#define __PFUNC__ __PRETTY_FUNCTION__

typedef std::chrono::time_point<myclock> tp;
tp now() { return myclock::now(); }

#if 0
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
#endif

uint64_t
nsdiff(tp start, tp end)
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>
  (end-start).count();
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

class VirtualCounter {
  int _val;
public:
  VirtualCounter() : _val(0) {}
  virtual void inc() { _val++; }
  virtual void dec() { _val--; }
  virtual int val() { return _val; }
};

Counter GlobalCtr;
VirtualCounter VirtualGlobalCtr;


#define CtrWork(CTR,CNT)			\
  {						\
  int rc = 0;					\
  tp start, end;				\
						\
  start = now();				\
  for (int i=0; i<cnt; i++) {			\
    CTR.inc();					\
  }						\
  end = now();				\
  MY_PRINT("RES: %s: " #CTR ".inc(): %"	\
		 PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
	     CNT, nsdiff(start,end));				\
  							\
  start = now();				\
  for (int i=0; i<CNT; i++) {		\
    CTR.dec();			\
  }						\
  end = now();				\
  MY_PRINT("RES: %s: " #CTR ".dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	     CNT, nsdiff(start,end));	\
						\
  start = now();				\
  for (int i=0; i<CNT; i++) {		\
    rc += CTR.val();			\
  }						\
  end = now();				\
  assert(rc==0);				\
  MY_PRINT("RES: %s: " #CTR ".val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	     CNT, nsdiff(start,end));	\
  }

#define CtrRefWork(CTR,CNT)			\
  {						\
  int rc = 0;					\
  tp start, end;				\
						\
  start = now();				\
  for (int i=0; i<CNT; i++) {		\
    CTR->inc();			\
  }						\
  end = now();				\
  MY_PRINT("RES: %s: " #CTR "->inc(): %"	\
		 PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
	   CNT, nsdiff(start,end));			\
  							\
  start = now();				\
  for (int i=0; i<CNT; i++) {		\
    CTR->dec();			\
  }						\
  end = now();				\
  MY_PRINT("RES: %s: " #CTR "->dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	   CNT, nsdiff(start,end));		\
						\
  start = now();				\
  for (int i=0; i<CNT; i++) {		\
    rc += CTR->val();			\
  }						\
  end = now();				\
  assert(rc==0);				\
  MY_PRINT("RES: %s: " #CTR "->val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	   CNT, nsdiff(start,end));	\
  }


void
GlobalCounterTest(int cnt) 
{
  CtrWork(GlobalCtr,cnt)
}

void
VirtualGlobalCounterTest(int cnt) 
{
  CtrWork(VirtualGlobalCtr,cnt)
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
  tp start, end;

  start = now();
  Counter *heapCtr = new Counter;
  end = now();
  assert(heapCtr!=NULL);

  MY_PRINT("RES: %s: new Counter: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(heapCtr,cnt);
}

void
VirtualHeapCounterTest(int cnt)
{  
  tp start, end;

  start = now();
  VirtualCounter *heapCtr = new VirtualCounter;
  end = now();
  assert(heapCtr!=NULL);

  MY_PRINT("RES: %s: new VirtualCounter: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

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
    VirtualGlobalCounterTest(acnt);
    VirtualHeapCounterTest(acnt);
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
}

void env_test(struct Arguments *args) {
if (!args->tests.env)  return;
  MY_PRINT("_UBENCH_ENVIRONMENT_TEST_: Start\n");
  for (int i=0; UNIX::environment->environ()[i]!=NULL; i++) {
    MY_PRINT("%d: ev=%s\n", i, UNIX::environment->environ()[i]);
  }
  MY_PRINT("getenv(\"hello\")=%s\n", UNIX::environment->getenv("hello"));
  MY_PRINT("_UBENCH_ENVIRONMENT_TEST_: End\n");
}

void
spawn_test(struct Arguments *args)
{
  static ebbrt::SpinBarrier bar(ebbrt::Cpu::Count());
  size_t n = ebbrt::Cpu::Count();
  size_t theCpu=ebbrt::Cpu::GetMine();
  std::atomic<size_t> count(0);
  if (!args->tests.spawn)  return;
  MY_PRINT("_UBENCH_SPAWN_TEST_: Start\n");

  EventManager::EventContext context;
  for (size_t i=0; i<n; i++) {
#ifndef __EBBRT_BM__
    ebbrt::Cpu *cpu = ebbrt::Cpu::GetByIndex(i);
    ebbrt::Context *ctxt = cpu->get_context();
    ebbrt::event_manager->Spawn([n,&context,theCpu,&count]() {
      MY_PRINT("HELLO: spawned: %zd %zd: %s:%d\n", size_t(ebbrt::Cpu::GetMine()), 
	     size_t(*ebbrt::active_context),
	     __FILE__, __LINE__);
      bar.Wait();
      MY_PRINT("GOODBYE: spawned: %zd %zd: %s:%d\n", size_t(ebbrt::Cpu::GetMine()), 
	     size_t(*ebbrt::active_context),
	     __FILE__, __LINE__);
      bar.Wait();
      count++;
      while (count < n);
      if (ebbrt::Cpu::GetMine()==theCpu) {
	ebbrt::event_manager->ActivateContext(std::move(context));
      }
      }, ctxt);
#else
    ebbrt::event_manager->SpawnRemote([n,&context,theCpu,&count]() {
      MY_PRINT("HELLO: spawned: %d\n", size_t(ebbrt::Cpu::GetMine()));
      bar.Wait();
      MY_PRINT("GOODBYE: spawned:%d\n", size_t(ebbrt::Cpu::GetMine()));
      bar.Wait();
      count++;
      while (count < n);
      if (ebbrt::Cpu::GetMine()==theCpu) {
	ebbrt::event_manager->ActivateContext(std::move(context));
      }
      }, i);
#endif
  }
      ebbrt::event_manager->SaveContext(context);
      MY_PRINT("Spawned: %d  Finished: %d\n", (int)n, (int)count);
      assert(n==count);
      MY_PRINT("_UBENCH_SPAWN_TEST_: END\n");
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
	args->tests.filein = optarg;
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
	if (optopt == 'F') {
	  MY_PRINT("ERROR: -F requires file path to be specified.\n");
	} else if (isprint (optopt)) {
	  MY_PRINT("Unknown option `-%c'.\n", optopt);
	} else {
	  MY_PRINT("Unknown option character `\\x%x'.\n",
			optopt);
	}
	return 0;
      default:
	return -1;
      }
  }  
  return 1;
}

void 
standardin_test(struct Arguments *args)
{
  if (!args->tests.standardin)  return;
  MY_PRINT("_UBENCH_STDIN_TEST_: Start\n");

  ebbrt::EventManager::EventContext context;

  MY_PRINT("Enter lines of characters ('.' to terminte):\n");

  UNIX::sin->async_read_start
    ([&context]
     (std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
      if (!buf) {
	assert(avail==0);
	MY_PRINT("Stream EOF\n");
	UNIX::sin->async_read_stop();
	ebbrt::event_manager->ActivateContext(std::move(context));
	return; 
      }
      for (auto& b : *buf) {
	if (b.Length()>0) {
	  size_t n = ebbrt::console::Write((const char *)b.Data(), b.Length()); 
	  if (n<=0) throw std::runtime_error("write to stdout failed");
	  if (!UNIX::sin->isFile() && b.Data()[0] == '.') {
	    MY_PRINT("End of Data: .\n");
	    UNIX::sin->async_read_stop();
	    ebbrt::event_manager->ActivateContext(std::move(context));
	    break;
	  }
	}
      }
    });
  ebbrt::event_manager->SaveContext(context);
  MY_PRINT("_UBENCH_STDIN_TEST_: End\n");
}

void 
filein_test(struct Arguments *args)
{
  if (!args->tests.filein) return;
  MY_PRINT("_UBENCH_FILEIN_TEST_: Start\n");

  ebbrt::EventManager::EventContext context;

  MY_PRINT("open and dump %s as a  stream", args->tests.filein); 
  auto fistream = UNIX::root_fs->openInputStream(args->tests.filein);
  fistream.Then([&context](ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>> fis) {
      ebbrt::EbbRef<UNIX::InputStream> is = fis.Get();		  
      is->async_read_start([&context,is](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
	  if (!buf) {
	    assert(avail==0);
	    MY_PRINT("Stream EOF\n"); 
	    is->async_read_stop();
	    ebbrt::event_manager->ActivateContext(std::move(context));
	    return; 
	  }
	  for (auto &b: *buf)  {
	    size_t n = ebbrt::console::Write((const char *)b.Data(), b.Length()); 
	    if (n<=0) throw std::runtime_error("write to stdout failed");
	  }
	});
    }); 

  ebbrt::event_manager->SaveContext(context);
  MY_PRINT("_UBENCH_FILEIN_TEST_: END\n");
}


void AppMain()
{
  struct Arguments args;

#ifdef __EBBRT_BM__
  UNIX::Init();
#else 
  UNIX::Init(margs.argc, (const char **)margs.argv);
#endif

  if (!process_args(UNIX::cmd_line_args->argc(),
		    UNIX::cmd_line_args->data(), &args)) {
    MY_PRINT("ERROR in processing arguments\n");
    goto DONE;
  }

#ifndef __EBBRT_BM__
  if (args.backend)  {
    auto bindir = 
      boost::filesystem::system_complete(UNIX::cmd_line_args->argv(0)).parent_path() /
      "bm/ubench.elf32";
    ebbrt::node_allocator->AllocateNode(bindir.string());
    return;
  } 
#endif

  MY_PRINT("_UBENCH_BENCHMARKS_: Start\n");  

  cmdline_test(&args);
  env_test(&args);
  standardin_test(&args);
  filein_test(&args);

#ifdef __EBBRT_BM__
  bootimgargs_test(&args);
#endif

  cpp_test(&args);
  ebb_test(&args);
  event_test(&args);
  malloc_test(&args);
  spawn_test(&args);

  MY_PRINT("_UBENCH_BENCHMARKS_: End\n");

 DONE:
#ifndef __EBBRT_BM__
  ebbrt::Cpu::Exit(0);
#else 
  UNIX::root_fs->processExit(1);
#endif
  
  return;
}
