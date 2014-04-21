#include <signal.h>

#include <array>

#include <ebbrt/EbbId.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/Runtime.h>
#include <ebbrt/Debug.h>
#include <stdio.h>


#include <ebbrt/Buffer.h>
#include <ebbrt/Console.h>
#include "../../src/Unix.h"


typedef ebbrt::EventManager ebbrtEM;
typedef ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>&&,
				    size_t avail)> ebbrtHandler;
typedef ebbrt::IOBuf IOBuf;

// also could use null_buffers() with async_read_some(null_buffers(), handler);
namespace UNIX {

  void InputStream::async_read_some() {}  

  InputStream::InputStream(Root *root) : 
 				      ebbrt::Messagable<InputStream>(root->myId()),
				      myRoot_(root), 
				      doRead_(false),
				      count_(0)
  {
    fd_ = myRoot_->fd();
    len_ = myRoot_->len();
    switch (myRoot_->type()){
    case RootMembers::kBLOCK_DEV:
    case RootMembers::kCHAR_DEV:
    case RootMembers::kSOCKET:
    case RootMembers::kPIPE: 
      //  ebbrt::kprintf("fd is a real stream");
      break;
    case RootMembers::kFILE:
      // ebbrt::kprintf("fd is a real file");
      break;
    default:
      throw std::runtime_error("UNSUPPORTED FILE TYPE");
    }
  }
  

  void InputStream::async_read_start(ebbrtHandler consumer) {
    if (doRead_==true) return;
    doRead_=true;
    consumer_ = std::move(consumer);
    ebbrt::kprintf("\nStarting Stream Read: fd:%d len:%ld type=%d myId=0x%x\n", 
		   fd_, len_, myRoot_->type(), myRoot_->myId());
    myRoot_->start_stream();
  }

  

};


