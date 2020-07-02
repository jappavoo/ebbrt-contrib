//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef APPS_CMDSERVER_BAREMETAL_SRC_PRINTER_H_
#define APPS_CMDSERVER_BAREMETAL_SRC_PRINTER_H_

#include <string>

#include <ebbrt/Message.h>
#include "../../src/StaticEbbIds.h"

class CmdServer : public ebbrt::Messagable<CmdServer> {
  class InConnection : public ebbrt::TcpHandler {
  public:
    explicit InConnection(ebbrt::NetworkManager::TcpPcb pcb);

    // call backs from pcb
    void Receive(std::unique_ptr<ebbrt::MutIOBuf> b) override;
    void Connected() override;
    void Close() override;
    void Abort() override;
    ~InConnection();
  };
  class OutConnection : public ebbrt::TcpHandler {
#if 1
    ebbrt::EventManager::EventContext& context_;
#else
    ebbrt::Promise<int> *promise_;
#endif
    enum STATE { NONE, BLOCKED, SUCCESS, ERROR } state_;
  public:
    explicit OutConnection(ebbrt::NetworkManager::TcpPcb pcb,
			ebbrt::EventManager::EventContext& context);

    // operations on connection
    int ConnectAndBlock(ebbrt::Ipv4Address address,
			uint16_t port);
    int SendAndBlock(const char *byte, size_t len);
    int CloseAndBlock();
    
    // call backs from pcb
    void Receive(std::unique_ptr<ebbrt::MutIOBuf> b) override;
    void Connected() override;
    void Close() override;
    void Abort() override;
    ~OutConnection();
  };
 public:
  explicit CmdServer(ebbrt::Messenger::NetworkId nid);

  static CmdServer& HandleFault(ebbrt::EbbId id);

  void Print(const char* string);
  void Print(const char* bytes, size_t len);
  
  void ReceiveMessage(ebbrt::Messenger::NetworkId nid,
                      std::unique_ptr<ebbrt::IOBuf>&& buffer);
  void Send(ebbrt::Ipv4Address address,
	    uint16_t port,
	    const char *bytes, size_t len);
  void Buffer(std::unique_ptr<ebbrt::MutIOBuf> b);
  void Do();
  void StartListening(uint16_t port);
 private:
  ebbrt::NetworkManager::ListeningTcpPcb listening_pcb_;
  uint16_t port_;
  static const size_t BUFSIZE=4096;
  ebbrt::Messenger::NetworkId remote_nid_;
  char buffer_[BUFSIZE];
  size_t bufLen_;
};

constexpr auto cmdServer = ebbrt::EbbRef<CmdServer>(kCmdServerEbbId);

#endif  // APPS_CMDSERVER_BAREMETAL_SRC_PRINTER_H_
