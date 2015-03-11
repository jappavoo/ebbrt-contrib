#ifndef __UBENCH_COMMON_H__
#define __UBENCH_COMMON_H__

#define ACTION_CNT 1000000
#define REPEAT_CNT 10

struct Arguments {
  struct {
    int cpp;
    int ebb;
    int event;
    int malloc;
    int standardin;
    char *filein;
    int cmdline;
    int env;
    int spawn;
  } tests;
  int backend;
  int repeatCnt;
  int actionCnt;
};

int process_args(int, char **, struct Arguments *);
void cmdline_test(struct Arguments *);
void env_test(struct Arguments *);
void AppMain(void);

struct MainArgs {
  int argc;
  char **argv;
};

extern struct MainArgs margs;

#endif
