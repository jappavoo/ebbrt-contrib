//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef APPS_CMDSERVER_HOSTED_SRC_PRINTER_H_
#define APPS_CMDSERVER_HOSTED_SRC_PRINTER_H_

#include <string>

#include <ebbrt/Message.h>
#include <ebbrt/StaticSharedEbb.h>

#include "../../src/StaticEbbIds.h"

class CmdServer : public ebbrt::StaticSharedEbb<CmdServer>,
                public ebbrt::Messagable<CmdServer> {
 public:
  CmdServer();

  static ebbrt::Future<void> Init();
  void Print(const char* string);
  void ReceiveMessage(ebbrt::Messenger::NetworkId nid,
                      std::unique_ptr<ebbrt::IOBuf>&& buffer);
};

constexpr auto printer = ebbrt::EbbRef<CmdServer>(kCmdServerEbbId);

#endif  // APPS_CMDSERVER_HOSTED_SRC_PRINTER_H_
