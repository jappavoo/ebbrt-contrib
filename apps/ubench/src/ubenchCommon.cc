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

#else

// BAREMETAL
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <argv.h>
#include <unistd.h>
#include <strings.h>
#include <cctype>

#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>

#define MY_PRINT ebbrt::kprintf

void  bootimgargs_test(struct Arguments *);
#endif

#include <ebbrt/SharedEbb.h>
#include <ebbrt/MulticoreEbb.h>
#include <ebbrt/SpinBarrier.h>
#include <ebbrt/CacheAligned.h>
 
#include "Unix.h"
#include "ubenchCommon.h"

#define __FUNC__ __FUNCTION__
#define __PFUNC__ __PRETTY_FUNCTION__

using namespace ebbrt;


struct alignas(cache_size) Result {
  uint64_t time;
  char pad[ebbrt::cache_size - sizeof(uint64_t)];
} MPResults[ebbrt::Cpu::kMaxCpus];


#ifndef __EBBRT_BM__
ebbrt::Context * indexToCPU(size_t i) 
{
    return ebbrt::Cpu::GetByIndex(i)->get_context();
}
#else
size_t indexToCPU(size_t i) {return i;}
#endif

void
MPTest(const char *name, int cnt, size_t n,
       ebbrt::MovableFunction<void(int)>work)
{  
  if (n<0) n=1; else if (n>ebbrt::Cpu::Count()) n=ebbrt::Cpu::Count();

  static ebbrt::SpinBarrier bar(n);
  size_t theCpu=ebbrt::Cpu::GetMine();
  std::atomic<size_t> count(0);

  MY_PRINT("%s: Start\n", name);

  EventManager::EventContext context;
  for (size_t i=0; i<n; i++) {
    ebbrt::event_manager->SpawnRemote([i,n,&context,theCpu,&count,
				       &work,cnt]() {
      tp start, end;
      bar.Wait();
      start = now();
      work(cnt);
      end = now();
      bar.Wait();
      MPResults[i].time=nsdiff(start,end);
      count++;
      while (count < (size_t)n);
      if (ebbrt::Cpu::GetMine()==theCpu) {
	ebbrt::event_manager->ActivateContext(std::move(context));
      }
    }, indexToCPU(i));
  }
  ebbrt::event_manager->SaveContext(context);
  for (size_t i=0; i<n; i++) {
    MY_PRINT("RES: %s: n=%" PRIu64 " i=%" PRIu64 ": %" PRId32 " %" PRIu64 "\n", 
	     name, n, i, cnt, MPResults[i].time);
  }
  assert(n==count);
  MY_PRINT("%s: END\n", name);
}

class Ctr {
  int _val;
public:
  Ctr() : _val(0) {}
  void inc() { _val++; }
  void dec() { _val--; }
  int  val() { return _val; }
};

class VirtualCtr {
  int _val;
public:
  VirtualCtr() : _val(0) {}
  virtual ~VirtualCtr() {}
  virtual void inc() { _val++; }
  virtual void dec() { _val--; }
  virtual int val() { return _val; }
};


class MPSharedCtr {
  std::atomic<int> _val;
public:
  MPSharedCtr() : _val(0) {}
  void inc() { _val++; }
  void dec() { _val--; }
  int val() { return _val; }
};

class EbbCtr;
typedef EbbRef<EbbCtr> EbbCtrRef ;

class EbbCtr : public SharedEbb<EbbCtr> {
  int _val;
  EbbCtr() : _val(0)  {}
public:
  void inc() { _val++; }
  void dec() { _val--; }
  int val() { return _val; }

  void destroy() { 
    // FIXME: We don't support EbbId Cleanup yet so
    // we are only reclaiming rep memory
    delete this;
  }

  static EbbCtrRef Create(EbbId id=ebb_allocator->Allocate()) {
    return SharedEbb<EbbCtr>::Create(new EbbCtr, id);
  }

  static EbbCtrRef GlobalCtr() {
    static EbbCtr theGlobalCtr;
    static EbbId theId = ebb_allocator->Allocate();
    static EbbCtrRef theRef = 
      SharedEbb<EbbCtr>::Create(&theGlobalCtr, theId); 
    return theRef;
  }

};

class MPSharedEbbCtr;
typedef EbbRef<MPSharedEbbCtr> MPSharedEbbCtrRef;

