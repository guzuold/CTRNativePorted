// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_executor_host.h"
#include "gnm_submission_snapshot.hpp"
#include "video_session_owner.hpp"
#include "shader_snapshot_owner.hpp"
namespace ctrport {
class GnmExecutorHost {
 std::mutex mutex_;uint64_t next_=1;
 std::map<uint64_t,std::unique_ptr<GnmSubmissionSnapshot>> submissions_;
 std::shared_ptr<DirectMemoryBacking> backing_;
 std::unique_ptr<ShaderSnapshotOwner> inputs_;
 std::map<std::pair<DWORD,uint64_t>,uint64_t> pending_;
 static int capture(void* raw,uint32_t count,uint64_t da,uint64_t ds,uint64_t ca,uint64_t cs,uint64_t* token) noexcept {
  if(!raw || !token)return 60;
  try {auto value=std::make_unique<GnmSubmissionSnapshot>();
   if(!capture_gnm_submission(count,da,ds,ca,cs,*value))return 88;
   auto& self=*static_cast<GnmExecutorHost*>(raw);std::lock_guard<std::mutex> guard(self.mutex_);
   if(!self.next_ || self.submissions_.size()>=4)return 88;const uint64_t identity=self.next_++;
   self.submissions_.emplace(identity,std::move(value));*token=identity;return 0;
  }catch(...){return 112;}
 }
 static int command(void* raw,uint64_t token,uint32_t index,uint32_t constant,const uint32_t** words,uint64_t* count,uint64_t* address) noexcept {
  if(!raw || !words || !count || !address || constant>1)return 60;
  auto& self=*static_cast<GnmExecutorHost*>(raw);std::lock_guard<std::mutex> guard(self.mutex_);
  const auto found=self.submissions_.find(token);if(found==self.submissions_.end())return 62;
  const auto& buffers=constant?found->second->constant:found->second->draw;if(index>=buffers.size())return 60;
  const auto& buffer=buffers[index];*words=buffer.words.empty()?nullptr:buffer.words.data();*count=buffer.words.size();*address=buffer.guest_address;return 0;
 }
 static int release(void* raw,uint64_t token) noexcept {
  if(!raw)return 60;auto& self=*static_cast<GnmExecutorHost*>(raw);std::lock_guard<std::mutex> guard(self.mutex_);
  if(!self.submissions_.erase(token))return 62;
  for(auto it=self.pending_.begin();it!=self.pending_.end();)if(it->second==token)it=self.pending_.erase(it);else ++it;
  return 0;
 }
 static int on_video_owner(void* raw,CtrGnmOwnerActionV1 action,void* argument) noexcept {
  if(!raw || !action)return 60;
  try {std::shared_ptr<RuntimeVideoSession> session;
   {std::lock_guard<std::mutex> guard(runtime_video_mutex);session=runtime_video_session;}
   if(!session)return 88;
   return session->invoke([&](const CtrVideoSessionApiV1* api){return action(argument,api->context);});
  }catch(...){return 112;}
 }
 static int guest_memory(void* raw,CtrGuestCopyApi* output) noexcept {
  if(!raw || !output)return 60;*output=make_owned_guest_copy_api();return 0;
 }
 static int shader_inputs(void* raw,CtrShaderInputsV1* inputs,CtrShaderOwnedRangesV1* ranges) noexcept {
  if(!raw || !inputs || !ranges)return 60;
  try {auto& self=*static_cast<GnmExecutorHost*>(raw);std::lock_guard<std::mutex> guard(self.mutex_);
   auto backing=direct_memory_backing;if(!backing || (self.backing_ && self.backing_!=backing))return 88;
   if(!self.inputs_){auto prepared=std::make_unique<ShaderSnapshotOwner>(backing);self.backing_=backing;self.inputs_=std::move(prepared);}
   *inputs=self.inputs_->api();*ranges=self.inputs_->ranges();return 0;
  }catch(...){return 112;}
 }
 static int pending_commands(void* raw,uint64_t caller,uint64_t* token) noexcept {
  if(!raw || !token)return 60;auto& self=*static_cast<GnmExecutorHost*>(raw);std::lock_guard<std::mutex> guard(self.mutex_);
  const auto at=self.pending_.find({GetCurrentThreadId(),caller});if(at==self.pending_.end())return 62;
  *token=at->second;return 0;
 }
public:
 int retain_pending(uint64_t caller,uint32_t count,uint64_t da,uint64_t ds,uint64_t ca,uint64_t cs){
  uint64_t token=0;const int status=capture(this,count,da,ds,ca,cs,&token);if(status)return status;
  std::lock_guard<std::mutex> guard(mutex_);const auto key=std::make_pair(GetCurrentThreadId(),caller);
  if(pending_.count(key)){submissions_.erase(token);return 88;}
  try {pending_.emplace(key,token);}catch(...){submissions_.erase(token);return 112;}
  uint64_t words=0;for(const auto& command:submissions_.at(token)->draw)words+=command.words.size();
  for(const auto& command:submissions_.at(token)->constant)words+=command.words.size();
  std::fprintf(stderr,"PENDING Gnm owned commands token=%llu buffers=%u words=%llu caller=0x%llx thread=%lu\n",static_cast<unsigned long long>(token),count,static_cast<unsigned long long>(words),static_cast<unsigned long long>(caller),GetCurrentThreadId());std::fflush(stderr);
  return 0;
 }
 CtrGnmExecutorHostV1 api(){return {sizeof(CtrGnmExecutorHostV1),1,this,capture,command,release,on_video_owner,guest_memory,shader_inputs,pending_commands};}
};
inline GnmExecutorHost gnm_executor_host_owner;
inline CtrGnmExecutorHostV1 gnm_executor_host_api=gnm_executor_host_owner.api();
inline int gnm_executor_host_control(bool dll_fixture=false){
 GnmExecutorHost host;const auto api=host.api();GuestStack storage(32768,4096);
 const auto base=reinterpret_cast<uint64_t>(storage.bottom());
 *reinterpret_cast<uint64_t*>(base)=base+256;*reinterpret_cast<uint32_t*>(base+128)=8;
 auto* guest=reinterpret_cast<uint32_t*>(base+256);guest[0]=0xc0001000;guest[1]=7;
 uint64_t token=0;if(api.capture(api.context,1,base,base+128,0,0,&token) || !token)return 1;
 const uint32_t* words=nullptr;uint64_t count=0,address=0;
 if(api.command(api.context,token,0,0,&words,&count,&address) || count!=2 || address!=base+256 || words[0]!=0xc0001000 || words[1]!=7)return 2;
 guest[1]=9;if(words[1]!=7)return 3;
 unsigned rejected=0;const auto* original=words;
 if(api.command(api.context,token,1,0,&words,&count,&address)==60 && words==original && count==2)++rejected;
 if(api.command(api.context,token,0,2,&words,&count,&address)==60 && words==original)++rejected;
 uint64_t unchanged=token;if(api.capture(api.context,0,base,base+128,0,0,&unchanged)==88 && unchanged==token)++rejected;
 if(api.release(api.context,token))return 4;
 if(api.release(api.context,token)==62)++rejected;
 if(api.command(api.context,token,0,0,&words,&count,&address)==62 && words==original && count==2)++rejected;
 std::array<uint64_t,4> held{};
 for(auto& id:held)if(api.capture(api.context,1,base,base+128,0,0,&id))return 5;
 uint64_t overflow=999;if(api.capture(api.context,1,base,base+128,0,0,&overflow)==88 && overflow==999)++rejected;
 for(auto id:held)if(api.release(api.context,id))return 5;
 if(rejected!=6)return 5;
 if(host.retain_pending(123,1,base,base+128,0,0))return 6;
 uint64_t pending=0;if(api.pending_commands(api.context,123,&pending) || !pending)return 6;
 if(api.pending_commands(api.context,124,&unchanged)==62 && unchanged==token)++rejected;
 int other=0;std::thread outsider([&]{uint64_t t=999;other=api.pending_commands(api.context,123,&t);});outsider.join();if(other==62)++rejected;
 if(api.release(api.context,pending))return 6;
 if(api.pending_commands(api.context,123,&unchanged)==62 && unchanged==token)++rejected;
 CtrGuestCopyApi memory{};if(api.guest_memory(api.context,&memory))return 6;
 uint32_t copied=0;if(memory.read(memory.context,base+256+4,&copied,4) || copied!=9)return 6;
 const uint32_t written=11;if(memory.write(memory.context,base+256+4,&written,4) || guest[1]!=11)return 6;guest[1]=9;
 auto previous=direct_memory_backing;auto backing=std::make_shared<DirectMemoryBacking>(16384);
 int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,16384,16384,0,0,&physical) || backing->map(&mapped,16384,51,0,physical,0))return 6;
 direct_memory_backing=backing;CtrShaderInputsV1 inputs{};CtrShaderOwnedRangesV1 ranges{};
 if(api.shader_inputs(api.context,&inputs,&ranges))return 6;
 const auto mapped_address=reinterpret_cast<uint64_t>(mapped);*static_cast<uint32_t*>(mapped)=19;
 CtrShaderRangeLeaseV1 lease{};CtrShaderSnapshot snap{};
 if(ranges.begin(ranges.context,mapped_address,4,&lease) || inputs.capture(inputs.context,mapped_address,4,&snap))return 6;
 *static_cast<uint32_t*>(mapped)=23;if(ctr_shader_snapshot_read(&snap,mapped_address,&copied,4) || copied!=19)return 6;
 if(inputs.release(inputs.context,&snap) || ranges.end(ranges.context,&lease))return 6;
 direct_memory_backing=previous;
 if(api.shader_inputs(api.context,&inputs,&ranges)==88)++rejected;
 else return 6;
 if(runtime_video_open(255,0,0,0)!=1)return 6;
 struct Probe {DWORD thread=0;void* session=nullptr;} probe;
 const auto callback=+[](void* arg,void* session)->int {auto& p=*static_cast<Probe*>(arg);p.thread=GetCurrentThreadId();p.session=session;return 73;};
 const auto thread=GetCurrentThreadId();const int status=api.on_video_owner(api.context,callback,&probe);
 if(status!=73 || probe.thread==thread || !probe.session)return 7;
 if(dll_fixture){
  const auto file=std::filesystem::absolute("build/CTRPort-infra/gnm_executor_host_fixture.dll");
  HMODULE module=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!module)return 8;
  using Fixture=int(*)(const CtrGnmExecutorHostV1*,uint64_t,uint64_t);
  const auto fixture=reinterpret_cast<Fixture>(GetProcAddress(module,"ctr_gnm_host_fixture"));
  const int result=fixture?fixture(&api,base,base+128):99;FreeLibrary(module);if(result)return 9;
 }
 if(runtime_video_close(1))return 7;
 std::cout<<"{\"dll_fixture\":"<<(dll_fixture?"true":"false")<<",\"scope\":\"gnm_executor_host_control\",\"owned_commands\":true,\"owner_dispatch\":true,\"negative_atomic\":10,\"synthetic\":true,\"game_frame\":false}\n";return 0;
}
}
