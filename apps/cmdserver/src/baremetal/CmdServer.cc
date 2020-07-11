//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#include "CmdServer.h"
#include "Printer.h"
#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/UniqueIOBuf.h>
#include <ebbrt/native/Multiboot.h>
#include <ebbrt/StaticIOBuf.h>
#include <ebbrt/UniqueIOBuf.h>
#include <ebbrt/EventManager.h>

// uncomment to get lots of console messages to trace operation
//#define DEBUG_WITH_KPRINTF

// EbbRT port allocator seems to have a bug... using this to ensure that every new connection
// for send uses a different port starting at the value
#define PORT_HACK

#ifdef PORT_HACK
#define START_LPORT 50000
#endif

#ifdef DEBUG_WITH_KPRINTF

#define DPrint(str) {							\
    uint32_t eid=ebbrt::event_manager->GetEventId();			\
    ebbrt::kprintf("%u: %s: %p:" str, eid, __PRETTY_FUNCTION__,this);		\
  }

#define DPrintf(fmt, ...) {						\
    uint32_t eid=ebbrt::event_manager->GetEventId();			\
    ebbrt::kprintf("%u: %s: %p" fmt, eid, __PRETTY_FUNCTION__, this,__VA_ARGS__);	\
}

#else

#define DPrint(str)
#define DPrintf(...)

#endif


namespace ebbrt {
  class StaticIOBufWithDisposalOwner {
  public:
    StaticIOBufWithDisposalOwner(const char* ptr, size_t len, 
    ebbrt::MovableFunction<void()> disposal) noexcept {
      ptr_ = (const uint8_t *)ptr;
      capacity_ = len;
      disposal_ = std::move(disposal);
    }
    StaticIOBufWithDisposalOwner(StaticIOBufWithDisposalOwner&&) = default;
    StaticIOBufWithDisposalOwner& operator=(StaticIOBufWithDisposalOwner&&) = default;

    ~StaticIOBufWithDisposalOwner() {
      disposal_();
    }
    const uint8_t* Buffer() const { return ptr_; }
    size_t Capacity() const { return capacity_; }
    
  private:
    const uint8_t* ptr_;
    size_t capacity_;
    ebbrt::MovableFunction<void()> disposal_;
  };

  typedef IOBufBase<StaticIOBufWithDisposalOwner> StaticIOBufWithDisposal;  
}
typedef ebbrt::StaticIOBufWithDisposal IOBuf;

CmdServer::CmdServer()  {}

CmdServer& CmdServer::HandleFault(ebbrt::EbbId id) {
  {
    ebbrt::LocalIdMap::ConstAccessor accessor;
    auto found = ebbrt::local_id_map->Find(accessor, id);
    if (found) {
      auto& pr = *boost::any_cast<CmdServer*>(accessor->second);
      ebbrt::EbbRef<CmdServer>::CacheRef(id, pr);
      return pr;
    }
  }

  ebbrt::EventManager::EventContext context;
  auto f = ebbrt::global_id_map->Get(id);
  CmdServer* p;
  f.Then([&f, &context, &p](ebbrt::Future<std::string> inner) {
    p = new CmdServer();
    ebbrt::event_manager->ActivateContext(std::move(context));
  });
  ebbrt::event_manager->SaveContext(context);
  auto inserted = ebbrt::local_id_map->Insert(std::make_pair(id, p));
  if (inserted) {
    ebbrt::EbbRef<CmdServer>::CacheRef(id, *p);
    return *p;
  }

  delete p;
  // retry reading
  ebbrt::LocalIdMap::ConstAccessor accessor;
  ebbrt::local_id_map->Find(accessor, id);
  auto& pr = *boost::any_cast<CmdServer*>(accessor->second);
  ebbrt::EbbRef<CmdServer>::CacheRef(id, pr);
  return pr;
}

void CmdServer::Do() {
  DPrintf("Do: bufLen:%lu: \n", bufLen_);
  static uint32_t addr = ebbrt::runtime::Frontend();
  static uint16_t port = 8090;
  static uint32_t naddr = ((addr >> 24) & 0xff) |
    (((addr >> 16) & 0xff) << 8) |
    (((addr >> 8) & 0xff) << 16) |
    ((addr & 0xff) << 24);
  static ebbrt::Ipv4Address eaddr(naddr);
  
  cmdServer->Send(eaddr,port,buffer_, bufLen_);
  
  bufLen_=0;
}

void CmdServer::Buffer(std::unique_ptr<ebbrt::MutIOBuf> b) {
  DPrint("START: MutIOBuf\n");
  auto dp = b->GetMutDataPointer();
  auto len = b->Length();

  assert(len < (BUFSIZE - bufLen_));
  
  memcpy(&buffer_[bufLen_],dp.Data(),len);
  bufLen_ += len;
  
  DPrintf("END: MutIOBuf: len:%lu bufLen:%lu\n", len, bufLen_);
}

void CmdServer::Buffer(ebbrt::MutIOBuf *b) {
  DPrint("START: *  MutIOBuf\n");
  auto dp = b->GetMutDataPointer();
  auto len = b->Length();

  assert(len < (BUFSIZE - bufLen_));
  
  memcpy(&buffer_[bufLen_],dp.Data(),len);
  bufLen_ += len;
  
  DPrintf("END: * MutIOBuf: len:%lu bufLen:%lu\n", len, bufLen_);
}

CmdServer::InConnection::InConnection(ebbrt::NetworkManager::TcpPcb pcb)
  : TcpHandler(std::move(pcb)) {
  DPrint("New InConnection\n");
}


