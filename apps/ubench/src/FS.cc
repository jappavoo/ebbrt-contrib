#include <ebbrt/Runtime.h>
#include <ebbrt/EbbAllocator.h>
#include <ebbrt/EbbRef.h>
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIOBuf.h>
#include <ebbrt/UniqueIOBuf.h>

#include <ebbrt/Debug.h>

#include "Unix.h"
#ifndef __EBBRT_BM__
#include "Cpu.h"
#endif

EBBRT_PUBLISH_TYPE(UNIX, FS);


#include <boost/crc.hpp>

#ifdef __EBBRT_BM__
#define printf ebbrt::kprintf
#endif

typedef ebbrt::IOBuf IOBuf;
typedef ebbrt::Messenger::NetworkId NetId;
 
#ifndef __EBBRT_BM__
ebbrt::Future<ebbrt::EbbRef<UNIX::FS>>
UNIX::FS::Init(std::string &&path)
{  
  struct stat sb;
  if (stat(path.c_str(), &sb) == -1) throw std::runtime_error("FS::Init stat failed");

  return ebbrt::global_id_map->Set(kRootFSId, std::move(path))
      .Then([](ebbrt::Future<void> f) {
        f.Get();
        return ebbrt::EbbRef<FS>(kRootFSId);
      });
}
#endif

UNIX::FS::FS(Root *root) : ebbrt::Messagable<UNIX::FS>(root->myId()),
			   myRoot_(root) {
#if 1
  auto fstr = myRoot_->getString();
  assert(fstr.Ready());
  std::string str = fstr.Get();
  const char *data = str.data();
  int len = str.size();
  {
    boost::crc_32_type  crc;
    crc.process_bytes( data, len );
    printf("%s: data=%p len=%d crc=0x%x (%c)\n", __PRETTY_FUNCTION__, 
	   data, len, crc.checksum(), data[0]);
  }
#endif
}

void UNIX::FS::Root::process_exit(int val)
{
#ifndef __EBBRT_BM__
    ebbrt::Cpu::Exit(val);
    return;
#else
    process_exit_msg_.val = val;
    auto msg_buf =  std::unique_ptr<ebbrt::StaticIOBuf>
      (new  ebbrt::StaticIOBuf((const uint8_t *)&process_exit_msg_,
			       sizeof(process_exit_msg_)));
    ebbrt::kprintf("sending exit to %d %x\n",
		   ebbrt::Messenger::NetworkId(ebbrt::runtime::Frontend()) == UNIX::fe,
		   ebbrt::runtime::Frontend());
    theRep_->SendMessage(UNIX::fe, std::move(msg_buf));
#endif
}

ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>>
UNIX::FS::Root::open_stream(std::string str)
{
#ifdef __EBBRT_BM__
  ebbrt::kprintf("21:%s\n", __PRETTY_FUNCTION__);
#endif

#ifndef __EBBRT_BM__
    throw std::runtime_error("FIXME: return a failed future");
#else
    if (p_!=NULL) throw std::runtime_error("YIKES: FIXME");
    p_ = new ebbrt::Promise<ebbrt::EbbRef<InputStream>>;
    auto msg_buf =  std::unique_ptr<ebbrt::StaticIOBuf>
      (new  ebbrt::StaticIOBuf((const uint8_t *)&open_stream_msg_,
			       sizeof(open_stream_msg_)));
    //strp = new std::string(std::move(str));
    auto data = str.data();
    auto len = str.length();
    auto path =  std::unique_ptr<ebbrt::StaticIOBuf>
      (new  ebbrt::StaticIOBuf((const uint8_t *)data,len));
    msg_buf->AppendChain(std::move(path));


#ifdef __EBBRT_BM__
    ebbrt::kprintf("msg_buf: Length:%d\n", msg_buf->Length());
    for (unsigned int i=0; i<msg_buf->Length(); i++) {
      ebbrt::kprintf("%02x", msg_buf->Data()[i]);
      }
    ebbrt::kprintf("\n");
    
    for (IOBuf *b=msg_buf->Next(); b->Data() != msg_buf->Data(); b=b->Next()) {
      ebbrt::kprintf("b=%p Length:%d\n", b, b->Length());
      for (unsigned int i=0; i<b->Length(); i++) {
	ebbrt::kprintf("%02x", b->Data()[i]);
      }
      ebbrt::kprintf("\n");
    }
#endif

    theRep_->SendMessage(UNIX::fe, std::move(msg_buf));
    return p_->GetFuture();
#endif
}

