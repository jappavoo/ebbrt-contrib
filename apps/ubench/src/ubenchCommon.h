#ifndef __UBENCH_COMMON_H__
#define __UBENCH_COMMON_H__

#define ACTION_CNT 100000
#define REPEAT_CNT 10

#define _UBENCH_CMD_LINE_ARGS_TEST_
#define _UBENCH_ENVIRONMENT_TEST_
//#define _UBENCH_STANDARD_IN_TEST_
//#define _UBENCH_FILE_IN_TEST_
//#define _UBENCH_BOOT_IMG_CMD_LINE_TEST_

#define   _UBENCH_BENCHMARKS_

struct Arguments {
  struct {
    int cpp;
    int ebb;
    int event;
    int malloc;
    int standardin;
    int filein;
    int cmdline;
    int env;
  } tests;
  int repeatCnt;
  int actionCnt;
};

int process_args(int, char **, struct Arguments *);
void cmdline_test(struct Arguments *);
void env_test(struct Arguments *);
void AppMain(void);

#endif
