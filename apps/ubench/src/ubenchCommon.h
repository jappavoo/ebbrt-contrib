#ifndef __UBENCH_COMMON_H__
#define __UBENCH_COMMON_H__

#include <ebbrt/EventManager.h>
#include <ebbrt/SpinBarrier.h>

#ifndef __EBBRT_BM__
#define MY_PRINT printf
#else
#define MY_PRINT ebbrt::kprintf
#endif

// DEFAULT IS TO BUILD A STANDALONE BACKEND 
// IMAGE
#define __STANDALONE__ 

#define ACTION_CNT    1000000
#define REPEAT_CNT    10
#define PROCESSOR_CNT 1

struct Arguments {
  struct {
    int cpp;
    int ebb;
    int malloc;
    int standardin;
    char *filein;
    int cmdline;
    int env;
    int spawn;
    int mp;
    int timing;
    int nullfunc;
    int nullobject;
    int bootargs;
  } tests;
  int backend;
  int standalone;
  int repeatCnt;
  int actionCnt;
  int processorCnt;
  int argc;
  char** argv;
};


typedef ebbrt::clock::HighResTimer MyTimer;
static inline void ns_start(MyTimer &t) { t.tick(); }
static inline uint64_t ns_stop(MyTimer &t) { return t.tock().count(); }

struct alignas(ebbrt::cache_size) Result {
  uint64_t time;
  int rc;
  char pad[ebbrt::cache_size - sizeof(uint64_t) - sizeof(int)];
};

extern struct Result MPResults[ebbrt::Cpu::kMaxCpus];

#ifndef __EBBRT_BM__
static ebbrt::Context * indexToCPU(size_t i) 
{
    return ebbrt::Cpu::GetByIndex(i)->get_context();
}
#else
static size_t indexToCPU(size_t i) {return i;}
#endif

template<typename F>
int
MPTest(const char *name, int cnt, size_t n,
       F&& work)
{  
  if (n<0) n=1; else if (n>ebbrt::Cpu::Count()) n=ebbrt::Cpu::Count();

  static ebbrt::SpinBarrier bar(n);
  size_t theCpu=ebbrt::Cpu::GetMine();
  std::atomic<size_t> count(0);

  MY_PRINT("%s: Start\n", name);

  ebbrt::EventManager::EventContext context;
  for (size_t i=0; i<n; i++) {
    ebbrt::event_manager->SpawnRemote([i,n,&context,theCpu,&count,
				       &work,cnt]() {
      uint64_t ns;
      MyTimer tmr;
      int rc;
      bar.Wait();
      ns_start(tmr);
      rc=std::forward<F>(work)(cnt);
      ns = ns_stop(tmr);
      bar.Wait();
      MPResults[i].time=ns;
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


void AppMain(void);

struct MainArgs {
  int argc;
  char **argv;
};

extern struct MainArgs margs;

#endif
