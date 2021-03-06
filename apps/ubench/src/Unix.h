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
#include <ebbrt/Message.h>

#include "StaticEbbIds.h"


namespace UNIX {
  class CmdLineArgs {      
  public:
    class Root {
    private:
      std::mutex lock_;
      ebbrt::EbbId myId_;
      CmdLineArgs *theRep_;
      ebbrt::SharedFuture<std::string> data_;
    public:
      Root(ebbrt::EbbId id);
      ebbrt::EbbId myId() { return myId_; }
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
    char **data() { return argv_vector.data(); }
    char *argv(int i) { return argv_vector[i]; }
    int argc() { return argc_; }
  };


  class Environment {      
  public:
    class Root {
    private:
      std::mutex lock_;
      ebbrt::EbbId myId_;
      Environment *theRep_;
      ebbrt::SharedFuture<std::string> data_;
    public:
      Root(ebbrt::EbbId id);
      ebbrt::EbbId myId() { return myId_; }
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

  class InputStream : public ebbrt::Messagable<InputStream> {
  public:
    void ReceiveMessage(ebbrt::Messenger::NetworkId nid, 
			std::unique_ptr<ebbrt::IOBuf>&& buf);
    struct RootMembers {
      int fd;
      size_t len;
      enum FileTypes { kUNKNOWN, kBLOCK_DEV, kCHAR_DEV, kSOCKET, kPIPE, 
		       kFILE, kDIR, kSYMLINK } type;
      static std::string toString(RootMembers * rm) {
	return std::move(std::string((char *)rm,sizeof(*rm)));
      }
    };
    // Kludge : not stable accross platforms
    class Root {
    private:
      std::mutex lock_;  
      ebbrt::EbbId myId_;                      
      InputStream *theRep_;
      ebbrt::SharedFuture<std::string> data_;
      RootMembers *members_;
      enum MessageType { kSTREAM_START, kSTREAM_DATA, kSTREAM_STOP};
      struct Message {
	MessageType type;
      };
      struct StreamStartMsg : Message {
      } stream_start_msg_;
      struct StreamDataMsg : Message {
	size_t avail;
      } stream_data_msg_;
      struct StreamStopMsg : Message {
      } stream_stop_msg_;
      friend InputStream;
    public:
      Root(ebbrt::EbbId id);
      InputStream * getRep_BIN();
      ebbrt::SharedFuture<std::string> getString() { return data_; }
      int fd() { return members_->fd; }
      size_t len() { return members_->len; }
      RootMembers::FileTypes type() { return members_->type; }
      ebbrt::EbbId myId() { return myId_; }
      void start_stream();
      void stop_stream();
      void process_message(ebbrt::Messenger::NetworkId, 
			   std::unique_ptr<ebbrt::IOBuf>&&);
    }; 
    static const size_t kBufferSize = 4096;
  private:
    friend Root;
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
    ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>&&,
				size_t avail)> consumer_;

    InputStream(Root *root);

    void async_read_some();
    static ebbrt::Future<ebbrt::EbbRef<InputStream>> Create(ebbrt::EbbId id, int fd);

  public:
    static InputStream & HandleFault(ebbrt::EbbId id);
    static ebbrt::Future<ebbrt::EbbRef<InputStream>> Create(int fd);
    static ebbrt::Future<ebbrt::EbbRef<InputStream>> InitSIn();
    void destroy();

    void async_read_start(ebbrt::MovableFunction<void(std::unique_ptr<ebbrt::IOBuf>&&,
						      size_t avail)> consumer);

    void  async_read_stop() { 
      if (doRead_==false) return;
#ifdef __EBBRT_BM__
      ebbrt::kprintf("\nStopping Streaming Read\n"); 
#else
      printf("\nStopping Streaming Read\n"); 
      if (sd_) sd_->cancel();
#endif
      doRead_=false; 
      myRoot_->stop_stream();
    }

    bool isFile() { return ( myRoot_->type() == RootMembers::kFILE); }
  };

  class FS : public ebbrt::Messagable<FS> {      
  public:
    void ReceiveMessage(ebbrt::Messenger::NetworkId nid, 
			std::unique_ptr<ebbrt::IOBuf>&& buf);

    class Root {
    private:
      std::mutex lock_;
      ebbrt::EbbId myId_;
      FS *theRep_;
      ebbrt::SharedFuture<std::string> data_;
      ebbrt::Promise<ebbrt::EbbRef<InputStream>> *p_;
      // FIXME: JA KLUGDE UNIX_EXIT does not belong here
      //        but too lazy to add a process object
      //        Add Process Object that has exit etc.
      enum MessageType { kOPEN_STREAM, kSTREAM_ID, kPROCESS_EXIT};
      struct Message {
	MessageType type;
      };
      struct OpenStreamMsg : Message {
      } open_stream_msg_;
      struct StreamIdMsg : Message {
	ebbrt::EbbId id;
      } stream_id_msg_;
      struct ProcessExitMsg : Message {
	int val;
      } process_exit_msg_;
      friend FS;
    public:
      Root(ebbrt::EbbId id);
      ebbrt::EbbId myId() { return myId_; }
      FS * getRep_BIN();
      ebbrt::SharedFuture<std::string> getString() { return data_; }
      ebbrt::Future<ebbrt::EbbRef<InputStream>> open_stream(std::string);
      void process_exit(int val);
      void process_message(ebbrt::Messenger::NetworkId, 
			   std::unique_ptr<ebbrt::IOBuf>&&);
    }; 
  private:
    // Representative Members
    Root *myRoot_;
    // Private Constrtor called by HandleFault
    FS(Root *root);
  public:
    static FS & HandleFault(ebbrt::EbbId id);
    static ebbrt::Future<ebbrt::EbbRef<FS>> Init(std::string &&path);
    void destroy();
    ebbrt::Future<ebbrt::EbbRef<InputStream>>  openInputStream(std::string path);
    void processExit(int val);
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
    InputStream::InitSIn().Block();
    FS::Init("/").Block();
  }
#endif

  constexpr auto cmd_line_args = ebbrt::EbbRef<CmdLineArgs>(kCmdLineArgsId);
  constexpr auto environment =  ebbrt::EbbRef<Environment>(kEnvironmentId);
  constexpr auto sin = ebbrt::EbbRef<InputStream>(kSInId);
  constexpr auto root_fs = ebbrt::EbbRef<FS>(kRootFSId);

};

#endif

