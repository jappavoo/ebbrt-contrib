#include <ebbrt/Debug.h>
#include <ebbrt/Console.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <argv.h>

#include <unistd.h>
#include <cctype>

#include "../../src/Unix.h"
#include "../../src/ubenchCommon.h"


void
stdin_test(struct Arguments *args) 
{
#ifdef _UBENCH_STANDARD_IN_TEST_
if (!args->tests.standardin) return;
  ebbrt::kprintf("_UBENCH_STANDARD_IN_TEST_: Start\n");
  UNIX::sin->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
      if (buf->Data() == NULL) { printf("Stream EOF\n"); return; }
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
}

void 
filein_test(struct Arguments *args) 
{
  if (!args->tests.filein) return;
#ifdef _UBENCH_FILE_IN_TEST_
  //asm volatile ("jmp .");
ebbrt::kprintf("_UBENCH_FILE_IN_TEST_: Start\n");
ebbrt::kprintf("0:%s\n", __PRETTY_FUNCTION__);
  auto fistream = UNIX::root_fs->openInputStream("/etc/passwd");
ebbrt::kprintf("1:%s\n", __PRETTY_FUNCTION__);
c  fistream.Then([](ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>> fis) {
    ebbrt::EbbRef<UNIX::InputStream> is = fis.Get();		  
    is->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
           if (buf->Data() == NULL) { ebbrt::kprintf("Stream EOF\n"); return; }
           do {
              ebbrt::console::Write((const char *)buf->Data(), buf->Length());  
	   } while(buf->Pop()!=nullptr);
    });
  });
ebbrt::kprintf("_UBENCH_FILE_IN_TEST_: End\n");
#endif

}

void
bootimgargs_test(struct Arguments *args)
{
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
    }
  }
  ebbrt::kprintf("_UBENCH_BOOT_IMG_CMD_LINE_TEST_: End\n");
#endif
}