void UNIX::FS::Root::process_message(NetId nid, 
				     std::unique_ptr<ebbrt::IOBuf>&& buf) {

#ifdef __EBBRT_BM__
  ebbrt::kprintf("20:%s\n", __PRETTY_FUNCTION__);
#endif

  Root::Message *msg = (Root::Message *)buf->Data();
  switch (msg->type) {
  case kOPEN_STREAM:
    {
      OpenStreamMsg *m = (OpenStreamMsg *)msg;
      printf("%s: kOPEN_STREAM: Received: %d\n",
	     __PRETTY_FUNCTION__, m->type);
      buf->Advance(sizeof(OpenStreamMsg));
      for (unsigned int i=0; i<buf->Length(); i++) {
	printf("%c", ((char *)buf->Data())[i]);
      }
      printf("\n");
      auto fis = theRep_->openInputStream(std::string((const char *)buf->Data(),buf->Length()));
      fis.Then([this,nid](ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>> f) {
	  stream_id_msg_.id = f.Get();
	  auto msg_buf =  std::unique_ptr<ebbrt::StaticIOBuf> 
	    (new ebbrt::StaticIOBuf((const uint8_t *)&stream_id_msg_,
				    sizeof(stream_id_msg_)));
	  theRep_->SendMessage(nid,std::move(msg_buf));
	});
    }
    break;
    case kSTREAM_ID:
      {
	StreamIdMsg *m = (StreamIdMsg *)msg;
	printf("%s: kSTREAM_ID: Received: %d 0x%x\n", 
	       __PRETTY_FUNCTION__, m->type, m->id);
	p_->SetValue(ebbrt::EbbRef<InputStream>(m->id));
	delete(p_);
	p_ = NULL;
      }
      break;
  case kPROCESS_EXIT:
    {
      ProcessExitMsg *m = (ProcessExitMsg *)msg;
      printf("%s: kPROCESS_EXIT: Recived: %d %d\n",
	     __PRETTY_FUNCTION__, m->type, m->val);
#ifndef __EBBRT_BM__
      ebbrt::Cpu::Exit(m->val);
#endif
    }
    break;
  default:
    printf("%s: ERROR: Received: %d\n", 
	   __PRETTY_FUNCTION__, msg->type);
  }
}

void UNIX::FS::ReceiveMessage(ebbrt::Messenger::NetworkId nid, 
			      std::unique_ptr<ebbrt::IOBuf>&& buf) {
  
  myRoot_->process_message(nid, std::move(buf));
};


UNIX::FS::Root::Root(ebbrt::EbbId id) : myId_(id), theRep_(NULL), p_(NULL) {
#ifdef __EBBRT_BM__
  ebbrt::kprintf("5:%s\n", __PRETTY_FUNCTION__);
#endif

  open_stream_msg_.type = kOPEN_STREAM;
  stream_id_msg_.type = kSTREAM_ID;
  process_exit_msg_.type = kPROCESS_EXIT;
  data_ = ebbrt::global_id_map->Get(id).Share();
}

UNIX::FS *
UNIX::FS::Root::getRep_BIN() {
  std::lock_guard<std::mutex> lock{lock_};
  if (theRep_) return theRep_; // implicity drop lock

#ifdef __EBBRT_BM__
  ebbrt::kprintf("11:%s\n", __PRETTY_FUNCTION__);
#endif

  lock_.unlock();  // drop lock
  data_.Block();    // if necessary wait for our root data to be ready 
                    // blocks as needed but when done the data is there
#ifdef __EBBRT_BM__
  ebbrt::kprintf("12:%s\n", __PRETTY_FUNCTION__);
#endif
  data_.Get();      // do a get on the future to ensure
                    // that we throw any errors that occurred during 
                    // getting the data
  lock_.lock();     // reaquire lock
  if (theRep_ == NULL)  { 
    // now that we are ready create the rep if necessary
    theRep_ = new FS(this);
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
UNIX::FS & 
UNIX::FS::HandleFault(ebbrt::EbbId id) {
#ifdef __EBBRT_BM__
  ebbrt::kprintf("10:%s\n", __PRETTY_FUNCTION__);
#endif
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
      ebbrt::EbbRef<FS>::CacheRef(id, rep);
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

#ifndef __EBBRT_BM__
ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>>
UNIX::FS::openInputStream(std::string path)
{
  int fd;
  //FIXME:  added path prefix adjustments here
  {
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1) throw std::runtime_error(__PRETTY_FUNCTION__);
  }

  fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("open");
    throw std::runtime_error(__PRETTY_FUNCTION__);
  }
  return InputStream::Create(fd);
}
#else 

ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>>  
UNIX::FS::openInputStream(std::string path) 
{
#ifdef __EBBRT_BM__
  ebbrt::kprintf("2:%s\n", __PRETTY_FUNCTION__);
#endif
  return myRoot_->open_stream(path);
}

#endif

void
UNIX::FS::processExit(int val)
{
#ifdef __EBBRT_BM__
  ebbrt::kprintf("2:%s\n", __PRETTY_FUNCTION__);
#endif
  return myRoot_->process_exit(val);
}


void UNIX::FS::destroy() { ebbrt::kabort(); }
