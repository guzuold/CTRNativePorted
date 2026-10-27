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
}
