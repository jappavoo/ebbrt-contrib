//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#include "CmdServer.h"

#include <iostream>

#include <ebbrt/GlobalIdMap.h>

EBBRT_PUBLISH_TYPE(, CmdServer);

CmdServer::CmdServer() : ebbrt::Messagable<CmdServer>(kCmdServerEbbId) {}

ebbrt::Future<void> CmdServer::Init() {
  return ebbrt::global_id_map->Set(
      kCmdServerEbbId, ebbrt::GlobalIdMap::OptArgs({.data=ebbrt::messenger->LocalNetworkId().ToBytes()}));
}

void CmdServer::Print(const char* str) {}

void CmdServer::ReceiveMessage(ebbrt::Messenger::NetworkId nid,
                             std::unique_ptr<ebbrt::IOBuf>&& buffer) {
  auto output = std::string(reinterpret_cast<const char*>(buffer->Data()),
                            buffer->Length());
  std::cout << nid.ToString() << ": " << output;
}
