// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
namespace ctrport {
// An exact guest publication authorizes a later sampled snapshot only while no
// subsequent color writer overlaps it. Keep full bytes in memory, never on disk.
class PublishedColorGenerations {
 struct Entry{uint64_t address;std::vector<unsigned char> bytes;size_t epoch;};
 std::vector<Entry> entries_;
 static bool overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m){return n && m && a<=UINT64_MAX-n && b<=UINT64_MAX-m && a<b+m && b<a+n;}
public:
 int publish(const ComputeGuestWriteback& writer,size_t epoch){
  if(!writer.completed || writer.poisoned)return 91;
  if(!writer.address || writer.initial.empty() || writer.initial.size()>32*1024*1024 || writer.address>UINT64_MAX-writer.initial.size())return 60;
  entries_.push_back({writer.address,writer.initial,epoch});return 0;
 }
 // Reuse already verified pre-writer publication bytes, never capture new
 // CPU contents after a GPU writer. Commit still rechecks actual guest bytes.
 int baseline(uint64_t address,uint64_t bytes,const std::vector<std::pair<uint64_t,uint64_t>>& writers,size_t before,std::vector<unsigned char>& output)const{
  if(before>writers.size())return 60;
  for(auto entry=entries_.rbegin();entry!=entries_.rend();++entry)if(entry->address==address && entry->bytes.size()==bytes){
   if(entry->epoch>before)return 88;
   for(size_t k=entry->epoch;k<before;++k)if(overlap(address,bytes,writers[k].first,writers[k].second))return 88;
   output=entry->bytes;return 0;
  }return 62;
 }
 bool matches(uint64_t address,uint64_t bytes,const std::vector<unsigned char>& snapshot,const std::vector<std::pair<uint64_t,uint64_t>>& writers)const{
  if(!bytes || address>UINT64_MAX-bytes || snapshot.size()!=bytes)return false;
  for(auto entry=entries_.rbegin();entry!=entries_.rend();++entry){
   if(entry->address!=address || entry->bytes.size()!=bytes)continue;
   if(entry->epoch>writers.size() || entry->bytes!=snapshot)return false;
   for(size_t k=entry->epoch;k<writers.size();++k)if(overlap(address,bytes,writers[k].first,writers[k].second))return false;
   return true;
  }return false;
 }
};
inline int published_color_generations_control(){
 PublishedColorGenerations owner;ComputeGuestWriteback writer;writer.address=0x1000;writer.initial.assign(64,0x25);
 if(owner.publish(writer,1)!=91)return 1;writer.completed=true;writer.poisoned=true;if(owner.publish(writer,1)!=91)return 2;writer.poisoned=false;
 if(owner.publish(writer,1))return 3;
 std::vector<std::pair<uint64_t,uint64_t>> ranges{{0x1000,64}};
 if(!owner.matches(writer.address,64,writer.initial,ranges))return 4;
 auto changed=writer.initial;changed[63]=0x26;if(owner.matches(writer.address,64,changed,ranges))return 5;
 if(owner.matches(0x1001,64,writer.initial,ranges) || owner.matches(UINT64_MAX-32,64,writer.initial,ranges))return 6;
 if(owner.matches(writer.address,64,writer.initial,{}))return 7;
 ranges.push_back({0x2000,64});if(!owner.matches(writer.address,64,writer.initial,ranges))return 8;
 ranges.push_back({0x103f,1});if(owner.matches(writer.address,64,writer.initial,ranges))return 9;
 writer.initial=changed;if(owner.publish(writer,ranges.size()))return 10;
 if(!owner.matches(writer.address,64,changed,ranges) || owner.matches(writer.address,64,std::vector<unsigned char>(64,0x25),ranges))return 11;
 // Repeated render-target writes use the last completed publication as the
 // optimistic commit baseline. A later overlapping writer cannot authorize it.
 std::vector<unsigned char> baseline(64,0xa5);
 if(owner.baseline(writer.address,64,ranges,ranges.size(),baseline) || baseline!=changed)return 12;
 auto prior=baseline;ranges.push_back({writer.address+63,1});
 if(owner.baseline(writer.address,64,ranges,ranges.size(),baseline)!=88 || baseline!=prior)return 13;
 if(owner.baseline(writer.address,64,ranges,1,baseline)!=88 || baseline!=prior)return 14;
 if(owner.baseline(writer.address,64,ranges,ranges.size()+1,baseline)!=60 || baseline!=prior)return 15;
 if(owner.baseline(writer.address+1,64,ranges,ranges.size(),baseline)!=62 || baseline!=prior)return 16;
 std::cout<<"{\"scope\":\"published_color_generations_control\",\"passed\":true,\"synthetic_metadata_and_bytes\":true,\"pending_poisoned_stale_partial_overflow_future_epoch_and_superseded_rejected\":true,\"newest_publication_authority\":true,\"repeated_writer_baseline_atomic_controls\":5,\"GPU_execution\":false}\n";return 0;
}
}
