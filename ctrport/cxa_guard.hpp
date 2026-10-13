// SPDX-License-Identifier: GPL-2.0-only
// Independent Itanium C++ ABI 3.3.3 implementation, Windows synchronization.
// https://itanium-cxx-abi.github.io/cxx-abi/abi.html#once-ctor
#pragma once
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <future>
namespace ctrport {
class CxaGuards {
 struct State {std::thread::id owner;std::condition_variable changed;};
 std::mutex mutex;std::unordered_map<void*,std::unique_ptr<State>> states;
public:
 int acquire(std::uint64_t* guard){
  if(!guard || reinterpret_cast<uintptr_t>(guard)%8)throw std::runtime_error("Invalid C++ guard");
  require_time_output(guard,8);std::unique_lock<std::mutex> lock(mutex);
  auto& slot=states[guard];if(!slot)slot=std::make_unique<State>();auto& state=*slot;
  while(state.owner!=std::thread::id{}){if(state.owner==std::this_thread::get_id())throw std::runtime_error("Recursive C++ static initialization");state.changed.wait(lock);}
  if(*reinterpret_cast<volatile unsigned char*>(guard))return 0;
  state.owner=std::this_thread::get_id();return 1;
 }
 void finish(std::uint64_t* guard,bool complete){
  if(!guard || reinterpret_cast<uintptr_t>(guard)%8)throw std::runtime_error("Invalid C++ guard");
  require_time_output(guard,8);std::lock_guard<std::mutex> lock(mutex);auto it=states.find(guard);
  if(it==states.end() || it->second->owner!=std::this_thread::get_id())throw std::runtime_error("C++ guard completion by non-owner");
  if(complete){std::atomic_thread_fence(std::memory_order_release);*reinterpret_cast<volatile unsigned char*>(guard)=1;}
  it->second->owner={};it->second->changed.notify_all();
 }
};
inline CxaGuards cxa_guards;
extern "C" int ctr_cxa_guard_acquire(uint64_t* guard,uint64_t caller){
 try{int result=cxa_guards.acquire(guard);std::printf("{\"scope\":\"original_cxa_guard_acquire_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=3GPpjQdAMTw library=libSceLibcInternal caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_cxa_guard_release(uint64_t* guard,uint64_t caller){try{cxa_guards.finish(guard,true);}catch(const std::exception& e){std::fprintf(stderr,"NID=9rAeANT2tyE library=libSceLibcInternal caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_cxa_guard_abort(uint64_t* guard,uint64_t caller){try{cxa_guards.finish(guard,false);}catch(const std::exception& e){std::fprintf(stderr,"NID=2emaaluWzUw library=libSceLibcInternal caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_sysv_cxa_guard_acquire();
extern "C" void ctr_sysv_cxa_guard_release();
extern "C" void ctr_sysv_cxa_guard_abort();
inline int cxa_guard_control(){
 alignas(8) uint64_t guard=0;auto address=reinterpret_cast<uint64_t>(&guard);
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_cxa_guard_acquire),address,0)!=1 || guard)return 1;
 ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_cxa_guard_abort),address,0);
 if(guard || ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_cxa_guard_acquire),address,0)!=1)return 2;
 bool recursive=false;try{cxa_guards.acquire(&guard);}catch(...){recursive=true;}if(!recursive)return 3;
 std::promise<void> entered;auto started=entered.get_future();auto waiting=std::async(std::launch::async,[&]{entered.set_value();return cxa_guards.acquire(&guard);});started.wait();
 if(waiting.wait_for(std::chrono::milliseconds(20))!=std::future_status::timeout)return 4;
 ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_cxa_guard_release),address,0);
 if(waiting.get()!=0 || guard!=1 || cxa_guards.acquire(&guard)!=0)return 5;
 bool rejected=false;try{cxa_guards.finish(&guard,false);}catch(...){rejected=true;}if(!rejected)return 6;
 std::cout<<"{\"scope\":\"cxa_guard_control\",\"passed\":true,\"abort_retry\":true,\"real_foreign_wait\":true,\"release_publication\":true,\"recursive_rejected\":true}\n";return 0;
}
}
