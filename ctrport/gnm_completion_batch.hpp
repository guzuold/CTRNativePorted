// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
#include "gnm_eop_completion.hpp"
namespace ctrport {
// An ordered submission's explicitly enrolled image writers. Each provider must
// download and retile the actual current GPU generation. This owner cannot infer
// that unregistered resources are coherent; enrollment belongs to the executor.
// Resume skips verified publications, so a later pending image never republishes
// an earlier image. Caller retains provider contexts and Vulkan token owners.
class GnmCompletionBatch {
 struct Image {ComputeGuestWriteback writeback;ComputeTiledDownload download;void* context;};
 std::vector<Image> images_;size_t next_=0;bool sealed_=false,completed_=false,running_=false;
public:
 int add(uint64_t address,std::vector<unsigned char> initial,ComputeTiledDownload download,void* context){
  if(sealed_)return 91;
  if(!address || initial.empty() || initial.size()>32*1024*1024 || address>UINT64_MAX-initial.size())return 60;
  for(const auto& image:images_)if(address<image.writeback.address+image.writeback.initial.size() && image.writeback.address<address+initial.size())return 88;
  ComputeGuestWriteback writer;writer.address=address;writer.initial=std::move(initial);
  images_.push_back({std::move(writer),download,context});return 0;
 }
 void seal(){sealed_=true;}
 size_t published()const{return next_;}bool completed()const{return completed_;}
 int finish(const CtrGuestCopyApi& memory,GnmCompletePriorGpuWork fence,void* context){
  if(!sealed_)return 88;if(running_)return 91;if(completed_)return 0;if(!fence)return 88;
  if(memory.size!=sizeof(memory) || memory.version!=1 || !memory.read || !memory.write)return 60;
  running_=true;struct Reset{bool& value;~Reset(){value=false;}}reset{running_};
  while(next_<images_.size()){
   auto& image=images_[next_];const int status=image.writeback.commit(memory,image.download,image.context);
   if(status)return status;++next_;
  }
  int status=112;try{
   // An earlier publication may have waited for a later image. Revalidate the
   // entire guest generation before granting completion authority to the EOP.
   for(const auto& image:images_){std::vector<unsigned char> current(image.writeback.initial.size());
    status=image_guest_copy(memory,image.writeback.address,current.data(),current.size(),false);if(status)return status;
    if(current!=image.writeback.initial)return 88;
   }
   status=fence(context);
  }catch(...){return 112;}
  if(!status)completed_=true;return status;
 }
};
}
