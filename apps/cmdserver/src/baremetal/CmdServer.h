//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef APPS_CMDSERVER_BAREMETAL_SRC_PRINTER_H_
#define APPS_CMDSERVER_BAREMETAL_SRC_PRINTER_H_

#include <string>

#include <ebbrt/Message.h>
#include "../../src/StaticEbbIds.h"

class CmdServer  {
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
    ebbrt::EventManager::EventContext& context_;
    enum STATE { NONE, BLOCKED, SUCCESS, ERROR } state_;
    size_t windowSize_;
  public:
    explicit OutConnection(ebbrt::NetworkManager::TcpPcb pcb,
			   ebbrt::EventManager::EventContext& context,
			   size_t windowSize);

    // operations on connection
    int ConnectAndBlock(ebbrt::Ipv4Address address,
			uint16_t port);
    int SendAndBlock(const char *byte, size_t len);
    int CloseAndBlock();
    void Disconnect();
    
    // call backs from pcb
    void Receive(std::unique_ptr<ebbrt::MutIOBuf> b) override;
    void Connected() override;
    void Close() override;
    void Abort() override;
    ~OutConnection();
  };
 public:
  explicit CmdServer();

  static CmdServer& HandleFault(ebbrt::EbbId id);

  void Send(ebbrt::Ipv4Address address,
	    uint16_t port,
	    const char *bytes, size_t len);
  void Buffer(std::unique_ptr<ebbrt::MutIOBuf> b);
  void Buffer(ebbrt::MutIOBuf *b);
  void Do();
  void StartListening(uint16_t port);
 private:
  ebbrt::NetworkManager::ListeningTcpPcb listening_pcb_;
  uint16_t port_;
  static const size_t BUFSIZE=4096;
  char buffer_[BUFSIZE];
  size_t bufLen_;
  static const size_t SENDWINDOWSIZE=256;
};

constexpr auto cmdServer = ebbrt::EbbRef<CmdServer>(kCmdServerEbbId);

#endif  // APPS_CMDSERVER_BAREMETAL_SRC_PRINTER_H_
