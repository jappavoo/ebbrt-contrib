#ifndef _UBENCH_STATIC_EBBIDS_H_
#define _UBENCH_STATIC_EBBIDS_H_

namespace UNIX {
  enum : ebbrt::EbbId {
    kCmdLineArgsId = ebbrt::kFirstStaticUserId,
    kEnvironmentId 
  };
};

#endif
