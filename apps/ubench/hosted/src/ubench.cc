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

#include "../../src/ubenchCommon.h"
#include "../../src/Unix.h"

#define __BACKEND_TEST__

int 
main(int argc, char **argv)
{
  auto bindir = boost::filesystem::system_complete(argv[0]).parent_path() /
    "bm/ubench.elf32";

  ebbrt::Runtime runtime;
  ebbrt::Context c(runtime);
  boost::asio::signal_set sig(c.io_service_, SIGINT);

  { // scope to trigger automatic deactivate on exit
    ebbrt::ContextActivation activation(c);

    // ensure clean quit on ctrl-c
    sig.async_wait([&c](const boost::system::error_code& ec,
                        int signal_number) { c.io_service_.stop(); });

    UNIX::Init(argc, (const char **)argv);
    AppMain();

#ifndef NO_UNIX_IO
    printf("Enter lines of characters ('.' to terminte):\n");
    UNIX::sin->async_read_start([bindir](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
	if (buf->Data()==NULL) {  printf("Stream EOF\n"); return; }
	do {
	  size_t n = write(STDOUT_FILENO, buf->Data(), buf->Length()); 
	  if (n<=0) throw std::runtime_error("write to stdout failed");
	  if (buf->Data()[0] == '.') {
	    UNIX::sin->async_read_stop();
	    printf("open and dump /etc/passwd a stream"); 
	    auto fistream = UNIX::root_fs->openInputStream("/etc/passwd");
	    fistream.Then([bindir](ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>> fis) {
		
		ebbrt::EbbRef<UNIX::InputStream> is = fis.Get();		  
		is->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
		    if (buf->Data() == NULL) { printf("Stream EOF\n"); return; }
		    do {
		      size_t n = write(STDOUT_FILENO, buf->Data(), buf->Length()); 
		      if (n<=0) throw std::runtime_error("write to stdout failed");
		    } while(buf->Pop()!=nullptr);
		  });
#ifdef __BACKEND_TEST__
		ebbrt::node_allocator->AllocateNode(bindir.string());
#endif
	      });
	    break;
	  }
	} while(buf->Pop()!=nullptr);
    });
#else
#ifdef __BACKEND_TEST__
  ebbrt::node_allocator->AllocateNode(bindir.string());
#endif
#endif
  }

  c.Run();
  return 0;
}
