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
#include "Unix.h"

EBBRT_PUBLISH_TYPE(UNIX, InputStream);

typedef ebbrt::IOBuf IOBuf;
typedef ebbrt::Messenger::NetworkId NetId;

#ifdef __EBBRT_BM__
#define printf ebbrt::kprintf
#endif

namespace UNIX {

  void InputStream::Root::start_stream()
  {
    if (UNIX::fe == ebbrt::messenger->LocalNetworkId()) return;
    else {
      std::unique_ptr<IOBuf> msg_buf =  
	std::unique_ptr<IOBuf>(new IOBuf((void*)&stream_start_msg_, 
					 sizeof(stream_start_msg_),
					 [](void*){}));
      theRep_->SendMessage(UNIX::fe, std::move(msg_buf));
    }
  }

  void InputStream::Root::stop_stream()
  {
    if (UNIX::fe == ebbrt::messenger->LocalNetworkId()) return;
    else {
      std::unique_ptr<IOBuf> msg_buf =  
	std::unique_ptr<IOBuf>(new IOBuf((void*)&stream_stop_msg_, 
					 sizeof(stream_stop_msg_),
					 [](void*){}));
      theRep_->SendMessage(UNIX::fe, std::move(msg_buf));
    }
  }

  void InputStream::Root::process_message(NetId nid, 
					  std::unique_ptr<ebbrt::IOBuf>&& buf) {
    Root::Message *msg = (Root::Message *)buf->Data();
    switch (msg->type) {
    case kSTREAM_START:
      {
	StreamStartMsg *m = (StreamStartMsg *)msg;
	printf("%s: kSTREAM_START: Received: %d\n",
	       __PRETTY_FUNCTION__, m->type);
	theRep_->async_read_start(
				  [this,nid]
				  (std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) 
				  {
				    stream_data_msg_.avail = avail;
				    std::unique_ptr<IOBuf> msg_buf =  
				      std::unique_ptr<IOBuf>(new IOBuf((void*)&stream_data_msg_, 
								     sizeof(stream_data_msg_),
								     [](void*){}));
				    //printf("type:%d avail:%ld\n", 
				    //  stream_data_msg_.type,
				    //  stream_data_msg_.avail);
				    msg_buf->AppendChain(std::move(buf));
				    theRep_->SendMessage(nid, std::move(msg_buf));
				  });
      }
      break;
    case kSTREAM_DATA:
      {
	StreamDataMsg *m = (StreamDataMsg *)msg;
	//	printf("%s: kSTREAM_DATA: Received: %d: avail=%ld \n",
	//       __PRETTY_FUNCTION__, m->type, m->avail);
	buf->Advance(sizeof(StreamDataMsg));
	theRep_->consumer_(std::move(buf), m->avail); // FIXME: DON'T LIKE THIS
      }
      break;
    case kSTREAM_STOP:
      {
	// StreamStopMsg *m = (StreamStopMsg *)msg;
	// printf("%s: kSTREAM_STOP: Received: %d\n", 
	//         __PRETTY_FUNCTION__, m->type);
	theRep_->async_read_stop();
      }
      break;

    default:
	printf("%s: ERROR: Received: %d\n", 
	       __PRETTY_FUNCTION__, msg->type);
    }
  }

  void InputStream::ReceiveMessage(ebbrt::Messenger::NetworkId nid, 
				   std::unique_ptr<ebbrt::IOBuf>&& buf) {

    myRoot_->process_message(nid, std::move(buf));
  };
  
  InputStream::Root::Root(ebbrt::EbbId id) : myId_(id), 
					     theRep_(NULL),
					     members_(NULL) {
    stream_start_msg_.type = kSTREAM_START;
    stream_data_msg_.type  = kSTREAM_DATA;
    stream_stop_msg_.type  = kSTREAM_STOP;
    
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
  
};
