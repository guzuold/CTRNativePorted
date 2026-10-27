// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
#include <array>
namespace ctrport {
// Transaction boundary for separately stored depth and stencil planes. The
// provider must download/retile the actual GPU generation; this owner supplies
// no PS4 layout formula and cannot grant completion without every plane.
struct DepthGuestWriteback {
 uint64_t token=0;std::array<ComputeGuestWriteback,2> planes{};
 uint32_t count=0;bool completed=false,poisoned=false;
 // Capture before the first GPU initialization/writer. The caller must supply
 // spans validated by its public layout provider; this method derives no layout.
 // Commit the owner only after every read completes, preserving caller state
 // on malformed spans, inaccessible second planes, or overlapping guest memory.
 static int capture(const CtrGuestCopyApi& memory,uint64_t owner,const uint64_t* addresses,const uint64_t* lengths,uint32_t planes_count,DepthGuestWriteback& output){
  if(!owner || !addresses || !lengths || !planes_count || planes_count>2)return 60;
  if(output.token || output.count || output.completed || output.poisoned)return 91;
  for(uint32_t i=0;i<planes_count;++i){
   if(!addresses[i] || !lengths[i] || lengths[i]>32*1024*1024 || addresses[i]>UINT64_MAX-lengths[i])return 60;
   for(uint32_t j=0;j<i;++j)if(addresses[i]<addresses[j]+lengths[j] && addresses[j]<addresses[i]+lengths[i])return 60;
  }
  try{DepthGuestWriteback pending;pending.token=owner;pending.count=planes_count;
   for(uint32_t i=0;i<planes_count;++i){auto& plane=pending.planes[i];plane.address=addresses[i];plane.initial.resize(static_cast<size_t>(lengths[i]));
    const int status=image_guest_copy(memory,plane.address,plane.initial.data(),lengths[i],false);if(status)return status;
   }
   // Optimistic group consistency: a CPU writer may alter the first plane
   // while the second is read. No owner is published until all spans still
   // match. This is validation, not a lock on guest threads; commit rechecks.
   for(uint32_t i=0;i<planes_count;++i){const auto& plane=pending.planes[i];std::vector<unsigned char> check(plane.initial.size());
    const int status=image_guest_copy(memory,plane.address,check.data(),check.size(),false);if(status)return status;
    if(check!=plane.initial)return 88;
   }
   output=std::move(pending);return 0;
  }catch(...){return 112;}
 }
 using Download=int(*)(void*,uint32_t,const unsigned char*,uint64_t,unsigned char*,uint64_t);
 int commit(const CtrGuestCopyApi& memory,Download download,void* context){
  if(completed || poisoned)return 91;
  if(!token || !count || count>2 || memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !memory.write)return 60;
  if(!download)return 88;
  std::array<std::vector<unsigned char>,2> outputs;
  try{
   for(uint32_t i=0;i<count;++i){const auto& p=planes[i];
    if(p.completed || p.poisoned || !p.address || p.initial.empty() || p.initial.size()>32*1024*1024 || p.address>UINT64_MAX-p.initial.size())return 60;
    for(uint32_t j=0;j<i;++j)if(p.address<planes[j].address+planes[j].initial.size() && planes[j].address<p.address+p.initial.size())return 60;
   }
   // Finish all GPU transfers before beginning either guest publication.
   for(uint32_t i=0;i<count;++i){const auto& p=planes[i];outputs[i].resize(p.initial.size());
    const int status=download(context,i,p.initial.data(),p.initial.size(),outputs[i].data(),outputs[i].size());if(status)return status;
   }
   for(uint32_t i=0;i<count;++i){std::vector<unsigned char> current(planes[i].initial.size());
    const int status=image_guest_copy(memory,planes[i].address,current.data(),current.size(),false);if(status)return status;
    if(current!=planes[i].initial)return 88;
   }
   // Once any publication is attempted, a partial result may never release
   // the dirty owner or signal EOP. Failure deliberately forbids retry.
   poisoned=true;
   for(uint32_t i=0;i<count;++i){
    const auto copy=+[](void* raw,const unsigned char*,uint64_t bytes,unsigned char* result,uint64_t capacity)->int{
     const auto& data=*static_cast<const std::vector<unsigned char>*>(raw);
     if(bytes!=data.size() || capacity!=bytes)return 60;std::copy(data.begin(),data.end(),result);return 0;
    };
    const int status=planes[i].commit(memory,copy,&outputs[i]);if(status)return status;
   }
   // Recheck the group after its last write: a concurrent writer may have
   // changed the first plane while the second was being published.
   for(uint32_t i=0;i<count;++i){std::vector<unsigned char> current(planes[i].initial.size());
    const int status=image_guest_copy(memory,planes[i].address,current.data(),current.size(),false);if(status)return status;
    if(current!=planes[i].initial)return 112;
   }
   completed=true;poisoned=false;return 0;
  }catch(...){return 112;}
 }
};

}
