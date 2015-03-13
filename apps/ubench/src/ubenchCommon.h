#ifndef __UBENCH_COMMON_H__
#define __UBENCH_COMMON_H__

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
  } tests;
  int backend;
  int repeatCnt;
  int actionCnt;
  int processorCnt;
};

int MPTest(const char *name, int cnt, size_t n,
	    ebbrt::MovableFunction<int(int)>work);


void AppMain(void);

struct MainArgs {
  int argc;
  char **argv;
};

extern struct MainArgs margs;

#ifndef __EBBRT_BM__
typedef std::chrono::high_resolution_clock myclock;
#else
#include <ebbrt/Clock.h>
typedef ebbrt::clock::Wall myclock;
#endif

typedef std::chrono::time_point<myclock> tp;
static inline tp now() { return myclock::now(); }

static inline uint64_t
nsdiff(tp start, tp end)
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>
  (end-start).count();
}

#endif
