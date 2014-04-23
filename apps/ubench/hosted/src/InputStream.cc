#include <signal.h>

#include <array>

#include <boost/filesystem.hpp>

#include <ebbrt/EbbId.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/Runtime.h>
#include <ebbrt/Debug.h>
#include <ebbrt/EbbAllocator.h>
#include <stdio.h>


#include <ebbrt/Buffer.h>
#include "../../src/Unix.h"

typedef ebbrt::EventManager ebbrtEM;
typedef ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>&&,
				    size_t avail)> ebbrtHandler;
typedef boost::asio::posix::stream_descriptor asioSD;
typedef boost::system::error_code boostEC;
typedef ebbrt::IOBuf IOBuf;
typedef ebbrt::Messenger::NetworkId NetId;

// also could use null_buffers() with async_read_some(null_buffers(), handler);
namespace UNIX {

  ebbrt::Future<ebbrt::EbbRef<InputStream>> InputStream::Create(ebbrt::EbbId id, int fd)
  {
    RootMembers members;
    members.fd = fd;
    members.len=0;
    members.type=RootMembers::kUNKNOWN;

    struct stat sb;
    if (fstat(members.fd, &sb) == -1) throw std::runtime_error("stat");
    printf("id: %d, fd: %d -  File type: ", id, fd);

    switch (sb.st_mode & S_IFMT) {
    case S_IFBLK: { 
      members.type = RootMembers::kBLOCK_DEV;
      printf("block device\n");
    }
      break;
    case S_IFCHR: {
      members.type = RootMembers::kCHAR_DEV;
      printf("character device\n");       
    }
      break;
    case S_IFSOCK: {
      members.type = RootMembers::kSOCKET;
      printf("socket\n");                 
    }
      break;
    case S_IFIFO: {
      members.type = RootMembers::kPIPE;
      printf("FIFO/pipe\n");
    }
      break;
    case S_IFREG: {
      members.type = RootMembers::kFILE;
      members.len = sb.st_size;                 
      printf("regular file\n");
    }
      break;
    case S_IFDIR: {
      members.type = RootMembers::kDIR;
      printf("directory\n");
    }
      break;
    case S_IFLNK: {
      members.type = RootMembers::kSYMLINK;
      printf("symlink\n"); 
    }
      break;
    default:
      members.type = RootMembers::kUNKNOWN;
      printf("UNKNOWN");
      throw std::runtime_error("unsupported file type");
    }

    std::string str = std::move(RootMembers::toString(&members));
    return ebbrt::global_id_map->Set(id, std::move(str))
         .Then([id](ebbrt::Future<void> f) {
                 f.Get();
                 return ebbrt::EbbRef<InputStream>(id);
              });
  }

  ebbrt::Future<ebbrt::EbbRef<InputStream>> InputStream::Create(int fd)
  {
    auto id = ebbrt::ebb_allocator->Allocate();
    return Create(id, fd);
  }

  ebbrt::Future<ebbrt::EbbRef<InputStream>> InputStream::InitSIn()
  {
    int fd = ::dup(STDIN_FILENO);
    return Create(kSInId, fd);
  }



  void InputStream::async_read_some()   {
    if (doRead_==false) return;
    if (sd_) {
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
						  if (cec) {printf("ERROR: on readable"); return;}
						  avail = asioAvail_.get();
						  }
						  std::unique_ptr<IOBuf> iobuf = 
						    std::unique_ptr<IOBuf>(new IOBuf((void*)buffer_, 
										     size, 
										     [this,size](void*p){
										       // printf("FREE %p\n",p);
										       count_ += size;
										     if (doRead_==true) async_read_some(); 
										     }));
						  consumer_(std::move(iobuf),avail);
						}
						)
			 );
    } else {
      size_t n;
      if (count_ < len_) {
	n = read(fd_, buffer_, kBufferSize);
	if (n<0) throw std::runtime_error("read of regular file failed");
	std::unique_ptr<IOBuf> iobuf = 
	  std::unique_ptr<IOBuf>(new IOBuf((void*)buffer_, n, 
					   [this,n](void*p){
					     // printf("free %p\n",p);
					     count_ += n;
					     if (count_ == len_) {
					       printf("\n %s:%d * CLOSE STREAM LOGIC HERE *\n",
						      __PRETTY_FUNCTION__, __LINE__);
					       
					       consumer_(
							 IOBuf::WrapBuffer(NULL,0),
							 0);
					     } else {
					       if (doRead_==true) async_read_some(); 
					     }
					   }));
	consumer_(std::move(iobuf),len_-(count_+n));
	
      }
    }
  }
  
  InputStream::InputStream(Root *root) : 
 				      ebbrt::Messagable<InputStream>(root->myId()),
				      myRoot_(root), 
				      sd_(NULL),
				      doRead_(false),
				      count_(0) 
  {
    fd_ = myRoot_->fd();
    len_ = myRoot_->len();
    switch (myRoot_->type()){
    case RootMembers::kBLOCK_DEV:
    case RootMembers::kCHAR_DEV:
    case RootMembers::kSOCKET:
    case RootMembers::kPIPE: {
      sd_ = new asioSD(ebbrt::active_context->io_service_,fd_);
    }
      break;
    case RootMembers::kFILE:
      break;
    default:
      throw std::runtime_error("UNSUPPORTED FILE TYPE");
    }
  }
  

  void InputStream::async_read_start(ebbrtHandler consumer) {
    if (doRead_==true) return;
    doRead_=true;
    consumer_ = std::move(consumer);
    printf("\nStarting Stream Read\n");
    async_read_some(); 
  }
  
};


