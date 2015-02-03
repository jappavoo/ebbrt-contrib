#include <ebbrt/Runtime.h>
#include <ebbrt/EbbAllocator.h>
#include <ebbrt/EbbRef.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>

#include <ebbrt/Debug.h>

#include "Unix.h"

#ifndef __EBBRT_BM__
ebbrt::Future<ebbrt::EbbRef<UNIX::CmdLineArgs>>
UNIX::CmdLineArgs::Init(int argc, const char **argv)
{
  std::string str;

  for (int i=0; i<argc; i++) {
    str.append(argv[i]);
    str.push_back(0);
  }  
  return ebbrt::global_id_map->Set(kCmdLineArgsId, std::move(str))
      .Then([](ebbrt::Future<void> f) {
        f.Get();
        return ebbrt::EbbRef<CmdLineArgs>(kCmdLineArgsId);
      });
}
#endif

UNIX::CmdLineArgs::CmdLineArgs(Root *root) : myRoot_(root) 
{
  // add code here to initialized argc and argv from root data pointer
  auto fstr = myRoot_->getString();
  assert(fstr.Ready());
  std::string str = fstr.Get();
  const char *data = str.data();
  int len = str.size();
  int i,j;

  argc_ = 0;
  for (i=0; i<len; i++) if (data[i] == 0) argc_++;

  argv_vector.reserve(argc_+1); // leave space for all arguments and null termination

  for (i=0,j=1,argv_vector.emplace_back((char *)data); j < argc_; i++) {
    if (data[i] == 0) { argv_vector.emplace_back((char *)&(data[i+1]));  j++; }  
  }
  // null terminate 
  argv_vector.emplace_back((char *)0);
}

UNIX::CmdLineArgs::Root::Root(ebbrt::EbbId id) : myId_(id), theRep_(NULL)  {
  data_ = ebbrt::global_id_map->Get(id).Share();
}

UNIX::CmdLineArgs *
UNIX::CmdLineArgs::Root::getRep_BIN() {
  std::lock_guard<std::mutex> lock{lock_};
  if (theRep_) return theRep_; // implicity drop lock

  lock_.unlock();  // drop lock
  data_.Block();    // if necessary wait for our root data to be ready 
                    // blocks as needed but when done the data is there
  data_.Get();      // do a get on the future to ensure
                    // that we throw any errors that occurred during 
                    // getting the data
  lock_.lock();     // reaquire lock
  if (theRep_ == NULL)  { 
    // now that we are ready create the rep if necessary
    theRep_ = new CmdLineArgs(this);
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
UNIX::CmdLineArgs & 
UNIX::CmdLineArgs::HandleFault(ebbrt::EbbId id) {
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
      ebbrt::EbbRef<CmdLineArgs>::CacheRef(id, rep);
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


void UNIX::CmdLineArgs::destroy() { ebbrt::kabort(); }
