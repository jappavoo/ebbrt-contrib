//          Copyright Boston University SESA Group 2013 - 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef __UNIX_EBBRT_H__
#define __UNIX_EBBRT_H__


#include <ebbrt/Messenger.h>
#include <ebbrt/Runtime.h>
#include <ebbrt/Future.h>
#include <ebbrt/Buffer.h>

#include "StaticEbbIds.h"

namespace UNIX {
  class CmdLineArgs {      
  public:
    class Root {
    private:
      std::mutex lock_;                        
      CmdLineArgs *theRep_;
      ebbrt::SharedFuture<std::string> data_;
    public:
      Root();
      CmdLineArgs * getRep_BIN();
      ebbrt::SharedFuture<std::string> getString() { return data_; }
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
    static ebbrt::Future<ebbrt::EbbRef<CmdLineArgs>> Init(int argc, 
							  const char **argv);
    void destroy();
    char *argv(int i) { return argv_vector[i]; }
    int argc() { return argc_; }
  };


  class Environment {      
  public:
    class Root {
    private:
      std::mutex lock_;                        
      Environment *theRep_;
      ebbrt::SharedFuture<std::string> data_;
    public:
      Root();
      Environment * getRep_BIN();
      ebbrt::SharedFuture<std::string> getString() { return data_; }
    }; 
  private:
    // Representative Members
    Root *myRoot_;
    char **environ_;
    // Private Constrtor called by HandleFault
    Environment(Root *root);
  public:
    static Environment & HandleFault(ebbrt::EbbId id);
    static ebbrt::Future<ebbrt::EbbRef<Environment>> Init();
    void destroy();

    char **environ() { return ::environ; }
    char *getenv(const char *name) { return ::getenv(name); }
    int putenv(char *string) { return ::putenv(string); }

  };

  class InputStream {
  public:

    struct RootMembers {
      int fd;
      size_t len;
      enum FileTypes { UNKNOWN, BLOCK_DEV, CHAR_DEV, SOCKET, PIPE, FILE, DIR, SYMLINK } type;
      static std::string toString(RootMembers * rm) {
	return std::move(std::string((char *)rm,sizeof(*rm)));
      }
    };
    // Kludge : not stable accross platforms
    class Root {
    private:
      std::mutex lock_;                        
      InputStream *theRep_;
      ebbrt::SharedFuture<std::string> data_;
      RootMembers *members_;
    public:
      Root();
      InputStream * getRep_BIN();
      ebbrt::SharedFuture<std::string> getString() { return data_; }
      int fd() { return members_->fd; }
      size_t len() { return members_->len; }
      RootMembers::FileTypes type() { return members_->type; }
    }; 
    static const size_t kBufferSize = 1024;
  private:
    Root *myRoot_;
#ifndef __EBBRT_BM__
    boost::asio::posix::stream_descriptor::bytes_readable asioAvail_;
    boost::asio::posix::stream_descriptor *sd_;
#endif

    int fd_;
    size_t len_;

    bool doRead_;
    char buffer_[kBufferSize];
    std::size_t count_;
    ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,size_t avail)> consumer_;

    InputStream(Root *root);

    void async_read_some();
  public:
    static InputStream & HandleFault(ebbrt::EbbId id);
    static ebbrt::Future<ebbrt::EbbRef<InputStream>> Init();
    void destroy();

#ifdef __EBBRT_BM__    
    void  async_read_stop() {}
    
    void async_read_start(ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,
						      size_t avail)> consumer) {}
#else    
    void  async_read_stop() { 
      if (doRead_==false) return;
      printf("\nStopping Streaming Read\n"); 
      doRead_=false; 
    }
    
    void async_read_start(ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,
						      size_t avail)> consumer);
    
#endif
  };

  extern ebbrt::Messenger::NetworkId fe;
  
#ifdef __EBBRT_BM__
  __attribute__ ((unused)) static void Init() {
  fe = ebbrt::Messenger::NetworkId(ebbrt::runtime::Frontend());
}
#else
  __attribute__ ((unused)) static void Init(int argc, const char **argv) {
    CmdLineArgs::Init(argc, argv).Block();
    Environment::Init().Block();
    InputStream::Init().Block();
  }
#endif

  
  constexpr auto cmd_line_args = ebbrt::EbbRef<CmdLineArgs>(kCmdLineArgsId);
  constexpr auto environment =  ebbrt::EbbRef<Environment>(kEnvironmentId);
  constexpr auto sin = ebbrt::EbbRef<InputStream>(kSInId);
};

#endif

