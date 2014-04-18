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


#include <ebbrt/Buffer.h>
#include "../../src/Unix.h"

typedef ebbrt::EventManager ebbrtEM;
typedef ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,size_t avail)> ebbrtHandler;
typedef boost::asio::posix::stream_descriptor asioSD;
typedef boost::system::error_code boostEC;
typedef ebbrt::IOBuf IOBuf;

// also could use null_buffers() with async_read_some(null_buffers(), handler);
namespace UNIX {
  class InputStream;
  InputStream *stdin;


class InputStream {
public:
  static const size_t kBufferSize = 1024;
private:
  int fd_;
  size_t len_;
  asioSD::bytes_readable asioAvail_;
  asioSD *sd_;
  bool doRead_;
  char buffer_[kBufferSize];
  std::size_t count_;
  ebbrtHandler consumer_;

  void async_read_some()   {
    if (sd_ == NULL || doRead_==false) return;
    sd_->async_read_some(
		       boost::asio::buffer(buffer_,kBufferSize),  
		       ebbrtEM::WrapHandler([this]
					    (
					     const boostEC& ec,
					     std::size_t size
					     ) 
					    {
					      size_t avail;
					      if (ec) {printf("ERROR: on Stream"); return;}
					      {
						boostEC cec;
						sd_->io_control(asioAvail_,cec);
						if (cec) { 
						  printf("ERROR: on readable"); return;
						}
						avail = asioAvail_.get();
					      }
					      std::unique_ptr<IOBuf> iobuf = 
						std::unique_ptr<IOBuf>(new IOBuf((void*)buffer_, 
										 size, 
										 [](void*){}));
					      consumer_(std::move(iobuf),avail);
					      count_ += size;
					      if (doRead_==true) async_read_some(); 
					     }
					     )
			);
  }


public:
  
  InputStream(int fd) : fd_(fd),len_(0),sd_(NULL),doRead_(false),count_(0) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) throw std::runtime_error("stat");
    printf("File type:                ");

    switch (sb.st_mode & S_IFMT) {
    case S_IFBLK:  printf("block device\n");           
    case S_IFCHR:  if ((sb.st_mode & S_IFMT) ==  S_IFCHR)  printf("character device\n");       
    case S_IFSOCK: if ((sb.st_mode & S_IFMT) ==  S_IFSOCK) printf("socket\n");                 
    case S_IFIFO:  if ((sb.st_mode & S_IFMT) ==  S_IFIFO)  printf("FIFO/pipe\n");
      sd_ = new asioSD(ebbrt::active_context->io_service_,fd);
      break;
    case S_IFDIR:  if ((sb.st_mode & S_IFMT) ==  S_IFDIR) printf("directory\n");
    case S_IFLNK:  if ((sb.st_mode & S_IFMT) ==  S_IFLNK) printf("symlink\n");                 
    case S_IFREG:  if ((sb.st_mode & S_IFMT) ==  S_IFREG) printf("regular file\n");            
    default:       
      len_ = sb.st_size;
    }
  }
  
  void  async_read_stop() { 
    if (doRead_==false) return;
    printf("\nStopping Streaming Read\n"); 
    doRead_=false; 
  }

  void async_read_start(ebbrtHandler consumer) {
    if (doRead_==true) return;
    doRead_=true;
    consumer_ = std::move(consumer);
    printf("\nStarting Stream Read\n");
    if (sd_ == NULL) {
      size_t n;
      while (count_ < len_) {
	n = read(fd_, buffer_, kBufferSize);
	if (n<0) throw std::runtime_error("read of regular file failed");
	std::unique_ptr<IOBuf> iobuf = 
	  std::unique_ptr<IOBuf>(new IOBuf((void*)buffer_, n, [](void*){}));
	consumer_(std::move(iobuf),len_-(count_+n));
	count_ += n;
      }
    } else {
      async_read_some(); 
    }
  }

  static void Init() {
    int fd = ::dup(STDIN_FILENO);
    UNIX::stdin = new InputStream(fd);
  }
};
  

};


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

    {
      UNIX::InputStream::Init();
      UNIX::stdin->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
	  size_t n = write(STDOUT_FILENO, buf->Data(), buf->Length()); 
	  if (n<=0) throw std::runtime_error("write to stdout failed");
	});
    }
 #if 0
    ebbrt::node_allocator->AllocateNode(bindir.string());
#endif
  }

  c.Run();
  return 0;
}
