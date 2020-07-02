//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#include "CmdServer.h"

#include <ebbrt/LocalIdMap.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/UniqueIOBuf.h>
#include <ebbrt/native/Multiboot.h>
#include <ebbrt/StaticIOBuf.h>
#include <ebbrt/UniqueIOBuf.h>
#include <ebbrt/EventManager.h>

EBBRT_PUBLISH_TYPE(, CmdServer);

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

CmdServer::CmdServer(ebbrt::Messenger::NetworkId nid)
    : Messagable<CmdServer>(kCmdServerEbbId), remote_nid_(std::move(nid)) {}

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
    p = new CmdServer(ebbrt::Messenger::NetworkId(inner.Get()));
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

void CmdServer::Print(const char* str) {
  auto len = strlen(str) + 1;
  auto buf = ebbrt::MakeUniqueIOBuf(len);
  snprintf(reinterpret_cast<char*>(buf->MutData()), len, "%s", str);
  SendMessage(remote_nid_, std::move(buf));
}

void CmdServer::Print(const char* bytes, size_t len) {
  auto buf = ebbrt::MakeUniqueIOBuf(len);
  memcpy(reinterpret_cast<char*>(buf->MutData()), bytes, len);
  SendMessage(remote_nid_, std::move(buf));
}

void CmdServer::Do() {
  char buf[1024];
  snprintf(buf, sizeof(buf),"Do: bufLen:%lu: ", bufLen_);
  Print(buf);
  Print(buffer_,bufLen_);
  bufLen_=0;
}

void CmdServer::Buffer(std::unique_ptr<ebbrt::MutIOBuf> b) {
  char buf[1024];
  auto dp = b->GetMutDataPointer();
  auto len = b->Length();

  assert(len < (BUFSIZE - bufLen_));
  
  memcpy(&buffer_[bufLen_],dp.Data(),len);
  bufLen_ += len;
  
  snprintf(buf, sizeof(buf),"MutIOBuf: len:%lu bufLen:%lu\n", len, bufLen_);
  Print(buf);
  
}

CmdServer::InConnection::InConnection(ebbrt::NetworkManager::TcpPcb pcb)
  : TcpHandler(std::move(pcb)) {}


CmdServer::OutConnection::OutConnection(ebbrt::NetworkManager::TcpPcb pcb,
				  ebbrt::EventManager::EventContext& context)
#if 1
  : TcpHandler(std::move(pcb)), context_(context), state_(NONE) {}
#else
  : TcpHandler(std::move(pcb)), promise_(NULL), state_(NONE) {}
#endif
void CmdServer::OutConnection::Connected() {
  {
    uint32_t id=ebbrt::event_manager->GetEventId();
    char buf[80];
    snprintf(buf, sizeof(buf), "%s: %u\n", __FUNCTION__, id);
    cmdServer->Print(buf,strlen(buf));
  }
  
  cmdServer->Print("Connected\n");
#if 1
  assert(state_ == BLOCKED);
  state_ = SUCCESS;
  ebbrt::event_manager->ActivateContext(std::move(context_));
#else
  assert(promise_);
  cmdServer->Print("Connected: promise not null\n");
  promise_->SetValue(1);
  cmdServer->Print("Connected: promise value set to 1\n");
#endif
}

void CmdServer::InConnection::Connected() {
    cmdServer->Print("In Connected\n");
}

void CmdServer::InConnection::Abort() {
  cmdServer->Print("Abort\n");
}


void CmdServer::OutConnection::Abort() {
  cmdServer->Print("Abort\n");
#if 1
  if (state_ == BLOCKED) {
    state_ = ERROR;
    ebbrt::event_manager->ActivateContext(std::move(context_));
  }
#else
  if (promise_)  promise_->SetValue(0);
#endif
}

void CmdServer::InConnection::Receive(std::unique_ptr<ebbrt::MutIOBuf> b) {
  cmdServer->Print("In Receive\n");
  cmdServer->Buffer(std::move(b));
}

void CmdServer::OutConnection::Receive(std::unique_ptr<ebbrt::MutIOBuf> b) {
  cmdServer->Print("Out Receive???\n");
}

