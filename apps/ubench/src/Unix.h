//          Copyright Boston University SESA Group 2013 - 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef __UNIX_EBBRT_H__
#define __UNIX_EBBRT_H__

#include <ebbrt/Messenger.h>

#include <ebbrt/Runtime.h>

namespace UNIX {
  extern ebbrt::Messenger::NetworkId fe;
  static void Init() {
    // assert runtime initialized
#ifdef __BAREMETAL__
    fe = ebbrt::Messenger::NetworkId(ebbrt::runtime::Frontend());
#endif
  }
#if 0
  class MasterEbb : public ebbrt::StaticSharedEbb<MasterEbb>,
		    public ebbrt::Messagable<MasterEbb> { 
  public:
    void ReceiveMessage(ebbrt::Messenger::NetworkId nid,
			std::unique_ptr<ebbrt::IOBuf>&& buffer);
  }

  class CmdLineArgs {
    
  public:
    Class Replica  {
      char **argv;
      int argc;
    protected:
      replicate();
    public:
      char *argv(int i) { return argv[i]; }
      int argc() { return argc; }
    };

    Class Master : public MasterEbb,
		   public Replica {
    public:
    };

    Class Drone : public DroneEbb<Drone>
		  public Replica {
    };
#endif
#if 1
    class CmdLineArgs {      
    public:
      typedef std::unique_ptr<ebbrt::IOBuf> RootDataPtr;	
      struct Root {
	std::atomic<intptr_t> theRep_;
	RootDataPtr data_;
      }; 
    private:
      // Representative Members
      Root *myRoot_;
      // FIXME: I think this really should be a std::unique_ptr but dont' know
      // how
      std::vector<char *> argv_vector;
      //    std:unique_ptr<char *>char argv_[];
      int argc_;
      // Private Constrtor called by HandleFault
      CmdLineArgs(Root *root);
    public:
      static CmdLineArgs & HandleFault(ebbrt::EbbId id);
      static ebbrt::EbbRef<CmdLineArgs> Create(int argc, const char **argv);
      void destroy();
      char *argv(int i) { return argv_vector[i]; }
      int argc() { return argc_; }
    };
#endif
};

#endif