class MPSharedEbbCtr : public SharedEbb<MPSharedEbbCtr> {
  std::atomic<int> _val;
  MPSharedEbbCtr() : _val(0) {}
public:
  void inc() { _val++; }
  void dec() { _val--; }
  int val() { return _val; }

  void destroy() { 
    // FIXME: We don't support EbbId Cleanup yet so
    // we are only reclaiming rep memory
    delete this;
  }

  static MPSharedEbbCtrRef Create(EbbId id=ebb_allocator->Allocate()) {
    return SharedEbb<MPSharedEbbCtr>::Create(new MPSharedEbbCtr, id);
  }

  static MPSharedEbbCtrRef GlobalCtr() {
    static MPSharedEbbCtr theGlobalCtr;
    static EbbId theId = ebb_allocator->Allocate();
    static MPSharedEbbCtrRef theRef = 
      SharedEbb<MPSharedEbbCtr>::Create(&theGlobalCtr, theId); 
    return theRef;
  }
};

class MPMultiEbbCtr;
typedef EbbRef<MPMultiEbbCtr> MPMultiEbbCtrRef;

class MPMultiEbbCtrRoot {
private:
  friend class MPMultiEbbCtr;
  EbbId _id;
  MPMultiEbbCtrRoot(EbbId id) : _id(id) {}
};


class MPMultiEbbCtr : public MulticoreEbb<MPMultiEbbCtr, MPMultiEbbCtrRoot>  {
  typedef MPMultiEbbCtrRoot Root;
  typedef MulticoreEbb<MPMultiEbbCtr, Root> Parent;
  const Root &_root;
  EbbId myId() { return _root._id; }

  std::atomic<int> _val;
  MPMultiEbbCtr(const Root &root) : _root(root), _val(0) {}
  // give access to the constructor
  friend  Parent;
public:
  void inc() { _val++; }
  void dec() { _val--; }
  int val()  {    
    int sum=0;
    LocalIdMap::ConstAccessor accessor; // serves as a lock on the rep map
    auto found = local_id_map->Find(accessor, myId());
    if (!found)
        throw std::runtime_error("Failed to find root for MulticoreEbb");
    auto pair = 
      boost::any_cast<std::pair<Root *, 
				boost::container::flat_map<size_t, 
							   MPMultiEbbCtr *>>>
      (&accessor->second);
    const auto& rep_map = pair->second;
    for (auto it = rep_map.begin(); it != rep_map.end() ; it++) {
      auto rep = boost::any_cast<const MPMultiEbbCtr *>(it->second);
      sum += rep->_val;
    }
    return sum;
  };

  void destroy()  {    
    LocalIdMap::Accessor accessor; // serves as a lock on the rep map
    auto found = local_id_map->Find(accessor, myId());
    if (!found)
        throw std::runtime_error("Failed to find root for MulticoreEbb");
    auto pair = 
      boost::any_cast<std::pair<Root *, 
				boost::container::flat_map<size_t, 
							   MPMultiEbbCtr *>>>
      (&accessor->second);
    auto& rep_map = pair->second;
    for (auto it = rep_map.begin(); it != rep_map.end() ; it++) {
      auto rep = boost::any_cast<const MPMultiEbbCtr *>(it->second);
      delete rep;
      it->second = NULL;
    }
  };
  
  static MPMultiEbbCtrRef Create(EbbId id=ebb_allocator->Allocate()) {
    return Parent::Create(new Root(id), id);
  }
};


Ctr GlobalCtr;
VirtualCtr VirtualGlobalCtr;


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


#define MPCtrRefWorkInc(CTR,CNT)		\
  {						\
    for (int i=0; i<CNT; i++) {			\
      CTR->inc();				\
    }						\
  }

#define MPCtrRefWorkDec(CTR,CNT)		\
  {						\
    for (int i=0; i<CNT; i++) {			\
      CTR->dec();	                        \
    }                                           \
  }	

#define MPCtrRefWorkVal(CTR,CNT)		\
  {						\
    int rc = 0;					\
    for (int i=0; i<CNT; i++) {			\
      rc += CTR->val();				\
    }						\
  }


void
GlobalCtrTest(int cnt) 
{
  CtrWork(GlobalCtr,cnt)
}

void
VirtualGlobalCtrTest(int cnt) 
{
  CtrWork(VirtualGlobalCtr,cnt)
}

