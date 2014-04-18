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

#ifndef __EBBRT_BM__
  class InputStream;
  extern InputStream *stdin;

  class InputStream {
  public:
    static const size_t kBufferSize = 1024;
  private:
    int fd_;
    size_t len_;
    boost::asio::posix::stream_descriptor::bytes_readable asioAvail_;
    boost::asio::posix::stream_descriptor *sd_;
    bool doRead_;
    char buffer_[kBufferSize];
    std::size_t count_;
    ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,size_t avail)> consumer_;
    
  void async_read_some();
  public:
    
    InputStream(int fd);
    
    void  async_read_stop() { 
      if (doRead_==false) return;
      printf("\nStopping Streaming Read\n"); 
      doRead_=false; 
    }
    
    void async_read_start(ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>,size_t avail)> consumer);
    
    static void Init() {
      int fd = ::dup(STDIN_FILENO);
      UNIX::stdin = new InputStream(fd);
    }
  };
#endif

  extern ebbrt::Messenger::NetworkId fe;
  
#ifdef __EBBRT_BM__
  __attribute__ ((unused)) static void Init() {
  fe = ebbrt::Messenger::NetworkId(ebbrt::runtime::Frontend());
}
#else
  __attribute__ ((unused)) static void Init(int argc, const char **argv) {
    CmdLineArgs::Init(argc, argv).Block();
    Environment::Init().Block();
  }
#endif

    constexpr auto cmd_line_args = ebbrt::EbbRef<CmdLineArgs>(kCmdLineArgsId);
    constexpr auto environment =  ebbrt::EbbRef<Environment>(kEnvironmentId);
};

#endif
