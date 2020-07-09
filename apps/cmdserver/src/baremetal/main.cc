//          Copyright Boston University SESA Group 2013 - 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Printer.h"
#include "CmdServer.h"

void AppMain() {
  uint32_t addr = ebbrt::runtime::Frontend();
  uint16_t port = 8090;
  uint32_t naddr = ((addr >> 24) & 0xff) |
    (((addr >> 16) & 0xff) << 8) |
    (((addr >> 8) & 0xff) << 16) |
		   ((addr & 0xff) << 24);
  char buf[80];
  snprintf(buf, sizeof(buf), "Hello -> (%u.%u.%u.%u -%u.%u.%u.%u) \n",
	   (addr >> 24) & 0xff,
	   (addr >> 16) & 0xff,
	   (addr >> 8) & 0xff,
	   (addr) & 0xff, 
	   (naddr >> 24) & 0xff,
	   (naddr >> 16) & 0xff,
	   (naddr >> 8) & 0xff,
	   (naddr) & 0xff);
  
  printer->Print(buf);
  cmdServer->Send(ebbrt::Ipv4Address(naddr),port,"Cmd\n", 4);
  cmdServer->StartListening(port);
}