CmdServer::InConnection::~InConnection() {
  cmdServer->Print("CmdServer::InConnection::~InConnection DESTROYED\n");
}


CmdServer::OutConnection::~OutConnection() {
  cmdServer->Print("CmdServer::OutConnection::~OutConnection DESTROYED\n");
}

void CmdServer::OutConnection::Close() {
  cmdServer->Print("OutConnection::Close\n");
}

void CmdServer::InConnection::Close() {
  cmdServer->Print("InConnection::Close\n");
  cmdServer->Do();
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
  {
    uint32_t id=ebbrt::event_manager->GetEventId();
    char buf[80];
    snprintf(buf, sizeof(buf), "%s: %u\n", __FUNCTION__, id);
    cmdServer->Print(buf,strlen(buf));
  }
  
  Install();
#if 1
  cmdServer->Print("Installed\n");
  // JA: FIXME:  There is a race here right ... assuming interupts on different cores
  Pcb().Connect(address, port);
  cmdServer->Print("SavingContext\n");
  state_ = BLOCKED;
  ebbrt::event_manager->SaveContext(context_);
  if (state_ == SUCCESS) return 1;
  return 0;
  
#else
  assert(promise_ == NULL);
  promise_ = new ebbrt::Promise<int>;
  auto f =promise_->GetFuture();
  cmdServer->Print("Promise Created\n");
  Pcb().Connect(address, port);
  cmdServer->Print("Connect Called\n");
  f.Block();
  cmdServer->Print("After Block");
  int val = f.Get();
  char buf[80];
  snprintf(buf, sizeof(buf), "f.Get=%d\n", val);
  cmdServer->Print(buf, strlen(buf));
  return val;
#endif
}

int
CmdServer::OutConnection::SendAndBlock(const char *bytes,
				    size_t len) {
  std::unique_ptr<IOBuf> buf = IOBuf::Create<IOBuf>
    (bytes, len, 
     [this]() {
      cmdServer->Print("IOBuf::has been freed\n");
      if (state_ == BLOCKED)  {
	state_ = SUCCESS;
	ebbrt::event_manager->ActivateContext(std::move(context_));
      }
     });

  char sbuf[80];
  snprintf(sbuf, sizeof(sbuf), "buf created: %s %lu\n", bytes, len);
  cmdServer->Print(sbuf, strlen(sbuf));

  
  Send(std::move(buf));
  cmdServer->Print("Connection Send called\n");
  Pcb().Output();
  cmdServer->Print("pcb Output called:  Blockeing\n");
  state_ = BLOCKED;
  ebbrt::event_manager->SaveContext(context_);
  cmdServer->Print("Send Done\n");
  if (state_ == SUCCESS) return len;
  return 0;
}

int
CmdServer::OutConnection::CloseAndBlock() {
  return 1;
}


void CmdServer::Send(ebbrt::Ipv4Address address,
		     uint16_t port,
		     const char *bytes, size_t len) {
  char buf[1024];
  
  snprintf(buf, sizeof(buf),
	   "address: %x (%u) port: %d cmdLineLen: %lu cmdline: %s\n",
	   address.toU32(), address.toU32(), port,
	   ebbrt::multiboot::cmdline_len_,
	   (char *)ebbrt::multiboot::CmdLine());
  Print(buf);
  ebbrt::EventManager::EventContext context;
  ebbrt::NetworkManager::TcpPcb pcb;
  auto connection = new OutConnection(std::move(pcb), context);
  Print("Connection Made\n");

  if (connection->ConnectAndBlock(address, port)) {
    Print("Connected before Send\n");
    connection->SendAndBlock(bytes,len);
    connection->CloseAndBlock();
    Print("Sent\n");
  } else {
    Print("ERROR: On Connect :: skip send\n");
  }
  connection->Shutdown();
  delete connection;
}

void CmdServer::ReceiveMessage(ebbrt::Messenger::NetworkId nid,
                             std::unique_ptr<ebbrt::IOBuf>&& buffer) {
  throw std::runtime_error("CmdServer: Received message unexpectedly!");
}
