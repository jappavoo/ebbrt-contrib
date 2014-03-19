#include <signal.h>

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
    ebbrt::EbbRef<UNIX::CmdLineArgs> cmdLine = 
      UNIX::CmdLineArgs::Create(argc, (const char **)argv);
    for (int i=0; i<cmdLine->argc(); i++) {
      printf("argv[%d]=%s\n",i,cmdLine->argv(i));
    }
    ebbrt::node_allocator->AllocateNode(bindir.string());
  }

   c.Run();
  return 0;
}
