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
  //  cmdServer->Send(ebbrt::Ipv4Address(naddr),port,"Cmd\n", 4);
  cmdServer->StartListening(port);

  cmdServer->Send(ebbrt::Ipv4Address(naddr),port,"Hi\n", 3);
  
  // cmdServer->Send(ebbrt::Ipv4Address(naddr),port,"root:x:0:0:root:/root:/bin/bash daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin bin:x:2:2:bin:/bin:/usr/sbin/nologin sys:x:3:3:sys:/dev:/usr/sbin/nologin sync:x:4:65534:sync:/bin:/bin/sync games:x:5:60:games:/usr/games:/usr/sbin/nologin man:x:6:12:man:/var/cache/man:/usr/sbin/nologin lp:x:7:7:lp:/var/spool/lpd:/usr/sbin/nologin mail:x:8:8:mail:/var/mail:/usr/sbin/nologin news:x:9:9:news:/var/spool/news:/usr/sbin/nologin uucp:x:10:10:uucp:/var/spool/uucp:/usr/sbin/nologin proxy:x:13:13:proxy:/bin:/usr/sbin/nologin www-data:x:33:33:www-data:/var/www:/usr/sbin/nologin backup:x:34:34:backup:/var/backups:/usr/sbin/nologin list:x:38:38:Mailing List Manager:/var/list:/usr/sbin/nologin irc:x:39:39:ircd:/var/run/ircd:/usr/sbin/nologin gnats:x:41:41:Gnats Bug-Reporting System (admin):/var/lib/gnats:/usr/sbin/nologin nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin systemd-timesync:x:100:102:systemd Time Synchronization,,,:/run/systemd:/bin/false systemd-network:x:101:103:systemd Network Management,,,:/run/systemd/netif:/bin/false systemd-resolve:x:102:104:systemd Resolver,,,:/run/systemd/resolve:/bin/false systemd-bus-proxy:x:103:105:systemd Bus Proxy,,,:/run/systemd:/bin/false syslog:x:104:108::/home/syslog:/bin/false _apt:x:105:65534::/nonexistent:/bin/false lxd:x:106:65534::/var/lib/lxd/:/bin/false messagebus:x:107:111::/var/run/dbus:/bin/false uuidd:x:108:112::/run/uuidd:/bin/false dnsmasq:x:109:65534:dnsmasq,,,:/var/lib/misc:/bin/false sshd:x:110:65534::/var/run/sshd:/usr/sbin/nologin tommyu:x:1000:1000:tommy,,,:/home/tommyu:/bin/bash awadyn:x:1001:1001:Yara Awad,,,:/home/awadyn:/bin/bash colord:x:111:118:colord colour management daemon,,,:/var/lib/colord:/bin/false wpartrid:x:1002:1002:,,,:/home/wpartrid:/bin/bash sesauser:x:1003:1003::/home/sesauser: handong:x:1004:1004:,,,:/home/handong:/bin/bash jmcadden:x:1005:1005:,,,:/home/jmcadden:/bin/bash libvirt-qemu:x:64055:119:Libvirt Qemu,,,:/var/lib/libvirt:/bin/false libvirt-dnsmasq:x:112:120:Libvirt Dnsmasq,,,:/var/lib/libvirt/dnsmasq:/bin/false uml-net:x:113:122::/home/uml-net:/bin/false ntp:x:114:123::/home/ntp:/bin/false devoshm:x:1006:1006:,,,:/home/devoshm:/bin/bash jappavoo:x:1007:1008:,,,:/home/jappavoo:/bin/bash lightdm:x:115:125:Light Display Manager:/var/lib/lightdm:/bin/false avahi:x:116:127:Avahi mDNS daemon,,,:/var/run/avahi-daemon:/bin/false pulse:x:117:129:PulseAudio daemon,,,:/var/run/pulse:/bin/false nvidia-persistenced:x:118:131:NVIDIA Persistence Daemon,,,:/:/sbin/nologin rtkit:x:119:132:RealtimeKit,,,:/proc:/bin/false usbmux:x:120:46:usbmux daemon,,,:/var/lib/usbmux:/bin/false\n", 2599);

  
   cmdServer->Send(ebbrt::Ipv4Address(naddr),port,"All Done!\n", 10);
   
      
}

  
