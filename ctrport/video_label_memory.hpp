// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "direct_mapping_lease.hpp"
namespace ctrport {
// Real guest-visible VideoOut label allocation, not a host registry pointer.
// Renderer must retain this owner until all original/GPU users have stopped.
// Writes do not manufacture SignalVoLabel or GPU completion: the real writer
// must publish labels before invoking the data-owned notification mechanism.
class VideoLabelMemory {
    std::shared_ptr<DirectMemoryBacking> backing_=std::make_shared<DirectMemoryBacking>(16384);
    uint64_t address_=0;
    static int read(void* raw,uint64_t address,void* output,uint64_t bytes) noexcept {
        if(!raw)return 1;auto& self=*static_cast<VideoLabelMemory*>(raw);
        if(!bytes || address<self.address_ || address-self.address_>=128 || bytes>128-(address-self.address_))return 1;
        return direct_copy_read(self.backing_.get(),address,output,bytes);
    }
    static int write(void* raw,uint64_t address,const void* input,uint64_t bytes) noexcept {
        if(!raw)return 1;auto& self=*static_cast<VideoLabelMemory*>(raw);
        if(!bytes || address<self.address_ || address-self.address_>=128 || bytes>128-(address-self.address_))return 1;
        return direct_copy_write(self.backing_.get(),address,input,bytes);
    }
public:
    VideoLabelMemory(){
        int64_t physical=-1;void* pointer=nullptr;
        if(backing_->allocate(0,16384,16384,0,0,&physical) || backing_->map(&pointer,16384,51,0,physical,0))throw std::runtime_error("Video label mapping failed");
        address_=reinterpret_cast<uint64_t>(pointer);
    }
    uint64_t address() const{return address_;}
    CtrGuestCopyApi api(){return {sizeof(CtrGuestCopyApi),1,this,&read,&write};}
};
inline int video_label_memory_control(){
    uint64_t retired=0;
    {
        auto labels=std::make_shared<VideoLabelMemory>();auto api=labels->api();retired=labels->address();
        uint64_t values[16];std::memset(values,0x79,sizeof(values));
        if(api.read(api.context,retired,values,sizeof(values)))return 1;
        for(auto value:values)if(value)return 1;
        values[15]=1;
        if(api.write(api.context,retired,values,sizeof(values)))return 1;
        std::memset(values,0,sizeof(values));if(api.read(api.context,retired,values,sizeof(values)) || values[15]!=1)return 1;
        unsigned rejected=0;
        for(auto location:{retired-1,retired+128,UINT64_MAX-63})if(api.write(api.context,location,values,sizeof(values)))++rejected;
        unsigned char output[129];std::memset(output,0x79,sizeof(output));
        if(api.read(api.context,retired,output,sizeof(output)))++rejected;
        for(auto value:output)if(value!=0x79)return 1;
        if(rejected!=4 || !is_owned_output(retired,retired+128))return 1;
    }
    if(is_owned_output(retired,retired+128))return 1;
    std::cout<<"{\"scope\":\"owned_video_label_memory_control\",\"guest_visible_bytes\":128,\"zero_initialized\":true,\"roundtrip_last_label\":1,\"negative_rejections\":4,\"revoked_after_owner\":true,\"GPU_completion\":false,\"game_frame\":false}\n";return 0;
}
}
