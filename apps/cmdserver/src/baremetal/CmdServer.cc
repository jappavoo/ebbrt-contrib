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

#define DEBUG_WITH_PRINTER

#ifdef DEBUG_WITH_PRINTER
#include "Printer.h"
#define DPrint printer->Print
#define DPrintf(fmt, ...) {						\
  char dpbuf[255];							\
  snprintf(dpbuf,  sizeof(dpbuf), "%s: " fmt, __func__, __VA_ARGS__);	\
  DPrint(dpbuf);						\
}
#else
#define DPrint
#define DPrintf(...)
#endif

#include "Printer.h"
#define Print printer->Print


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
  : TcpHandler(std::move(pcb)), context_(context), state_(NONE) {}

void CmdServer::OutConnection::Connected() {
  {
    uint32_t id=ebbrt::event_manager->GetEventId();
    char buf[80];
    snprintf(buf, sizeof(buf), "%s: %u\n", __FUNCTION__, id);
    Print(buf,strlen(buf));
  }
  
  Print("Connected\n");
  assert(state_ == BLOCKED);
  state_ = SUCCESS;
  ebbrt::event_manager->ActivateContext(std::move(context_));
}

void CmdServer::InConnection::Connected() {
    Print("In Connected\n");
}

void CmdServer::InConnection::Abort() {
  Print("Abort\n");
}


void CmdServer::OutConnection::Abort() {
  Print("Abort\n");
  if (state_ == BLOCKED) {
    state_ = ERROR;
    ebbrt::event_manager->ActivateContext(std::move(context_));
  }
}

void CmdServer::InConnection::Receive(std::unique_ptr<ebbrt::MutIOBuf> b) {
  Print("In Receive\n");
  cmdServer->Buffer(std::move(b));
}

void CmdServer::OutConnection::Receive(std::unique_ptr<ebbrt::MutIOBuf> b) {
  Print("Out Receive???\n");
}

CmdServer::InConnection::~InConnection() {
  Print("CmdServer::InConnection::~InConnection DESTROYED\n");
}


CmdServer::OutConnection::~OutConnection() {
  Print("CmdServer::OutConnection::~OutConnection DESTROYED\n");
}

void CmdServer::OutConnection::Close() {
  Print("OutConnection::Close\n");
}

void CmdServer::InConnection::Close() {
  Print("InConnection::Close\n");
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
    Print(buf,strlen(buf));
  }
  
  Install();

  Print("Installed\n");
  // JA: FIXME:  There is a race here right ... assuming interupts on different cores
  Pcb().Connect(address, port);
  Print("SavingContext\n");
  state_ = BLOCKED;
  ebbrt::event_manager->SaveContext(context_);
  if (state_ == SUCCESS) return 1;
  return 0;
}

int
CmdServer::OutConnection::SendAndBlock(const char *bytes,
				    size_t len) {
  std::unique_ptr<IOBuf> buf = IOBuf::Create<IOBuf>
    (bytes, len, 
     [this]() {
      Print("IOBuf::has been freed\n");
      if (state_ == BLOCKED)  {
	state_ = SUCCESS;
	ebbrt::event_manager->ActivateContext(std::move(context_));
      }
     });

  char sbuf[80];
  snprintf(sbuf, sizeof(sbuf), "buf created: %s %lu\n", bytes, len);
  Print(sbuf, strlen(sbuf));

  
  Send(std::move(buf));
  Print("Connection Send called\n");
  Pcb().Output();
  Print("pcb Output called:  Blockeing\n");
  state_ = BLOCKED;
  ebbrt::event_manager->SaveContext(context_);
  Print("Send Done\n");
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

