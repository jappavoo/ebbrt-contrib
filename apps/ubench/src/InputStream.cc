#include <signal.h>

#include <array>

#include <boost/filesystem.hpp>

#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/Runtime.h>
#include <stdio.h>


#include <ebbrt/Buffer.h>
#include "Unix.h"

typedef ebbrt::EventManager ebbrtEM;
typedef ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,size_t avail)> ebbrtHandler;
typedef boost::asio::posix::stream_descriptor asioSD;
typedef boost::system::error_code boostEC;
typedef ebbrt::IOBuf IOBuf;


// also could use null_buffers() with async_read_some(null_buffers(), handler);
namespace UNIX {

  InputStream *stdin;

  void InputStream::async_read_some()   {
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


  InputStream::InputStream(int fd) : fd_(fd),len_(0),sd_(NULL),doRead_(false),count_(0) {
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
  

  void InputStream::async_read_start(ebbrtHandler consumer) {
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

};


