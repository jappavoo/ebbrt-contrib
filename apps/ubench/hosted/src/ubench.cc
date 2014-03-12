#include <signal.h>

#include <boost/filesystem.hpp>

#include <ebbrt/Context.h>
#include <ebbrt/ContextActivation.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/NodeAllocator.h>
#include <ebbrt/Runtime.h>

#include <stdio.h>

int 
main(int argc, char **argv)
{

  printf("Hello World!!!\n");

  auto bindir = boost::filesystem::system_complete(argv[0]).parent_path() /
    "../../../baremetal/build/" /
    "/hello_world_bm.elf32";

  ebbrt::Runtime runtime;
  ebbrt::Context c(runtime);
  boost::asio::signal_set sig(c.io_service_, SIGINT);
  {
    ebbrt::ContextActivation activation(c);

    // ensure clean quit on ctrl-c
    sig.async_wait([&c](const boost::system::error_code& ec,
                        int signal_number) { c.io_service_.stop(); });
#if 0
    Printer::Init().Then([bindir](ebbrt::Future<void> f) {
      f.Get();
      ebbrt::node_allocator->AllocateNode(bindir.string());
    });
#endif
  }

   c.Run();
  return 0;
}
