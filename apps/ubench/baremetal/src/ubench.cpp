#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>
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

int process_img_args(int argc, char **argv) 
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


//#define _UBENCH_CMD_LINE_ARGS_TEST_
//#define _UBENCH_ENVIRONMENT_TEST_
//#define _UBENCH_STANDARD_IN_TEST_
#define _UBENCH_FILE_IN_TEST_
//#define _UBENCH_BOOT_IMG_CMD_LINE_TEST_
//#define   _UBENCH_BENCHMARKS_

void AppMain()
{

  UNIX::Init();

#ifdef _UBENCH_CMD_LINE_ARGS_TEST_
  for (int i=0; i<UNIX::cmd_line_args->argc(); i++) {
    ebbrt::kprintf("UNIX::cmd_line_args->argv(%d)=%s\n",i,
		   UNIX::cmd_line_args->argv(i));
  }
#endif

#ifdef _UBENCH_ENVIRONMENT_TEST_
  //  asm volatile ("jmp .");
  ebbrt::kprintf("_UBENCH_ENVIRONMENT_TEST_: Start\n");
  for (int i=0; UNIX::environment->environ()[i]!=NULL; i++) {
    ebbrt::kprintf("%d: ev=%s\n", i, UNIX::environment->environ()[i]);
  }
  ebbrt::kprintf("getenv(\"hello\")=%s\n", UNIX::environment->getenv("hello"));
ebbrt::kprintf("_UBENCH_ENVIRONMENT_TEST_: End\n");
#endif

#ifdef _UBENCH_STANDARD_IN_TEST_
  ebbrt::kprintf("_UBENCH_STANDARD_IN_TEST_: Start\n");
  UNIX::sin->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
	do {
	  ebbrt::console::Write((const char *)buf->Data(), buf->Length());
	  if (!UNIX::sin->isFile() && (buf->Data()[0] == '.')) {
	    UNIX::sin->async_read_stop();
	    break;
	  }
	} while(buf->Pop()!=nullptr);
    });
  ebbrt::kprintf("_UBENCH_STANDARD_IN_TEST_: End\n");
#endif


#ifdef _UBENCH_FILE_IN_TEST_
  //asm volatile ("jmp .");
ebbrt::kprintf("_UBENCH_FILE_IN_TEST_: Start\n");
ebbrt::kprintf("0:%s\n", __PRETTY_FUNCTION__);
  auto fistream = UNIX::root_fs->openInputStream("/etc/passwd");
ebbrt::kprintf("1:%s\n", __PRETTY_FUNCTION__);
  fistream.Then([](ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>> fis) {
    ebbrt::EbbRef<UNIX::InputStream> is = fis.Get();		  
    is->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
           do {
              ebbrt::console::Write((const char *)buf->Data(), buf->Length());  
	   } while(buf->Pop()!=nullptr);
    });
  });
ebbrt::kprintf("_UBENCH_FILE_IN_TEST_: End\n");
#endif

#ifdef _UBENCH_BOOT_IMG_CMD_LINE_TEST_
  ebbrt::kprintf("_UBENCH_BOOT_IMG_CMD_LINE_TEST_: Start\n");
  {
    int argc;
    char **argv;
    char *img_cmdline = NULL;
    
    if (img_cmdline)  ebbrt::kprintf("ubench: BEGIN: %s\n", img_cmdline);

    if (img_cmdline) {
      string_to_argv(img_cmdline, &argc, &argv);
      for (int i=0; i<argc; i++) {
      ebbrt::kprintf("argc[%d]=%s\n", i, argv[i]);
      }
      if (process_img_args(argc, argv) == -1) return;
    }
  }
  ebbrt::kprintf("_UBENCH_BOOT_IMG_CMD_LINE_TEST_: End\n");
#endif

#ifdef _UBENCH_BENCHMARKS_
  ebbrt::kprintf("_UBENCH_BENCHMARKS_: Start\n");
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
  ebbrt::kprintf("_UBENCH_BENCHMARKS_: End\n");
#endif

  return;
}

