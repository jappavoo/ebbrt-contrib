#ifndef __EBBRT_CPU_H__
#define __EBBRT_CPU_H__

#include <pthread.h>
#include <mutex>
#include <ebbrt/Context.h>
#include <ebbrt/Nid.h>

namespace ebbrt {
  class Cpu {
  public:
    static const constexpr size_t kMaxCpus = 256;
    
  Cpu(size_t index) : index_(index), ctxt_(rt_) {
      // printf("%p: index=%zd index_=%zd\n", this, index, index_);
    }
    
    static Cpu& GetMine() { return *my_cpu_tls_; }
    static Nid GetMyNode() { return GetMine().nid(); }
    static Cpu* GetByIndex(size_t index);
    static size_t Count();
    static pthread_t EarlyInit();
    static int GetPhysCpus() { 
#if 1
      return sysconf(_SC_NPROCESSORS_ONLN); 
#else
      return 2;
#endif
    }
    
    void Init();
    operator size_t() const { return index_; }
    Nid nid() const { return nid_; }
    void set_nid(Nid nid) { nid_ = nid; }
    void set_tid(pthread_t tid) { tid_ = tid; };
    pthread_t get_tid() { return tid_;}
    Context * get_context() { return & ctxt_; }
  private:
    static Cpu * Create(size_t index);
    static volatile int running_;
    static volatile int created_;
    static volatile int inited_;
    static void * run(void *arg);    
    static Runtime rt_;
    static thread_local Cpu* my_cpu_tls_;
    
    static std::mutex init_lock_;
    static int numCpus;
    size_t index_;
    pthread_t tid_;
    Nid nid_;
    Context ctxt_;
    friend class EventManager;
  };
};

#endif
