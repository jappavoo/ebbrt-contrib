#include <signal.h>

#include <array>

#include <boost/filesystem.hpp>

#include <ebbrt/EbbId.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/Runtime.h>
#include <ebbrt/Debug.h>
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

InputStream::Root::Root(ebbrt::EbbId id) : myId_(id), theRep_(NULL), members_(NULL) {
    data_ = ebbrt::global_id_map->Get(id).Share();
  }

  InputStream *
  InputStream::Root::getRep_BIN() {
    std::lock_guard<std::mutex> lock{lock_};
    if (theRep_) return theRep_; // implicity drop lock

    lock_.unlock();   // drop lock
    data_.Block();    // if necessary wait for our root data to be ready 
                      // blocks as needed but when done the data is there
    data_.Get();      // do a get on the future to ensure
                      // that we throw any errors that occurred during 
                      // getting the data
    lock_.lock();     // reaquire lock
    if (theRep_ == NULL)  {
      // finish setup the root
      members_ = (RootMembers *)data_.Get().data();
      // now that we are ready create the rep if necessary
      theRep_ = new InputStream(this);
    }
    return theRep_;      // implicity drop lock
  }


// I am modeling this based on the following example from TBB doc
// http://www.threadingbuildingblocks.org/docs/help/reference/containers_overview/concurrent_hash_map_cls/concurrent_operations.htm
//  extern tbb::concurrent_hash_map<Key,Resource,HashCompare> Map;
// void ConstructResource( Key key ) {
//        accessor acc;
//        if( Map.insert(acc,key) ) {
//                // Current thread inserted key and has exclusive access.
//                ...construct the resource here...
//        }
//        // Implicit destruction of acc releases lock
// }

// void DestroyResource( Key key ) {
//        accessor acc;
//        if( Map.find(acc,key) ) {
//                // Current thread found key and has exclusive access.
//                ...destroy the resource here...
//                // Erase key using accessor.
//                Map.erase(acc);
//        }
// }
InputStream & 
InputStream::HandleFault(ebbrt::EbbId id) {
 retry:
  {
    ebbrt::LocalIdMap::ConstAccessor rd_access;
    if (ebbrt::local_id_map->Find(rd_access, id)) {
      // COMMON: "HOT PATH": NODE HAS A ROOT ESTABLISHED
      // EVERYONE MUST EVENTUALLY GET HERE OR THROW AND EXCEPTION
      // Found the root get a rep and return
      auto &root = *boost::any_cast<Root *>(rd_access->second);
      rd_access.release();  // drop lock
      // NO LOCKS;
      auto &rep = *(root.getRep_BIN());   // this may block internally
      ebbrt::EbbRef<InputStream>::CacheRef(id, rep);
      // The sole exit path out of handle fault
      return rep; // drop rd_access lock by exiting outer scope
    }
  } 
  // failed to find root: NO LOCK held and we need to establish a root for this node
  ebbrt::LocalIdMap::Accessor wr_access;
  if (ebbrt::local_id_map->Insert(wr_access, id)) {
    // WRITE_LOCK HELD:  THIS HOLDS READERS FROM MAKING PROGESS
    //                   ONLY ONE WRITER EXITS
    Root *root = new Root(id);
    wr_access->second = root;
    wr_access.release(); // WE CAN NOW DROP THE LOCK and retry as a normal reader
  }
  // NO LOCKS HELD
  // if we failed to insert then someone else must have beat us
  // and is the writer and will eventuall fill in the entry.
  // all we have to do is retry a read on the entry
  goto retry;
}


  void InputStream::destroy() { ebbrt::kabort(); }



  ebbrt::Future<ebbrt::EbbRef<InputStream>> InputStream::InitSIn()
  {
    RootMembers members;
    members.fd = ::dup(STDIN_FILENO);
    members.len=0;
    members.type=RootMembers::UNKNOWN;

    struct stat sb;
    if (fstat(members.fd, &sb) == -1) throw std::runtime_error("stat");
    printf("Standard Input: File type: ");

    switch (sb.st_mode & S_IFMT) {
    case S_IFBLK: { 
      members.type = RootMembers::BLOCK_DEV;
      printf("block device\n");
    }
      break;
    case S_IFCHR: {
      members.type = RootMembers::CHAR_DEV;
      printf("character device\n");       
    }
      break;
    case S_IFSOCK: {
      members.type = RootMembers::SOCKET;
      printf("socket\n");                 
    }
      break;
    case S_IFIFO: {
      members.type = RootMembers::PIPE;
      printf("FIFO/pipe\n");
    }
      break;
    case S_IFREG: {
      members.type = RootMembers::FILE;
      members.len = sb.st_size;                 
      printf("regular file\n");
    }
      break;
    case S_IFDIR: {
      members.type = RootMembers::DIR;
      printf("directory\n");
    }
      break;
    case S_IFLNK: {
      members.type = RootMembers::SYMLINK;
      printf("symlink\n"); 
    }
      break;
    default:
      members.type = RootMembers::UNKNOWN;
      printf("UNKNOWN");
      throw std::runtime_error("unsupported file type");
    }

    std::string str = std::move(RootMembers::toString(&members));
    return ebbrt::global_id_map->Set(kSInId, std::move(str))
         .Then([](ebbrt::Future<void> f) {
                 f.Get();
                 return ebbrt::EbbRef<InputStream>(kSInId);
              });
  }


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
						  if (cec) {printf("ERROR: on readable"); return;}
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
  
  
  InputStream::InputStream(Root *root) : myRoot_(root), 
					 sd_(NULL),doRead_(false),count_(0) 
  {
    fd_ = myRoot_->fd();
    len_ = myRoot_->len();
    switch (myRoot_->type()){
    case RootMembers::BLOCK_DEV:
    case RootMembers::CHAR_DEV:
    case RootMembers::SOCKET:
    case RootMembers::PIPE: {
      sd_ = new asioSD(ebbrt::active_context->io_service_,fd_);
    }
      break;
    case RootMembers::FILE:
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


