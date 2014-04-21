#include <signal.h>

#include <array>

#include <boost/filesystem.hpp>

#include <ebbrt/Context.h>
#include <ebbrt/ContextActivation.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/NodeAllocator.h>
#include <ebbrt/Runtime.h>
#include <stdio.h>

#include "../../src/Unix.h"

int 
main(int argc, char **argv)
{
  printf("Hello World!!!\n");
  auto bindir = boost::filesystem::system_complete(argv[0]).parent_path() /
    "/bm/ubench.elf32";

  ebbrt::Runtime runtime;
  ebbrt::Context c(runtime);
  boost::asio::signal_set sig(c.io_service_, SIGINT);

  { // scope to trigger automatic deactivate on exit
    ebbrt::ContextActivation activation(c);
    // ensure clean quit on ctrl-c
    sig.async_wait([&c](const boost::system::error_code& ec,
                        int signal_number) { c.io_service_.stop(); });

    UNIX::Init(argc, (const char **)argv);
    for (int i=0; i<UNIX::cmd_line_args->argc(); i++) {
      printf("argv[%d]=%s\n",i,UNIX::cmd_line_args->argv(i));
    }

    for (int i=0; UNIX::environment->environ()[i]!=NULL; i++) {
      printf("%d: ev=%s\n", i, UNIX::environment->environ()[i]);
    }
    printf("getenv(\"hello\")=%s\n", UNIX::environment->getenv("hello"));

#if 0
    UNIX::sin->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
	  size_t n = write(STDOUT_FILENO, buf->Data(), buf->Length()); 
	  if (n<=0) throw std::runtime_error("write to stdout failed");
    });
#else
    ebbrt::node_allocator->AllocateNode(bindir.string());
#endif
  }

  c.Run();
  return 0;
}
