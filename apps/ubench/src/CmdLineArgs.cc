#include <ebbrt/Runtime.h>
#include <ebbrt/EbbAllocator.h>
#include <ebbrt/EbbRef.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/Debug.h>

#include "Unix.h"

ebbrt::EbbRef<UNIX::CmdLineArgs>
UNIX::CmdLineArgs::Create(int argc, const char **argv)
{
  int n=0;
  for (int i=0; i<argc; i++) {
    n+=strlen(argv[i])+1;
  }
  auto root = new Root();
  root->theRep_ = 0;
  root->data_ = ebbrt::IOBuf::Create(n);
  uint8_t *bytes = root->data_->WritableData();
  n=0;
  for (int i=0; i<argc; i++) {
    strcpy((char *)&bytes[n],argv[i]);
    n+=strlen(argv[i])+1;
  }  
  auto id = ebbrt::ebb_allocator->AllocateLocal();
  // Insert is Atomic
  ebbrt::local_id_map->Insert(std::make_pair(id, root));
  return ebbrt::EbbRef<UNIX::CmdLineArgs>(id);
}

UNIX::CmdLineArgs::CmdLineArgs(Root *root) : myRoot_(root) 
{
  // add code here to initialized argc and argv from root data pointer
  const uint8_t *data =  root->data_->Data();
  size_t len = root->data_->Capacity();
  int i,j;

  argc_ = 0;

  for (i=0; i<len; i++) if (data[i] == 0) argc_++;

  argv_vector.reserve(argc_);
  for (i=0,j=1,argv_vector.emplace_back((char *)data); j < argc_; i++) {
    if (data[i] == 0) { argv_vector.emplace_back((char *)&(data[i+1]));  j++; }  
  }
#if 0  
  argv_ = (char **)malloc(sizeof(char *) * argc_);
  for (i=0,j=1,argv_[0]=(char *)data; j<argc_; i++) {

  }
#endif
}

UNIX::CmdLineArgs & 
UNIX::CmdLineArgs::HandleFault(ebbrt::EbbId id) {
  // const accessor acquires read long on entry on success
  ebbrt::LocalIdMap::ConstAccessor accessor; 
  auto found = ebbrt::local_id_map->Find(accessor, id);
  intptr_t ZERO = 0;
  intptr_t ONE  = 1;

  if (!found)
      throw std::runtime_error("Failed to find root for UNIX::CmdLineArgs");
  
  auto root = boost::any_cast<Root *>(accessor->second);
  if (root->theRep_.load() == ZERO) {
    if (root->theRep_.compare_exchange_strong(ZERO, ONE)) {
      root->theRep_ = reinterpret_cast<intptr_t>(new CmdLineArgs(root));
    }  else {
	while (root->theRep_.load()==ONE);
    }
  } 
  CmdLineArgs *theRep = reinterpret_cast<CmdLineArgs *>(root->theRep_.load());
  ebbrt::EbbRef<CmdLineArgs>::CacheRef(id, *theRep);
  return *(theRep);
}


void UNIX::CmdLineArgs::destroy() { ebbrt::kabort(); }