CmdServer::OutConnection::OutConnection(ebbrt::NetworkManager::TcpPcb pcb,
					ebbrt::EventManager::EventContext& context,
					size_t windowSize)
  : TcpHandler(std::move(pcb)), context_(context), state_(NONE),
    windowSize_(windowSize) {
  DPrint("New OutConnection\n");
    }

void CmdServer::OutConnection::Connected() {
  DPrint("START\n");
  assert(state_ == BLOCKED);
  state_ = SUCCESS;
  ebbrt::event_manager->ActivateContext(std::move(context_));
}

void CmdServer::InConnection::Connected() {
  DPrint("START\n");
}

void CmdServer::InConnection::Abort() {
  DPrint("START\n");
}


void CmdServer::OutConnection::Abort() {
  DPrint("START\n");
  if (state_ == BLOCKED) {
    state_ = ERROR;
    ebbrt::event_manager->ActivateContext(std::move(context_));
  }
}

void CmdServer::InConnection::Receive(std::unique_ptr<ebbrt::MutIOBuf> b) {
  DPrintf("In Receive buf=%p\n", b.get());
  cmdServer->Buffer(std::move(b));
  //  cmdServer->Buffer(b.get());
}

void CmdServer::OutConnection::Receive(std::unique_ptr<ebbrt::MutIOBuf> b) {
  DPrint("Out Receive???\n");
}

CmdServer::InConnection::~InConnection() {
  DPrint("DESTROYED\n");
}


CmdServer::OutConnection::~OutConnection() {
  DPrint("DESTROYED\n");
}

void CmdServer::OutConnection::Close() {
  DPrint("Closed\n");
}

void CmdServer::InConnection::Close() {
  DPrint("Closed\n");
  cmdServer->Do();
  // ANY FORM OF CLEANUP SEEMS TO trigger bugs in the destruction and rcu logic
  Shutdown(); 
  delete this;
}

void CmdServer::StartListening(uint16_t port) {
  port_ = port;
  listening_pcb_.Bind(port, [this](ebbrt::NetworkManager::TcpPcb pcb) {
      // accept handler
      auto connection = new InConnection(std::move(pcb));
      connection->Install();
  });
}

int CmdServer::OutConnection::ConnectAndBlock(ebbrt::Ipv4Address address,
					      uint16_t port) {
#ifdef PORT_HACK
  static uint16_t lport = START_LPORT;
#endif
  
  DPrint("START\n");
  
  Install();

  DPrint("Installed\n");
  // JA: FIXME:  There is a race here right ... assuming interupts on different cores
#ifdef PORT_HACK
   lport++;
    Pcb().Connect(address, port, lport);
#else
    Pcb().Connect(address, port);
#endif
  DPrint("Blocking!!!!\n");
  DPrint("SavingContext\n");
  state_ = BLOCKED;
  ebbrt::event_manager->SaveContext(context_);
  if (state_ == SUCCESS) return 1;
  return 0;
}

int
CmdServer::OutConnection::SendAndBlock(const char *bytes,
				       size_t len) {


  size_t sendLen;
  size_t sent = 0;
  std::unique_ptr<IOBuf> buf;
  DPrintf("%lu\n", len);
  
  while (len) {
    // send at most windowSize amound of data
    // at once
    if (len > windowSize_) sendLen = windowSize_;
    else sendLen = len;
    
    buf = IOBuf::Create<IOBuf>
      (bytes+sent, sendLen, 
       [this]() {
	DPrint("IOBuf::has been freed\n");
	if (state_ == BLOCKED)  {
	  state_ = SUCCESS;
	  ebbrt::event_manager->ActivateContext(std::move(context_));
	}
       });
    DPrintf("buf created: %lu/%lu\n", sendLen, len);
    Send(std::move(buf)); // buf can now be reassigned.... some else is responsible for delete
    DPrint("Connection Send called\n");
    Pcb().Output();
    DPrint("pcb Output called:  Blocking\n");
    state_ = BLOCKED;
    ebbrt::event_manager->SaveContext(context_);
    if (state_ != SUCCESS) return 0;
      
    DPrintf("Send Done: %lu/%lu\n", sendLen, len+sent);
    sent += sendLen;
    len -= sendLen;
  }
  return sent;
}

void CmdServer::OutConnection::Disconnect() {
  Pcb().Disconnect();
}

int
CmdServer::OutConnection::CloseAndBlock() {
  state_ = BLOCKED;
  ebbrt::event_manager->SaveContext(context_);
  return 1;
}

class TcpPCB : public ebbrt::NetworkManager::TcpPcb {
public:
  ~TcpPCB() {
    DPrint("TcpPCB::~TcpPCB(): called\n");
  }   
};


void CmdServer::Send(ebbrt::Ipv4Address address,
		     uint16_t port,
		     const char *bytes, size_t len) {
  DPrintf("address: %x (%u) port: %d cmdLineLen: %lu cmdline: %s\n",
	  address.toU32(), address.toU32(), port,
	  ebbrt::multiboot::cmdline_len_,
	  (char *)ebbrt::multiboot::CmdLine());
  ebbrt::EventManager::EventContext context;
  //  TcpPCB *pcb = new TcpPCB;
  TcpPCB pcb;
  auto connection = new OutConnection(std::move(pcb), context, SENDWINDOWSIZE);
  
  DPrint("Connection Made\n");


  if (connection->ConnectAndBlock(address, port)) {
    DPrint("Connected before Send\n");
    connection->SendAndBlock(bytes,len);
    DPrint("Sent\n");
    //    connection->Disconnect();
  } else {
    DPrint("ERROR: On Connect :: skip send\n");
  }
  // trigger pcb destructor
  DPrint("calling Shutdown\n");
  connection->Shutdown();
  DPrint("Shutdown called\n");
  //   connection->CloseAndBlock();
  //  delete pcb
  delete connection;
}

