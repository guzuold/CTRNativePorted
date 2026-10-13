// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <set>
#include <atomic>
#include <array>
#include <thread>
#include "direct_memory_copy.hpp"
#include "video/shader_inputs.h"
#include "shader_owned_ranges.h"
#include "snapshot_phase_timing.hpp"
namespace ctrport {
// CPU snapshots of explicitly owned direct mappings. Source synchronization is
// the caller's responsibility; this is not GPU coherence or a compiler binding.
// No borrowed guest pointers escape. Context must outlive all callbacks/tokens.
class ShaderSnapshotOwner {
 struct Snapshot {uint64_t address;std::vector<unsigned char> bytes;};
 std::shared_ptr<DirectMemoryBacking> backing_;
 std::mutex mutex_;uint64_t next_=1;
 uint64_t range_begins_=0,range_ends_=0;
 std::map<uint64_t,std::unique_ptr<Snapshot>> snapshots_;
 struct Lease {std::unique_lock<std::recursive_mutex> transaction;DWORD thread;CtrShaderRangeLeaseV1 identity;};
 std::map<uint64_t,std::unique_ptr<Lease>> leases_;
 static int begin_range(void* context,uint64_t address,uint64_t cap,CtrShaderRangeLeaseV1* out){
  if(!context || !out || !address || !cap || cap>32*1024*1024)return 60;
  auto& self=*static_cast<ShaderSnapshotOwner*>(context);
  try{if(!self.backing_)return 88;SnapshotPhaseTiming timing("range",cap);auto transaction=self.backing_->copy_transaction();timing.Next("extent_probe");
   const auto bytes=self.backing_->readable_mapped_extent(address,cap,32*1024*1024);if(!bytes)return 88;timing.Next("publish_lease");
   std::lock_guard<std::mutex> guard(self.mutex_);if(!self.next_)return 88;
   const CtrShaderRangeLeaseV1 identity{address,bytes,self.next_++};
   auto lease=std::make_unique<Lease>(Lease{std::move(transaction),GetCurrentThreadId(),identity});
   self.leases_.emplace(identity.token,std::move(lease));++self.range_begins_;*out=identity;return 0;
  }catch(...){return 112;}
 }
 static int end_range(void* context,const CtrShaderRangeLeaseV1* input){
  if(!context || !input)return 60;auto& self=*static_cast<ShaderSnapshotOwner*>(context);
  std::lock_guard<std::mutex> guard(self.mutex_);const auto at=self.leases_.find(input->token);
  if(at==self.leases_.end())return 88;const auto& lease=*at->second;
  if(lease.thread!=GetCurrentThreadId())return 116;
  if(input->guest_address!=lease.identity.guest_address || input->bytes!=lease.identity.bytes)return 88;
  self.leases_.erase(at);++self.range_ends_;return 0;
 }
 static int capture_callback(void* context,uint64_t address,uint64_t bytes,CtrShaderSnapshot* out){
  if(!context || !out || !address || !bytes || bytes>32*1024*1024 || address>UINT64_MAX-bytes)return 60;
  auto& self=*static_cast<ShaderSnapshotOwner*>(context);
  try{if(!self.backing_)return 88;SnapshotPhaseTiming timing("capture",bytes);auto transaction=self.backing_->copy_transaction();timing.Next("allocate_copy_validate");
   auto prepared=std::make_unique<Snapshot>();prepared->address=address;prepared->bytes.resize(static_cast<size_t>(bytes));
   self.backing_->copy_mapped(address,prepared->bytes.data(),static_cast<size_t>(bytes),false,32*1024*1024);timing.Next("publish_snapshot");
   std::lock_guard<std::mutex> lock(self.mutex_);if(!self.next_)return 88;const uint64_t token=self.next_++;
   CtrShaderSnapshot result{prepared->bytes.data(),bytes,address,token};self.snapshots_.emplace(token,std::move(prepared));*out=result;return 0;
  }catch(...){return 112;}
 }
 static int release_callback(void* context,const CtrShaderSnapshot* snapshot){
  if(!context || !snapshot)return 60;auto& self=*static_cast<ShaderSnapshotOwner*>(context);std::lock_guard<std::mutex> lock(self.mutex_);
  const auto found=self.snapshots_.find(snapshot->snapshot_token);if(found==self.snapshots_.end())return 88;const auto& value=*found->second;
  if(snapshot->bytes!=value.bytes.data() || snapshot->size!=value.bytes.size() || snapshot->guest_address!=value.address)return 88;
  self.snapshots_.erase(found);return 0;
 }
public:
 explicit ShaderSnapshotOwner(std::shared_ptr<DirectMemoryBacking> backing):backing_(std::move(backing)){}
 CtrShaderInputsV1 api(){return {1,sizeof(CtrShaderInputsV1),this,capture_callback,release_callback};}
 CtrShaderOwnedRangesV1 ranges(){return {1,sizeof(CtrShaderOwnedRangesV1),this,begin_range,end_range};}
 // Read only after joining synchronous compiler work. Counts prove cleanup,
 // not source synchronization, descriptor semantics or GPU coherence.
 std::array<uint64_t,4> lifetime_counts(){std::lock_guard<std::mutex> lock(mutex_);return {range_begins_,range_ends_,leases_.size(),snapshots_.size()};}
};
inline int shader_owned_ranges_control(){
 auto backing=std::make_shared<DirectMemoryBacking>(65536);int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,65536,32768,16384,0,&physical)||backing->map(&mapped,32768,51,0,physical,0))return 1;
 ShaderSnapshotOwner owner(backing);const auto ranges=owner.ranges();const auto inputs=owner.api();
 const auto address=reinterpret_cast<uint64_t>(mapped);CtrShaderRangeLeaseV1 lease{};
 if(ranges.begin(ranges.context,address+32,65536,&lease)||lease.bytes!=32736)return 1;
 CtrShaderSnapshot snap{};if(inputs.capture(inputs.context,lease.guest_address,lease.bytes,&snap)||inputs.release(inputs.context,&snap))return 1;
 unsigned rejected=0;auto forged=lease;--forged.bytes;if(ranges.end(ranges.context,&forged)==88)++rejected;
 int wrong=0;std::thread worker([&]{wrong=ranges.end(ranges.context,&lease);});worker.join();if(wrong==116)++rejected;
 if(ranges.end(ranges.context,&lease))return 1;if(ranges.end(ranges.context,&lease)==88)++rejected;
 for(auto bad:{uint64_t(1),UINT64_MAX,address+32768}){auto output=lease;
  if(ranges.begin(ranges.context,bad,64,&output)==88 && output.token==lease.token)++rejected;}
 DWORD previous=0;if(!VirtualProtect(static_cast<unsigned char*>(mapped)+16384,16384,PAGE_NOACCESS,&previous))return 1;
 if(ranges.begin(ranges.context,address,32768,&lease)||lease.bytes!=16384 || ranges.end(ranges.context,&lease))return 1;
 if(!VirtualProtect(static_cast<unsigned char*>(mapped)+16384,16384,previous,&previous))return 1;
 if(rejected!=6)return 1;
 std::cout<<"{\"scope\":\"shader_owned_ranges_control\",\"passed\":true,\"negative_rejections\":6,\"readable_prefix\":16384,\"synthetic\":true,\"source_writer_coherence\":false}\n";return 0;
}
inline int shader_large_image_snapshot_control(){
 constexpr uint64_t size=17694720;
 auto backing=std::make_shared<DirectMemoryBacking>(32*1024*1024);int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,size,size,0,0,&physical)||backing->map(&mapped,size,51,0,physical,0))return 1;
 const auto address=reinterpret_cast<uint64_t>(mapped);auto* bytes=static_cast<unsigned char*>(mapped);bytes[0]=17;bytes[size-1]=29;
 ShaderSnapshotOwner owner(backing);const auto ranges=owner.ranges();const auto inputs=owner.api();CtrShaderRangeLeaseV1 lease{};CtrShaderSnapshot snapshot{};
 if(ranges.begin(ranges.context,address,size,&lease) || lease.bytes!=size || inputs.capture(inputs.context,address,size,&snapshot))return 2;
 unsigned char first=0,last=0;if(ctr_shader_snapshot_read(&snapshot,address,&first,1) || ctr_shader_snapshot_read(&snapshot,address+size-1,&last,1) || first!=17 || last!=29)return 3;
 bytes[0]=99;if(ctr_shader_snapshot_read(&snapshot,address,&first,1) || first!=17)return 4;
 if(inputs.release(inputs.context,&snapshot) || ranges.end(ranges.context,&lease))return 5;
 unsigned rejected=0;auto preserved=lease;if(ranges.begin(ranges.context,address,32ull*1024*1024+1,&preserved)==60 && preserved.token==lease.token)++rejected;
 auto output=snapshot;if(inputs.capture(inputs.context,address,32ull*1024*1024+1,&output)==60 && output.snapshot_token==snapshot.snapshot_token)++rejected;
 const auto counts=owner.lifetime_counts();if(rejected!=2 || counts[0]!=counts[1] || counts[2] || counts[3])return 6;
 std::cout<<"{\"scope\":\"large_image_snapshot_control\",\"passed\":true,\"bytes\":"<<size<<",\"negative_rejections\":2,\"synthetic\":true,\"source_writer_coherence\":false,\"gpu_execution\":false}\n";return 0;
}
inline int shader_snapshot_owner_control(){
 auto backing=std::make_shared<DirectMemoryBacking>(16384);int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,16384,16384,0,0,&physical)||backing->map(&mapped,16384,51,0,physical,0))return 1;
 const auto address=reinterpret_cast<uint64_t>(mapped);auto* source=static_cast<unsigned char*>(mapped);for(size_t i=0;i<64;++i)source[i]=static_cast<unsigned char>(i);
 ShaderSnapshotOwner owner(backing);auto api=owner.api();CtrShaderSnapshot snapshot{};
 if(api.capture(api.context,address,64,&snapshot))return 1;source[0]=255;unsigned char result[64]{};
 if(ctr_shader_snapshot_read(&snapshot,address,result,64) || result[0] || result[63]!=63)return 1;
 unsigned rejected=0;auto forged=snapshot;forged.size=63;if(api.release(api.context,&forged))++rejected;
 forged=snapshot;forged.bytes=source;if(api.release(api.context,&forged))++rejected;
 CtrShaderSnapshot preserved=snapshot;
 for(const auto a:{UINT64_MAX-31,address+16384-32})if(api.capture(api.context,a,64,&preserved) && preserved.snapshot_token==snapshot.snapshot_token && preserved.bytes==snapshot.bytes)++rejected;
 if(api.release(api.context,&snapshot))return 1;if(api.release(api.context,&snapshot))++rejected;
 if(rejected!=5)return 1;
 std::atomic<unsigned> failed{0};std::mutex tokens_mutex;std::set<uint64_t> tokens;std::vector<std::thread> workers;
 for(unsigned t=0;t<8;++t)workers.emplace_back([&]{for(unsigned call=0;call<32;++call){CtrShaderSnapshot current{};if(api.capture(api.context,address,64,&current)){++failed;continue;}
  unsigned char copy[64]{};if(ctr_shader_snapshot_read(&current,address,copy,64) || copy[0]!=255 || copy[63]!=63)++failed;
  {std::lock_guard<std::mutex> lock(tokens_mutex);if(!tokens.insert(current.snapshot_token).second)++failed;}
  if(api.release(api.context,&current))++failed;
 }});
 for(auto& worker:workers)worker.join();if(failed || tokens.size()!=256)return 1;

 std::cout<<"{\"scope\":\"shader_snapshot_owner_control\",\"passed\":true,\"immutable_copy\":true,\"negative_rejections\":5,\"concurrent_snapshots\":256,\"synthetic\":true,\"GPU_coherence\":false,\"compiler_bound\":false}\n";return 0;
}
}
