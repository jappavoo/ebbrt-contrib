#ifndef __UBENCH_COMMON_H__
#define __UBENCH_COMMON_H__

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
    int bootargs;
  } tests;
  int backend;
  int standalone;
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

typedef ebbrt::clock::HighResTimer Timer;
inline void ns_start(Timer &t) { t.tick(); }
inline uint64_t ns_stop(Timer &t) { return t.tock().count(); }

#endif
