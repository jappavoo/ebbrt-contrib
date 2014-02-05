#include <ebbrt/Debug.h>
#include <argv.h>

void appmain(char *cmdline)
{
  char **argv;
  int argc;

  ebbrt::kprintf("Hello World!!!\n"); 

  if (cmdline!=NULL) {
    ebbrt::kprintf("cmdline: %s\n", cmdline);
    string_to_argv(cmdline, &argc, &argv);
    for (int i=0; i<argc; i++) {
      ebbrt::kprintf("argc[%d]=%s\n", i, argv[i]);
    }
  }
  ebbrt::kprintf("%s: done\n", __PRETTY_FUNCTION__);
}

