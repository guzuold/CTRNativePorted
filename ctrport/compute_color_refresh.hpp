// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_sampled_refresh.hpp"
#include "color_guest_writeback.hpp"
#include "video/graphics_companion.h"
namespace ctrport {
// Transport proof against a separately allocated full backing reference image.
// Original compute execution and guest writeback are prerequisites, not inferred
// from successful attachment allocation. No pixel payload leaves memory.
inline int refresh_compute_color(void* session,uint64_t color,const CtrGraphicsColorRequirementV1& c,
 const ComputeGuestWriteback& owner,const CtrGuestCopyApi& memory,uint64_t& compared){
 compared=0;if(!owner.completed || owner.poisoned)return 88;
 const uint32_t texel=c.data_format==3?2u:((c.data_format==10 || c.data_format==4)?4u:0u);
 if(!texel || c.number_format!=(c.data_format==4?7u:0u) || c.layers!=1 || c.samples!=1 || c.tile_mode!=14)return 88;
 if(!c.pitch || !c.height || c.pitch>4096 || c.height>4096 || c.bytes!=uint64_t(c.pitch)*c.height*texel || c.bytes>16*1024*1024 || c.guest_address!=owner.address || c.bytes!=owner.initial.size())return 60;
 if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read)return 60;
 try{
  std::vector<unsigned char> current(owner.initial.size()),expected(owner.initial.size()),actual(owner.initial.size());
  int status=memory.read(memory.context,owner.address,current.data(),current.size());if(status)return status;if(current!=owner.initial)return 88;
  CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=p.pitch=c.pitch;p.height=c.height;p.depth=p.layers=p.levels=p.samples=1;p.data_format=c.data_format;p.number_format=c.number_format;p.tile_mode=c.tile_mode;p.guest_address=c.guest_address;
  for(unsigned k=0;k<4;++k)p.swizzle[k]=4+k;
  uint64_t reference=0;status=ctr_video_storage_image_create_v1(session,&p,&reference);if(status)return status;if(!reference)return 112;
  struct Release{void* session;uint64_t token;~Release(){if(token)ctr_video_sampled_image_release_v1(session,token);}}release{session,reference};
  CtrGraphicsImageExtentV1 extent{1,sizeof(extent),c.pitch,c.height,c.guest_address,c.bytes};
  status=ctr_video_sampled_image_upload_tiled_v1(session,reference,owner.initial.data(),c.bytes,&extent);if(status)return status;
  status=ctr_video_sampled_image_readback_linear_v1(session,reference,expected.data(),c.bytes,uint64_t(c.pitch)*texel);if(status)return status;
  status=c.data_format==4?initialize_float_color(session,color,c,owner.initial.data(),c.bytes):ctr_video_color_attachment_upload_tiled_v1(session,color,owner.initial.data(),c.bytes,&extent);if(status)return status;
  status=ctr_video_color_attachment_readback_linear_v1(session,color,actual.data(),c.bytes,uint64_t(c.pitch)*texel);if(status)return status;
  if(actual!=expected)return 112;
  status=memory.read(memory.context,owner.address,current.data(),current.size());if(status)return status;if(current!=owner.initial)return 88;
  status=ctr_video_sampled_image_release_v1(session,reference);release.token=0;if(status)return status;
  compared=c.bytes;return 0;
 }catch(...){return 112;}
}
inline int compute_color_refresh_gpu_control(){
 struct State{unsigned completed=0;};State state;
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  if(!api || !api->context)return 60;auto& state=*static_cast<State*>(raw);
  GuestStack stack(262144,4096);const auto memory=make_owned_guest_copy_api();
  for(const uint32_t format:{3u,10u,4u}){
   const uint64_t bytes=uint64_t(128)*128*(format==3?2:4);
   auto* source=reinterpret_cast<unsigned char*>(stack.bottom());
   CtrGraphicsColorRequirementV1 c{1,sizeof(c),0,128,128,1,1,14,format,format==4?7u:0u,reinterpret_cast<uint64_t>(source),bytes};
   uint64_t color=0;int status=ctr_video_color_attachment_create_v1(api->context,&c,1,&color);if(status)return status;
   struct Release{void* session;uint64_t token;~Release(){ctr_video_color_attachment_release_v1(session,token);}}release{api->context,color};
   for(unsigned pass=0;pass<2;++pass){
    for(size_t k=0;k<bytes;++k)source[k]=static_cast<unsigned char>((k*13+pass*71)^(k>>8));
    ComputeGuestWriteback owner;owner.address=c.guest_address;owner.initial.assign(source,source+bytes);uint64_t compared=99;
    if(refresh_compute_color(api->context,color,c,owner,memory,compared)!=88 || compared)return 1;
    owner.completed=true;owner.poisoned=true;if(refresh_compute_color(api->context,color,c,owner,memory,compared)!=88 || compared)return 2;owner.poisoned=false;
    source[0]^=1;if(refresh_compute_color(api->context,color,c,owner,memory,compared)!=88 || compared)return 3;source[0]^=1;
    status=refresh_compute_color(api->context,color,c,owner,memory,compared);if(status || compared!=bytes)return status?status:112;
    ++state.completed;
   }
  }
  return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};
 const int status=ctr_video_run_window_session_v1(&options,body,&state);if(status || state.completed!=6)return status?status:112;
 std::cout<<"{\"scope\":\"compute_color_refresh_GPU_control\",\"passed\":true,\"actual_color_refreshes\":6,\"formats_RG8_RGBA8_R32float\":true,\"pending_poisoned_stale_guest_rejected\":true,\"synthetic_compute_generation\":true,\"actual_transfer_fences_and_full_comparisons\":true,\"game_draw\":false,\"R3\":false}\n";return 0;
}
}
