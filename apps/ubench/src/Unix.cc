#include <ebbrt/Messenger.h>

namespace UNIX {
#ifdef __EBBRT_BM__
  ebbrt::Messenger::NetworkId fe=ebbrt::Messenger::NetworkId(0);
#else
  ebbrt::Messenger::NetworkId fe=ebbrt::Messenger::NetworkId(boost::asio::ip::address_v4(0));
#endif
};
