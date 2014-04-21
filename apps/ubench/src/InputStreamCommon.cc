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

namespace UNIX {

  void InputStream::ReceiveMessage(ebbrt::Messenger::NetworkId nid, 
				   std::unique_ptr<ebbrt::IOBuf>&& buf) {
  };
  
  InputStream::Root::Root(ebbrt::EbbId id) : myId_(id), 
					     theRep_(NULL),
					     members_(NULL) {
    stream_start_msg.type = kSTREAM_START;
    stream_data_msg.type  = kSTREAM_DATA;
    stream_stop_msg.type  = kSTREAM_STOP;
    
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
