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


struct alignas(ebbrt::cache_size) Result {
  uint64_t time;
  int rc;
  char pad[ebbrt::cache_size - sizeof(uint64_t) - sizeof(int)];
} MPResults[ebbrt::Cpu::kMaxCpus];


#ifndef __EBBRT_BM__
ebbrt::Context * indexToCPU(size_t i) 
{
    return ebbrt::Cpu::GetByIndex(i)->get_context();
}
#else
size_t indexToCPU(size_t i) {return i;}
#endif

int
MPTest(const char *name, int cnt, size_t n,
       ebbrt::MovableFunction<int(int)>work)
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
      int rc;
      bar.Wait();
      start = now();
      rc=work(cnt);
      end = now();
      bar.Wait();
      MPResults[i].time=nsdiff(start,end);
      MPResults[i].rc=rc;
      count++;
      while (count < (size_t)n);
      if (ebbrt::Cpu::GetMine()==theCpu) {
	ebbrt::event_manager->ActivateContext(std::move(context));
      }
    }, indexToCPU(i));
  }
  ebbrt::event_manager->SaveContext(context);
  for (size_t i=0; i<n; i++) {
    MY_PRINT("RES: %s: rc=%d n=%" PRIu64 " i=%" PRIu64 ": %" PRId32 " %" PRIu64 "\n", 
	     name, MPResults[i].rc, n, i, cnt, MPResults[i].time);
  }
  assert(n==count);
  MY_PRINT("%s: END\n", name);
  return 1;
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
  mutable MPMultiEbbCtr *repArray[ebbrt::Cpu::kMaxCpus];

  MPMultiEbbCtrRoot(EbbId id) : _id(id) {
    bzero(repArray,sizeof(repArray));
  }

  void setRep(size_t i, MPMultiEbbCtr *rep) const { 
    repArray[i] = rep;
  }

  int gatherVal(void) const;
  int otherGatherVal(void) const;
  void destroy() const;
};


class MPMultiEbbCtr : public MulticoreEbb<MPMultiEbbCtr, MPMultiEbbCtrRoot>  {
  typedef MPMultiEbbCtrRoot Root;
  typedef MulticoreEbb<MPMultiEbbCtr, Root> Parent;
  const Root &_root;
  EbbId myId() { return _root._id; }

  std::atomic<int> _val;
  MPMultiEbbCtr(const Root &root) : _root(root), _val(0) {
    _root.setRep(ebbrt::Cpu::GetMine(), this);
  }
  // give access to the constructor
  friend  Parent;
  friend MPMultiEbbCtrRoot;
public:
  void inc() { _val++; }
  void dec() { _val--; }
  int val() { return _root.gatherVal(); }

  void destroy()  { _root.destroy(); }
  
  static MPMultiEbbCtrRef Create(EbbId id=ebb_allocator->Allocate()) {
    return Parent::Create(new Root(id), id);
  }
};

void
MPMultiEbbCtrRoot::destroy(void) const
{
  size_t numCores=ebbrt::Cpu::Count();
  for (size_t i=0; numCores && i<ebbrt::Cpu::kMaxCpus; i++) {
    if (repArray[i]) { 
      delete repArray[i]; 
      numCores--;
    }
  }
  delete this;
}

int 
MPMultiEbbCtrRoot::gatherVal(void) const 
{
  int gval=0;
  size_t numCores=ebbrt::Cpu::Count();
  for (size_t i=0; numCores && i<ebbrt::Cpu::kMaxCpus; i++) {
    if (repArray[i]) { 
      gval=repArray[i]->_val;
      numCores--;
    }
  }
  return gval;
}

int 
MPMultiEbbCtrRoot::otherGatherVal(void) const
{
  int gval=0;
  LocalIdMap::ConstAccessor accessor; // serves as a lock on the rep map
  auto found = local_id_map->Find(accessor, _id);
  if (!found)
    throw std::runtime_error("Failed to find root for MulticoreEbb");
  auto pair = 
    boost::any_cast<std::pair<MPMultiEbbCtrRoot *, 
			      boost::container::flat_map<size_t, 
							 MPMultiEbbCtr *>>>
    (&accessor->second);
  const auto& rep_map = pair->second;
  for (auto it = rep_map.begin(); it != rep_map.end() ; it++) {
    auto rep = boost::any_cast<const MPMultiEbbCtr *>(it->second);
    gval += rep->_val;
  }
  return gval;
};