void
StackCtrTest(int cnt) 
{
  Ctr stackCtr;
  CtrWork(stackCtr,cnt)
}

void
HeapCtrTest(int cnt)
{  
  tp start, end;

  start = now();
  Ctr *heapCtr = new Ctr;
  end = now();
  assert(heapCtr!=NULL);

  MY_PRINT("RES: %s: new Ctr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(heapCtr,cnt);

  start = now();
  delete heapCtr;
  end = now();

  MY_PRINT("RES: %s: delete Ctr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

}


void
VirtualHeapCtrTest(int cnt)
{  
  tp start, end;

  start = now();
  VirtualCtr *virtHeapCtr = new VirtualCtr;
  end = now();
  assert(virtHeapCtr!=NULL);

  MY_PRINT("RES: %s: new VirtualCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(virtHeapCtr,cnt);

  start = now();
  delete virtHeapCtr;
  end = now();
  assert(virtHeapCtr!=NULL);

  MY_PRINT("RES: %s: delete VirtualCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
}


void
MPSharedHeapCtrTest(int cnt, size_t n)
{
  tp start, end;

  start = now();
  MPSharedCtr *ctr = new MPSharedCtr;
  end = now();
  assert(ctr!=NULL);

  MY_PRINT("RES: %s: new MPSharedCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  MPTest("MPSharedHeapCtrTest()::inc", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkInc(ctr,cnt);
    } );

  MPTest("MPSharedHeapCtrTest()::dec", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkDec(ctr,cnt);
    } );

  MPTest("MPSharedHeapCtrTest()::val", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkVal(ctr,cnt);
    } );

  start = now();
  delete ctr;
  end = now();

  MY_PRINT("RES: %s: delete MPSharedCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
}

void
GlobalEbbCtrTest(int cnt)
{
  tp start, end;
  EbbCtrRef globalEbbCtr;

  start = now();
  globalEbbCtr = EbbCtr::GlobalCtr();
  end = now();

  MY_PRINT("RES: %s: EbbCtr::GlobalEbbCtr() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(globalEbbCtr,cnt);
}

void
HeapEbbCtrTest(int cnt)
{
  tp start, end;
  EbbCtrRef heapEbbCtr;

  start = now();
  heapEbbCtr = EbbCtr::Create();
  end = now();

  MY_PRINT("RES: %s: EbbCtr::Create() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(heapEbbCtr,cnt);

  start = now();
  heapEbbCtr->destroy();
  end = now();

  MY_PRINT("RES: %s: EbbCtr::destroy() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
}


void
MPSharedHeapEbbCtrTest(int cnt, size_t n)
{
  tp start, end;
  MPSharedEbbCtrRef ctr;

  start = now();
  ctr = MPSharedEbbCtr::Create();
  end = now();

  MY_PRINT("RES: %s: MPSharedEbbCtr::Create() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  MPTest("MPSharedHeapEbbCtrTest()::inc", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkInc(ctr,cnt);
    } );

  MPTest("MPSharedHeapEbbCtrTest()::dec", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkDec(ctr,cnt);
    } );

  MPTest("MPSharedHeapEbbCtrTest()::val", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkVal(ctr,cnt);
    } );

  start = now();
  ctr->destroy();
  end = now();

  MY_PRINT("RES: %s: MPSharedEbbCtr::destroy() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

}


void
MPMultiHeapEbbCtrTest(int cnt, size_t n)
{
  tp start, end;
  MPMultiEbbCtrRef ctr;

  start = now();
  ctr = MPMultiEbbCtr::Create();
  end = now();

  MY_PRINT("RES: %s: MPMultiEbbCtr::Create() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  MPTest("MPMultiHeapEbbCtrTest()::inc", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkInc(ctr,cnt);
    } );

  MPTest("MPMultiHeapEbbCtrTest()::dec", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkDec(ctr,cnt);
    } );

  MPTest("MPMultiHeapEbbCtrTest()::val", cnt, n, [ctr](int cnt) {
      MPCtrRefWorkVal(ctr,cnt);
    } );

  start = now();
  ctr->destroy();
  end = now();

  MY_PRINT("RES: %s: MPMultiEbbCtr::destroy() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
}

void
mp_test(struct Arguments *args)
{
  if (!args->tests.mp) return;
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  int n = args->processorCnt;

  MY_PRINT("_UBENCH_MP_TEST_: START\n");
  // Base line C++ method dispatch numbers 
  for (int i=0; i<rcnt; i++) {
    MPSharedHeapCtrTest(acnt,n);
    MPSharedHeapEbbCtrTest(acnt,n);
    MPMultiHeapEbbCtrTest(acnt, n);
  }
  MY_PRINT("_UBENCH_MP_TEST_: END\n");

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
    GlobalCtrTest(acnt);
    StackCtrTest(acnt);
    HeapCtrTest(acnt);
    VirtualGlobalCtrTest(acnt);
    VirtualHeapCtrTest(acnt);
  }
  MY_PRINT("_UBENCH_CPP_TEST_: END\n");
}

void ebb_test(struct Arguments *args) 
{
  if (!args->tests.ebb) return;
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;

  MY_PRINT("_UBENCH_EBB_TEST_: START\n");
    // Base line Ebb method dispatch numbers 
  for (int i=0; i<rcnt; i++) {
    GlobalEbbCtrTest(acnt);
    HeapEbbCtrTest(acnt);
  }

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

void
spawnNullLocalTest( int acnt, int n)
{
  MPTest(__PFUNC__, acnt, n, [](int acnt) {
      for (int i=0; i<acnt; i++) 
	ebbrt::event_manager->Spawn([](){},/* force_async=*/true);
    });
}

void
spawnNullRemoteTest(int acnt, int n)
{
  size_t numCores = ebbrt::Cpu::Count();
  MPTest(__PFUNC__, acnt, n, [numCores](int acnt) {
      for (int i=0; i<acnt; i++) {
	for (size_t j=0; j<numCores; j++) {
	  ebbrt::event_manager->SpawnRemote([](){}, indexToCPU(j));
	}
      }
    });
}

void
spawn_test(struct Arguments *args)
{
  if (!args->tests.spawn)  return; 
  MY_PRINT("_UBENCH_SPAWN_TEST_: Start\n");

  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  int n = args->processorCnt; 

  for (int i=0; i<rcnt; i++) {
    spawnNullLocalTest(acnt, n);
    spawnNullRemoteTest(acnt, n);
  }

  MY_PRINT("_UBENCH_SPAWN_TEST_: END\n");
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


int 
process_args(int argc, char **argv, struct Arguments *args) 
{
  int c;
  
  bzero(args, sizeof(struct Arguments));

  args->repeatCnt = REPEAT_CNT;
  args->actionCnt = ACTION_CNT;
  args->processorCnt = PROCESSOR_CNT;

  opterr = 0;

  while ((c = getopt (argc, argv, "hA:BCEF:IP:R:cevmps")) != -1) {
    switch (c)
      { 
      case 'h':
	MY_PRINT("%s: [-h] [-A actionCount] [-B] [-C] [-E] [-F file] [-I] [-P  processorCount] [-R repeatCount] [-c] [-e] [-m] [-p] [-s]\n"
		" EbbRT micro benchmarks\n"
		" -h : help\n"
		" -A actionCount : number of times to do test action\n"
                " -B : run tests on Backend\n"
                " -C : Command line test\n"
                " -E : Environment test\n"
		" -F file : File Input test\n"
		" -I : Standard Input test\n"
                " -P processorCount: number of processors/cores\n"
		" -R repeatCount : number of times to repeat test\n"
		" -c : run basic UP  C++ tests\n"
		" -e : run basic UP Ebb tests\n"
		" -m : malloc tests\n"
  	        "- p : run MP C++ and Ebb tests\n"
   	        " -s : run MP Local and Remote spawn test\n", argv[0]);
	return -1;
      case 'A':
	args->actionCnt = atoi(optarg);
	break;
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
      case 'P':
	args->processorCnt = atoi(optarg);
	break;
      case 'R':
	args->repeatCnt = atoi(optarg);
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
      case 'p':
	args->tests.mp = 1;
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
  spawn_test(&args);
  mp_test(&args);
  malloc_test(&args);

  MY_PRINT("_UBENCH_BENCHMARKS_: End\n");

 DONE:
#ifndef __EBBRT_BM__
  ebbrt::Cpu::Exit(0);
#else 
  UNIX::root_fs->processExit(1);
#endif
  
  return;
}
