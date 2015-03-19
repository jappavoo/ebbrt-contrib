#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>
#include <ebbrt/Multiboot.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <argv.h>

#include <unistd.h>
#include <cctype>

#include "../../src/Unix.h"
#include "../../src/ubenchCommon.h"

void
bootimgargs_test(struct Arguments *args)
{
  if (!args->tests.bootargs) return;
  ebbrt::kprintf("_UBENCH_BOOT_IMG_CMD_LINE_TEST_: Start\n");
  {
    int argc;
    char **argv;
    char *img_cmdline = (char *)ebbrt::multiboot::cmdline_addr_;
    
    if (img_cmdline)  ebbrt::kprintf("ubench: BEGIN: %s\n", img_cmdline);

    if (img_cmdline) {
      string_to_argv(img_cmdline, &argc, &argv);
      for (int i=0; i<argc; i++) {
        ebbrt::kprintf("argc[%d]=%s\n", i, argv[i]);
      }
    }
  }
  ebbrt::kprintf("_UBENCH_BOOT_IMG_CMD_LINE_TEST_: End\n");
}

