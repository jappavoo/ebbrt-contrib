#include <ebbrt/Runtime.h>
#include <ebbrt/EbbAllocator.h>
#include <ebbrt/EbbRef.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>

#include <ebbrt/Debug.h>

#include "Unix.h"

#ifndef __EBBRT_BM__
ebbrt::Future<ebbrt::EbbRef<UNIX::Environment>>
UNIX::Environment::Init()
{
  std::string str;

  for (int i=0; ::environ[i]!=NULL; i++) {
    str.append(::environ[i]);
    str.push_back(0);
  }  
  return ebbrt::global_id_map->Set(kEnvironmentId, std::move(str))
      .Then([](ebbrt::Future<void> f) {
        f.Get();
        return ebbrt::EbbRef<Environment>(kEnvironmentId);
      });
}
#endif

#ifndef __EBBRT_BM__ 
UNIX::Environment::Environment(Root *root) : myRoot_(root), environ_(NULL) {}
#else
UNIX::Environment::Environment(Root *root) : myRoot_(root), environ_(NULL)
{
  // add code here to initialized argc and argv from root data pointer
  auto fstr = myRoot_->getString();
  assert(fstr.Ready());
  std::string str = fstr.Get();
  const char *data = str.data();
  int len = str.size();
  int i,j,numvar;

  numvar = 0;
  for (i=0; i<len; i++) if (data[i] == 0) numvar++;

  environ_ = (char **)malloc(sizeof(char *)*numvar);
  environ_[numvar] = NULL;

  for (i=0,j=1,environ_[0]=(char *)data; j < numvar; i++) {
    if (data[i] == 0) { environ_[j]=(char *)&(data[i+1]);  j++; }  
  }
  ::environ = environ_;
}
#endif

UNIX::Environment::Root::Root() : theRep_(NULL) {
 data_ = ebbrt::global_id_map->Get(kEnvironmentId).Share();
}

UNIX::Environment *
UNIX::Environment::Root::getRep_BIN() {
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
    theRep_ = new Environment(this);
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
UNIX::Environment & 
UNIX::Environment::HandleFault(ebbrt::EbbId id) {
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
      ebbrt::EbbRef<Environment>::CacheRef(id, rep);
      // The sole exit path out of handle fault
      return rep; // drop rd_access lock by exiting outer scope
    }
  } 
  // failed to find root: NO LOCK held and we need to establish a root for this node
  ebbrt::LocalIdMap::Accessor wr_access;
  if (ebbrt::local_id_map->Insert(wr_access, id)) {
    // WRITE_LOCK HELD:  THIS HOLDS READERS FROM MAKING PROGESS
    //                   ONLY ONE WRITER EXITS
    Root *root = new Root();
    wr_access->second = root;
    wr_access.release(); // WE CAN NOW DROP THE LOCK and retry as a normal reader
  }
  // NO LOCKS HELD
  // if we failed to insert then someone else must have beat us
  // and is the writer and will eventuall fill in the entry.
  // all we have to do is retry a read on the entry
  goto retry;
}


void UNIX::Environment::destroy() { ebbrt::kabort(); }
