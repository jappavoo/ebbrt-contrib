#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <array>

#include <boost/filesystem.hpp>
#include <boost/container/static_vector.hpp>

#include <ebbrt/Context.h>
#include <ebbrt/ContextActivation.h>
#include <ebbrt/GlobalIdMap.h>
#include <ebbrt/StaticIds.h>
#include <ebbrt/NodeAllocator.h>
#include <ebbrt/Runtime.h>
#include <stdio.h>

#include "../../src/ubenchCommon.h"
#include "../../src/Cpu.h"
#include "../../src/Unix.h"

void Main(void);

namespace ebbrt {
  // static memebers
  std::array<ExplicitlyConstructed<ebbrt::Cpu>, ebbrt::Cpu::kMaxCpus> cpus;
  int Cpu::numCpus=0;

  Runtime  Cpu::rt_;
  thread_local ebbrt::Cpu* ebbrt::Cpu::my_cpu_tls_;
  
  volatile int Cpu::running_ = 0;
  volatile int Cpu::created_ = 0;
  volatile int Cpu::inited_ = 0;

  std::mutex Cpu::init_lock_;

  ebbrt::Cpu * Cpu::Create(size_t index) 
  {
    cpus[index].construct(index);
    numCpus++;
    return (ebbrt::Cpu *)&cpus[index];
  }
  
  void Cpu::Init() 
  {
    my_cpu_tls_ = this;
    set_tid(pthread_self());
  }
  
  void * Cpu::run(void *arg) 
  {
    size_t index = (size_t)arg;
    auto n = GetPhysCpus();

    init_lock_.lock();
    auto cpu = Create(index);
    cpu->Init();
    __sync_fetch_and_add(&created_,1);
    init_lock_.unlock();

#ifdef __TRACE_SPAWN__
    printf("index=%zd:&cpu=%p:mine=%zd:tid=0x%zx:&cpu=%p:my_cpu_tls=%p:"
    "ctxt.index_=%zd:created\n", index, cpu, 
	     (size_t)GetMine(), GetMine().get_tid(), &GetMine(), my_cpu_tls_, 
	     size_t(cpu->ctxt_));
#endif

    while (created_<n);

    cpu->ctxt_.Activate();
    ebbrt::event_manager->Spawn([](){
	auto n = GetPhysCpus();
#ifdef __TRACE_SPAWN__
        printf("spawned: %zd %zd: %s:%d\n", size_t(ebbrt::Cpu::GetMine()), 
	        size_t(*ebbrt::active_context),
	        __FILE__, __LINE__);
#endif
	__sync_fetch_and_add(&inited_,1);
	while (inited_< n);     // sync up -- for everyone to be ready to start 
	if ((size_t)GetMine()==0) Main();
      });
    cpu->ctxt_.Deactivate();
    __sync_fetch_and_add(&running_,1);

    while (running_< n); // wait for everyone to be spawned
	  
    // this thread should now infinitely run the event loop
    boost::asio::io_service::work work(cpu->ctxt_.io_service_);
//    while (1)   
   cpu->ctxt_.Run();
    assert(0);
  }
  
  pthread_t Cpu::EarlyInit(void) 
  { 
    // lets now get all the cpus create one per physical core
    int i;
    pthread_attr_t attr;
    pthread_t rc=0;
    int n = GetPhysCpus();

    pthread_attr_init(&attr);
  
    for (i=0; i<n; i++) {
      pthread_t tid; 
      cpu_set_t cpuSet;
      
      CPU_ZERO(&cpuSet);
      // pin to a core, round robined over the physical cores
      CPU_SET(i, &cpuSet);
      uintptr_t id = i;
      pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuSet);
      if (i>0)   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      if (pthread_create(&tid, &attr, run, (void *)id) != 0) {
	perror("pthread_create");
	assert(0);
      }
      if (i==0) rc=tid;
    }
    while (running_ < n);
    return rc;
  }

  ebbrt::Cpu* Cpu::GetByIndex(size_t index) {
    if (index > numCpus-1) {
      return nullptr;
    }
    return (ebbrt::Cpu *)&(cpus[index]);
  }

  size_t Cpu::Count() { return numCpus; }
  
}  // namespace ebbrt

#ifdef FRONTENDIO_TEST
void 
frontendio_test()
{
  printf("Enter lines of characters ('.' to terminte):\n");
  UNIX::sin->async_read_start([bindir](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
	if (buf->Data()==NULL) {  printf("Stream EOF\n"); return; }
	do {
	  size_t n = write(STDOUT_FILENO, buf->Data(), buf->Length()); 
	  if (n<=0) throw std::runtime_error("write to stdout failed");
	  if (buf->Data()[0] == '.') {
	    UNIX::sin->async_read_stop();
	    printf("open and dump /etc/passwd a stream"); 
	    auto fistream = UNIX::root_fs->openInputStream("/etc/passwd");
	    fistream.Then([bindir](ebbrt::Future<ebbrt::EbbRef<UNIX::InputStream>> fis) {
		
		ebbrt::EbbRef<UNIX::InputStream> is = fis.Get();		  
		is->async_read_start([](std::unique_ptr<ebbrt::IOBuf> buf,size_t avail) {
		    if (buf->Data() == NULL) { printf("Stream EOF\n"); return; }
		    do {
		      size_t n = write(STDOUT_FILENO, buf->Data(), buf->Length()); 
		      if (n<=0) throw std::runtime_error("write to stdout failed");
		    } while(buf->Pop()!=nullptr);
		  });
		ebbrt::node_allocator->AllocateNode(bindir.string());
	      });
	    break;
	  }
	} while(buf->Pop()!=nullptr);
    });
}
#endif

struct MainArgs margs;

void
Main(void)
{
  ebbrt::event_manager->Spawn([=]() {
#ifdef __TRACE_SPAWN__
      printf("spawned: %zd %zd: %s:%d\n", size_t(ebbrt::Cpu::GetMine()), 
	     size_t(*ebbrt::active_context),
	     __FILE__, __LINE__);
#endif
      AppMain();
    }
  );
}

int 
main(int argc, char **argv)
{
  void *status;
  margs = { argc, argv };

  pthread_t tid=ebbrt::Cpu::EarlyInit();

  pthread_join(tid, &status);

  printf("YIKES tid=%zx exited\n!!!", tid);
  return 0;
}
