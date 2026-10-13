// SPDX-License-Identifier: GPL-2.0-or-later
// Context identity/validation adapted from shadPS4 ajm.cpp136 and
// ajm_context.cpp24, e3ce810f3a653f43ac64ebab63023de281a4103a.
// Runtime-owned worker/queue. No codec, guest batch or audio output is stubbed.
#pragma once
#include "kernel_time.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <thread>
#include "../engine/shadPS4/externals/LibAtrac9/C/src/libatrac9.h"
namespace ctrport {
class AudioJobContext {
 std::mutex mutex;std::condition_variable wake;bool stopped=false;
 std::deque<std::function<void()>> jobs;std::thread worker;
 bool at9_registered=false;
public:
 AudioJobContext(){
  std::promise<void> ready;auto started=ready.get_future();
  worker=std::thread([this,signal=std::move(ready)]()mutable{
   signal.set_value();std::unique_lock<std::mutex> lock(mutex);
   for(;;){wake.wait(lock,[this]{return stopped || !jobs.empty();});
    if(stopped && jobs.empty())break;
    auto job=std::move(jobs.front());jobs.pop_front();lock.unlock();job();lock.lock();}
  });started.get();
 }
 ~AudioJobContext(){ {std::lock_guard<std::mutex> lock(mutex);stopped=true;}wake.notify_all();worker.join();}
 int register_codec(uint32_t codec){
  std::lock_guard<std::mutex> lock(mutex);
  if(codec>=23)return static_cast<int>(0x80930005U);
  if(codec!=1)throw std::runtime_error("AJM requested codec factory unavailable");
  if(at9_registered)return static_cast<int>(0x80930009U);
  void* handle=Atrac9GetHandle();if(!handle)return static_cast<int>(0x80930006U);
  Atrac9ReleaseHandle(handle);at9_registered=true;return 0;
 }
 std::future<void> enqueue_host_control(std::function<void()> action){
  auto job=std::make_shared<std::packaged_task<void()>>(std::move(action));auto result=job->get_future();
  {std::lock_guard<std::mutex> lock(mutex);if(stopped)throw std::runtime_error("AJM worker stopped");jobs.push_back([job]{(*job)();});}
  wake.notify_one();return result;
 }
};
class AudioJobContexts {
 std::mutex mutex;std::map<uint32_t,std::shared_ptr<AudioJobContext>> contexts;uint32_t next=1;
public:
 int initialize(int64_t reserved,uint32_t* output){
  if(reserved || !output)return static_cast<int>(0x80930005U);
  require_time_output(output,4);
  auto context=std::make_shared<AudioJobContext>();
  std::lock_guard<std::mutex> lock(mutex);
  if(!next)throw std::runtime_error("AJM context identities exhausted");
  const uint32_t id=next;contexts.emplace(id,std::move(context));*output=id;++next;return 0;
 }
 std::shared_ptr<AudioJobContext> get(uint32_t id){std::lock_guard<std::mutex> lock(mutex);auto i=contexts.find(id);return i==contexts.end()?nullptr:i->second;}
};
inline AudioJobContexts audio_job_contexts;
extern "C" int ctr_ajm_initialize(int64_t reserved,uint32_t* output,uint64_t caller){
 try{const int rc=audio_job_contexts.initialize(reserved,output);
 std::printf("{\"scope\":\"original_AjmInitialize_return\",\"caller\":%llu,\"result\":%d,\"worker_queue_owned\":%s,\"codec_implemented\":false}\n",static_cast<unsigned long long>(caller),rc,rc?"false":"true");std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=dl+4eHSzUu4 library=libSceAjm caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" int ctr_ajm_register(uint32_t context,uint32_t codec,int64_t reserved,uint64_t caller){
 try{int rc=static_cast<int>(0x80930005U);
 if(!reserved){auto owner=audio_job_contexts.get(context);rc=owner?owner->register_codec(codec):static_cast<int>(0x80930002U);}
 std::printf("{\"scope\":\"original_AjmModuleRegister_return\",\"caller\":%llu,\"context\":%u,\"codec\":%u,\"result\":%d,\"native_ATRAC9_factory\":true,\"guest_decode_bound\":false}\n",static_cast<unsigned long long>(caller),context,codec,rc);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=Q3dyFuwGn64 library=libSceAjm caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_ajm_register();
inline int ajm_codec_control(){
 void* handle=Atrac9GetHandle();if(!handle)return 1;
 unsigned char bad[4]={0,0,0,0},good[4]={0xfe,0x74,0x07,0xe0};
 if(Atrac9InitDecoder(handle,bad)==0 || Atrac9InitDecoder(handle,good)!=0){Atrac9ReleaseHandle(handle);return 1;}
 Atrac9CodecInfo info{};if(Atrac9GetCodecInfo(handle,&info) || info.channels!=2 || info.samplingRate!=48000){Atrac9ReleaseHandle(handle);return 1;}Atrac9ReleaseHandle(handle);
 AudioJobContext context;if(context.register_codec(1) || context.register_codec(1)!=static_cast<int>(0x80930009U) || context.register_codec(23)!=static_cast<int>(0x80930005U))return 1;
 uint32_t id=0;if(audio_job_contexts.initialize(0,&id) || ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_ajm_register),id,1,0))return 1;
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_ajm_register),0,1,0))!=static_cast<int>(0x80930002U))return 1;
 std::cout<<"{\"scope\":\"ajm_codec_control\",\"SysV3\":true,\"native_valid_invalid_ATRAC9_configuration\":true,\"stereo_48000\":true,\"duplicate_invalid_context_invalid_codec\":true,\"guest_decode\":false,\"passed\":true}\n";return 0;
}
extern "C" void ctr_sysv_ajm_initialize();
inline int ajm_context_control(){
 AudioJobContexts registry;struct Output{uint32_t before,id,after;};Output out{123,0,456};
 if(registry.initialize(1,&out.id)!=static_cast<int>(0x80930005U) || out.id || registry.initialize(0,nullptr)!=static_cast<int>(0x80930005U))return 1;
 if(registry.initialize(0,&out.id) || out.id!=1 || out.before!=123 || out.after!=456)return 1;
 auto context=registry.get(out.id);if(!context || registry.get(0) || registry.get(99))return 1;
 std::atomic<unsigned> count{0};DWORD host=GetCurrentThreadId(),owner=host;
 auto first=context->enqueue_host_control([&]{owner=GetCurrentThreadId();++count;});
 auto second=context->enqueue_host_control([&]{if(count!=1)throw std::runtime_error("AJM FIFO violated");++count;});
 first.get();second.get();if(count!=2 || host==owner)return 1;
 uint32_t other=0;if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_ajm_initialize),0,reinterpret_cast<uint64_t>(&other)) || !other || !audio_job_contexts.get(other))return 1;
 std::cout<<"{\"scope\":\"ajm_context_control\",\"SysV2\":true,\"real_worker_FIFO\":true,\"invalid_parameters\":2,\"sentinels\":true,\"codec_support\":false,\"passed\":true}\n";return 0;
}
}