Ctr GlobalCtr;
VirtualCtr VirtualGlobalCtr;


#define CtrWork(CTR,CNT,SUM)			\
  {						\
  int rc = 0;					\
  tp start, end;				\
  int i;					\
  start = now();				\
  for (i=0; i<cnt; i++) {			\
    CTR.inc();					\
  }						\
  SUM+=i;					\
  end = now();						\
  MY_PRINT("RES: %s: " #CTR ".inc(): %"			\
		 PRId32 " %" PRIu64 "\n",  __PFUNC__,	\
	   CNT, nsdiff(start,end));			\
  							\
  start = now();				\
  for (i=0; i<CNT; i++) {		\
    CTR.dec();			\
  }						\
  SUM+=i;					\
  end = now();					\
  MY_PRINT("RES: %s: " #CTR ".dec(): %"		\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	     CNT, nsdiff(start,end));	\
						\
  start = now();				\
  for (i=0; i<CNT; i++) {			\
    rc += CTR.val();				\
  }						\
  SUM+=rc;					\
  end = now();					\
  assert(rc==0);				\
  MY_PRINT("RES: %s: " #CTR ".val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	     CNT, nsdiff(start,end));	\
  }

#define CtrRefWork(CTR,CNT,SUM)			\
  {						\
  int rc = 0;					\
  tp start, end;				\
  int i;					\
  start = now();				\
  for (i=0; i<CNT; i++) {			\
    CTR->inc();					\
  }						\
  SUM+=i;					\
  end = now();					\
  MY_PRINT("RES: %s: " #CTR "->inc(): %"	\
	   PRId32 " %" PRIu64 "\n",  __PFUNC__,		\
	   CNT, nsdiff(start,end));			\
  							\
  start = now();				\
  for (i=0; i<CNT; i++) {			\
    CTR->dec();					\
  }						\
  SUM+=i;					\
  end = now();					\
  MY_PRINT("RES: %s: " #CTR "->dec(): %"	\
          PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	   CNT, nsdiff(start,end));		\
						\
  start = now();				\
  for (i=0; i<CNT; i++) {			\
    rc += CTR->val();				\
  }						\
  SUM+=rc;					\
  end = now();					\
  assert(rc==0);				\
  MY_PRINT("RES: %s: " #CTR "->val(): %"	\
	  PRId32 " %" PRIu64 "\n", __PFUNC__,	\
	   CNT, nsdiff(start,end));	\
  }


#define MPCtrRefWorkInc(CTR,CNT,SUM)		\
  { int i;					\
    for (i=0; i<CNT; i++) {			\
      CTR->inc();				\
    }						\
    SUM=i;					\
  }

#define MPCtrRefWorkDec(CTR,CNT,SUM)		\
  { int i;					\
    for (i=0; i<CNT; i++) {			\
      CTR->dec();	                        \
    }                                           \
    SUM=i;					\
  }	

#define MPCtrRefWorkVal(CTR,CNT,SUM)		\
  {						\
    int rc = 0,i;				\
    for (i=0; i<CNT; i++) {			\
      rc += CTR->val();				\
    }						\
    SUM=rc;					\
  }


int
GlobalCtrTest(int cnt) 
{
  int rc=0;
  CtrWork(GlobalCtr,cnt,rc);
  return rc;
}

int
VirtualGlobalCtrTest(int cnt) 
{
  int rc=0;
  CtrWork(VirtualGlobalCtr,cnt,rc);
  return rc;
}

int
StackCtrTest(int cnt) 
{
  int rc=0;
  Ctr stackCtr;
  CtrWork(stackCtr,cnt,rc);
  return rc;
}

int
HeapCtrTest(int cnt)
{  
  int rc=0;
  tp start, end;

  start = now();
  Ctr *heapCtr = new Ctr;
  end = now();
  assert(heapCtr!=NULL);

  MY_PRINT("RES: %s: new Ctr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(heapCtr,cnt,rc);

  start = now();
  delete heapCtr;
  end = now();

  MY_PRINT("RES: %s: delete Ctr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
  return rc;

}


int
VirtualHeapCtrTest(int cnt)
{  
  int rc=0;
  tp start, end;

  start = now();
  VirtualCtr *virtHeapCtr = new VirtualCtr;
  end = now();
  assert(virtHeapCtr!=NULL);

  MY_PRINT("RES: %s: new VirtualCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(virtHeapCtr,cnt,rc);

  start = now();
  delete virtHeapCtr;
  end = now();
  assert(virtHeapCtr!=NULL);

  MY_PRINT("RES: %s: delete VirtualCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
  return rc;
}


int
MPSharedHeapCtrTest(int cnt, size_t n)
{
  tp start, end;

  start = now();
  MPSharedCtr *ctr = new MPSharedCtr;
  end = now();
  assert(ctr!=NULL);

  MY_PRINT("RES: %s: new MPSharedCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  MPTest("int MPSharedHeapCtrTest()::inc", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkInc(ctr,cnt,rc);
      return rc;
    } );

  MPTest("int MPSharedHeapCtrTest()::dec", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkDec(ctr,cnt,rc);
      return rc;
    } );

  MPTest("int MPSharedHeapCtrTest()::val", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkVal(ctr,cnt,rc);
      return rc;
    } );

  start = now();
  delete ctr;
  end = now();

  MY_PRINT("RES: %s: delete MPSharedCtr: %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
  return 1;
}

int
GlobalEbbCtrTest(int cnt)
{
  int rc=0;
  tp start, end;
  EbbCtrRef globalEbbCtr;

  start = now();
  globalEbbCtr = EbbCtr::GlobalCtr();
  end = now();

  MY_PRINT("RES: %s: EbbCtr::GlobalEbbCtr() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(globalEbbCtr,cnt,rc);
  return rc;
}

int
HeapEbbCtrTest(int cnt)
{
  int rc=0;
  tp start, end;
  EbbCtrRef heapEbbCtr;

  start = now();
  heapEbbCtr = EbbCtr::Create();
  end = now();

  MY_PRINT("RES: %s: EbbCtr::Create() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  CtrRefWork(heapEbbCtr,cnt,rc);

  start = now();
  heapEbbCtr->destroy();
  end = now();

  MY_PRINT("RES: %s: EbbCtr::destroy() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
  return rc;
}


int
MPSharedHeapEbbCtrTest(int cnt, size_t n)
{
  tp start, end;
  MPSharedEbbCtrRef ctr;

  start = now();
  ctr = MPSharedEbbCtr::Create();
  end = now();

  MY_PRINT("RES: %s: MPSharedEbbCtr::Create() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  MPTest("int MPSharedHeapEbbCtrTest()::inc", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkInc(ctr,cnt,rc);
      return rc;
    } );

  MPTest("int MPSharedHeapEbbCtrTest()::dec", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkDec(ctr,cnt,rc);
      return rc;
    } );

  MPTest("int MPSharedHeapEbbCtrTest()::val", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkVal(ctr,cnt,rc);
      return rc;
    } );

  start = now();
  ctr->destroy();
  end = now();

  MY_PRINT("RES: %s: MPSharedEbbCtr::destroy() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
  return 1;

}


int
MPMultiHeapEbbCtrTest(int cnt, size_t n)
{
  tp start, end;
  MPMultiEbbCtrRef ctr;

  start = now();
  ctr = MPMultiEbbCtr::Create();
  end = now();

  MY_PRINT("RES: %s: MPMultiEbbCtr::Create() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));

  MPTest("int MPMultiHeapEbbCtrTest()::inc", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkInc(ctr,cnt,rc);
      return rc;
    } );

  MPTest("int MPMultiHeapEbbCtrTest()::dec", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkDec(ctr,cnt,rc);
      return rc;
    } );

  MPTest("int MPMultiHeapEbbCtrTest()::val", cnt, n, [ctr](int cnt) {
      int rc=0;
      MPCtrRefWorkVal(ctr,cnt,rc);
      return rc;
    } );

  start = now();
  ctr->destroy();
  end = now();

  MY_PRINT("RES: %s: MPMultiEbbCtr::destroy() : %" PRId32 " %" PRIu64 "\n", 
	   __PFUNC__,1, nsdiff(start,end));
  return 1;
}

int
mp_test(struct Arguments *args)
{
  if (!args->tests.mp) return 0;
  int rc=0;
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  int n = args->processorCnt;

  MY_PRINT("_UBENCH_MP_TEST_: START\n");
  // Base line C++ method dispatch numbers 
  for (int i=0; i<rcnt; i++) {
    rc+=MPSharedHeapCtrTest(acnt,n);
    rc+=MPSharedHeapEbbCtrTest(acnt,n);
    rc+=MPMultiHeapEbbCtrTest(acnt, n);
  }
  MY_PRINT("_UBENCH_MP_TEST_: END\n");
  return rc;
}

int
cpp_test(struct Arguments *args)
{
  if (!args->tests.cpp) return 0;
  int rc=0;
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  
  MY_PRINT("_UBENCH_CPP_TEST_: START\n");
  // Base line C++ method dispatch numbers 
  for (int i=0; i<rcnt; i++) {
    rc+=GlobalCtrTest(acnt);
    rc+=StackCtrTest(acnt);
    rc+=HeapCtrTest(acnt);
    rc+=VirtualGlobalCtrTest(acnt);
    rc+=VirtualHeapCtrTest(acnt);
  }
  MY_PRINT("_UBENCH_CPP_TEST_: END\n");
  return rc;
}

int 
ebb_test(struct Arguments *args) 
{
  if (!args->tests.ebb) return 0;
  int rc=0;
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;

  MY_PRINT("_UBENCH_EBB_TEST_: START\n");
    // Base line Ebb method dispatch numbers 
  for (int i=0; i<rcnt; i++) {
    rc+=GlobalEbbCtrTest(acnt);
    rc+=HeapEbbCtrTest(acnt);
  }

  MY_PRINT("_UBENCH_EBB_TEST_: END\n");
  return rc;
}

extern void hoard_threadtest(int,char **);

int
malloc_test(struct Arguments *args)
{
  if (!args->tests.malloc) return 0;

  MY_PRINT("_UBENCH_MALLOC_TEST_: START\n");

  hoard_threadtest(UNIX::cmd_line_args->argc()-optind,
		   &(UNIX::cmd_line_args->data()[optind]));

  MY_PRINT("_UBENCH_MALLOC_TEST_: END\n");
  return 1;
}

int
spawnNullLocalTest( int acnt, int n)
{
  MPTest(__PFUNC__, acnt, n, [](int acnt) {
      int i;
      for (i=0; i<acnt; i++) 
	ebbrt::event_manager->Spawn([](){},/* force_async=*/true);
      return i;
    });
  return 1;
}

int
spawnNullRemoteTest(int acnt, int n)
{
  size_t numCores = ebbrt::Cpu::Count();
  MPTest(__PFUNC__, acnt, n, [numCores](int acnt) {
      int i;
      for (i=0; i<acnt; i++) {
	for (size_t j=0; j<numCores; j++) {
	  ebbrt::event_manager->SpawnRemote([](){}, indexToCPU(j));
	}
      }
      return i;
    });
  return 1;
}

int
spawn_test(struct Arguments *args)
{
  if (!args->tests.spawn)  return 0; 
  int rc=0;
  MY_PRINT("_UBENCH_SPAWN_TEST_: Start\n");

  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  int n = args->processorCnt; 

  for (int i=0; i<rcnt; i++) {
    rc+=spawnNullLocalTest(acnt, n);
    rc+=spawnNullRemoteTest(acnt, n);
  }

  MY_PRINT("_UBENCH_SPAWN_TEST_: END\n");
  return rc;
}


int cmdline_test(struct Arguments *args) 
{
  if (!args->tests.cmdline) return 0;
  MY_PRINT("_UBENCH_CMDLINE_TEST_: Start\n");
  for (int i=0; i<UNIX::cmd_line_args->argc(); i++) {
    MY_PRINT("UNIX::cmd_line_args->argv(%d)=%s\n",i,
		   UNIX::cmd_line_args->argv(i));
  }
  for (int i=optind; i < UNIX::cmd_line_args->argc(); i++) {
    MY_PRINT("non option arguments %s\n", UNIX::cmd_line_args->argv(i));
  }
  MY_PRINT("_UBENCH_CMDLINE_TEST_: END\n");
  return 1;
}

int env_test(struct Arguments *args) {
  if (!args->tests.env)  return 0;
  MY_PRINT("_UBENCH_ENVIRONMENT_TEST_: Start\n");
  for (int i=0; UNIX::environment->environ()[i]!=NULL; i++) {
    MY_PRINT("%d: ev=%s\n", i, UNIX::environment->environ()[i]);
  }
  MY_PRINT("getenv(\"hello\")=%s\n", UNIX::environment->getenv("hello"));
  MY_PRINT("_UBENCH_ENVIRONMENT_TEST_: End\n");
  return 1;
}

int timing_test(struct Arguments *args) {
  if (!args->tests.timing)  return 0;
  MY_PRINT("_UBENCH_TIMER_TEST_: Start\n");
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  int n = args->processorCnt; 

  for (int i=0; i<rcnt; i++) {
    MPTest(__PFUNC__, acnt, n, [](int acnt) {
	int j;
	for (j=0; j<acnt; j++) {
	  asm volatile (" ");
	}
	return j;
      });
  }
  MY_PRINT("_UBENCH_TIMER_TEST_: End\n");
  return 1;
}

void nullfunc(void)
{
  asm volatile ("");
}

int nullfunc_test(struct Arguments *args) {
  if (!args->tests.nullfunc)  return 0;
  MY_PRINT("_UBENCH_NULLFUNC_TEST_: Start\n");
  int rcnt = args->repeatCnt;
  int acnt = args->actionCnt;
  int n = args->processorCnt; 

  for (int i=0; i<rcnt; i++) {
    MPTest(__PFUNC__, acnt, n, [](int acnt) {
	int j;
	for (j=0; j<acnt; j++) nullfunc();
	return j;
      });
  }
  MY_PRINT("_UBENCH_NULLFUNC_TEST_: End\n");
  return 1;
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

  while ((c = getopt (argc, argv, "hA:BCEF:IP:R:cevnmpst")) != -1) {
    switch (c)
      { 
      case 'h':
	MY_PRINT("%s: [-h] [-A actionCount] [-B] [-C] [-E] [-F file] [-I] [-P  processorCount] [-R repeatCount] [-c] [-e] [-m] [-n] [-p] [-s] [-t]\n"
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
  	        " -n : test/calibrate nullfunc invocation cost\n"
  	        " -p : run MP C++ and Ebb tests\n"
   	        " -s : run MP Local and Remote spawn test\n"
		" -t : test/calibrate timing\n", argv[0]);
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
      case 'n':
	args->tests.nullfunc = 1;
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
      case 't':
	args->tests.timing = 1;
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

int
standardin_test(struct Arguments *args)
{
  if (!args->tests.standardin)  return 0;
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
  return 1;
}

int
filein_test(struct Arguments *args)
{
  if (!args->tests.filein) return 0;
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
  return 1;
}


void AppMain()
{
  int rc=0;
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

  rc+=cmdline_test(&args);
  rc+=env_test(&args);
  rc+=standardin_test(&args);
  rc+=filein_test(&args);

#ifdef __EBBRT_BM__
  bootimgargs_test(&args);
#endif

  rc+=timing_test(&args);
  rc+=nullfunc_test(&args);
  rc+=cpp_test(&args);
  rc+=ebb_test(&args);
  rc+=spawn_test(&args);
  rc+=mp_test(&args);
  rc+=malloc_test(&args);

  MY_PRINT("_UBENCH_BENCHMARKS_: rc=%d End\n", rc);

 DONE:
#ifndef __EBBRT_BM__
  ebbrt::Cpu::Exit(0);
#else 
  UNIX::root_fs->processExit(1);
#endif
  
  return;
}
