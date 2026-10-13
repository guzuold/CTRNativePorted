// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
#include "video/session_api.h"
namespace ctrport {
// Bridge actual completed RGBA8 color generation to the existing whole-public
// GPU retile provider. No CPU rendering or tiled-layout formula; unsupported
// formats remain explicit88. RG8 uses a fenced color readback and real storage
// image upload because the sampled color-copy adapter admits only RGBA8.
// R32float uses the same fenced raw-bit storage transfer when the selected
// session admits exact format4/number7; missing capability remains88.
// Exact R32float initialization through the public storage-image detiler.
// Full raw-bit comparison is required before the attachment is admitted.
inline int initialize_float_color(void* session,uint64_t color,const CtrGraphicsColorRequirementV1& c,const void* source,uint64_t bytes){
 if(!source || c.data_format!=4 || c.number_format!=7 || c.tile_mode!=14 || c.samples!=1 || c.layers!=1 || !c.pitch || !c.height || c.pitch>4096 || c.height>4096 || bytes!=c.bytes || bytes!=uint64_t(c.pitch)*c.height*4 || bytes>32*1024*1024)return 88;
 CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=p.pitch=c.pitch;p.height=c.height;p.depth=p.layers=p.levels=p.samples=1;p.data_format=4;p.number_format=7;p.tile_mode=14;p.guest_address=c.guest_address;
 uint64_t token=0;int status=ctr_video_storage_image_create_v1(session,&p,&token);if(status)return status;
 struct Cleanup{void* session;uint64_t token;~Cleanup(){ctr_video_sampled_image_release_v1(session,token);}}cleanup{session,token};
 CtrGraphicsImageExtentV1 extent{1,sizeof(extent),c.pitch,c.height,c.guest_address,bytes};
 status=ctr_video_sampled_image_upload_tiled_v1(session,token,source,bytes,&extent);if(status)return status;
 std::vector<unsigned char> linear(static_cast<size_t>(bytes)),actual(static_cast<size_t>(bytes));
 status=ctr_video_sampled_image_readback_linear_v1(session,token,linear.data(),bytes,uint64_t(c.pitch)*4);if(status)return status;
 status=ctr_video_color_attachment_upload_linear_v1(session,color,linear.data(),bytes,uint64_t(c.pitch)*4);if(status)return status;
 status=ctr_video_color_attachment_readback_linear_v1(session,color,actual.data(),bytes,uint64_t(c.pitch)*4);if(status)return status;
 return linear==actual?0:112;
}
struct ColorGuestDownload {
 void* session=nullptr;uint64_t color=0;CtrGraphicsColorRequirementV1 shape{};
 static int download(void* raw,const unsigned char* prior,uint64_t length,unsigned char* output,uint64_t capacity){
  if(!raw || !prior || !output)return 60;const auto& d=*static_cast<ColorGuestDownload*>(raw);const auto& c=d.shape;
  if(!d.session || !d.color || c.version!=1 || c.size!=sizeof(c))return 60;
  const uint64_t texel_bytes=c.data_format==3?2u:4u;
  if((c.data_format!=10 && c.data_format!=3 && c.data_format!=4) || c.number_format!=(c.data_format==4?7u:0u) || (c.tile_mode!=14 && !(c.tile_mode==10 && c.data_format==10)) || c.samples!=1 || c.layers!=1 || !c.pitch || !c.height || c.pitch>4096 || c.height>4096 || c.bytes!=uint64_t(c.pitch)*c.height*texel_bytes || length!=c.bytes || capacity!=length)return 88;
  CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=p.pitch=c.pitch;p.height=c.height;p.depth=p.layers=p.levels=p.samples=1;p.data_format=c.data_format;p.number_format=c.number_format;p.tile_mode=c.tile_mode;p.guest_address=c.guest_address;
  uint64_t token=0;int status=c.data_format!=10?ctr_video_storage_image_create_v1(d.session,&p,&token):ctr_video_sampled_image_create_v1(d.session,&p,&token);if(status)return status;
  struct Cleanup{void* session;uint64_t token;~Cleanup(){ctr_video_sampled_image_release_v1(session,token);}}cleanup{d.session,token};
  if(c.data_format!=10){std::vector<unsigned char> linear(static_cast<size_t>(length));
   status=ctr_video_color_attachment_readback_linear_v1(d.session,d.color,linear.data(),length,uint64_t(c.pitch)*texel_bytes);
   if(!status)status=ctr_video_sampled_image_upload_linear_v1(d.session,token,linear.data(),length,uint64_t(c.pitch)*texel_bytes);
  }else status=ctr_video_color_to_sampled_v1(d.session,d.color,token);if(status)return status;
  CtrGraphicsImageExtentV1 extent{1,sizeof(extent),c.pitch,c.height,c.guest_address,c.bytes};
  return ctr_video_image_readback_tiled_v1(d.session,token,prior,length,&extent,output);
 }
};
inline int color_guest_writeback_gpu_control(bool include_float=false,bool display10=false){
 struct State{bool observed=false,include_float=false,display10=false;};State state;state.include_float=include_float;state.display10=display10;
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  if(!api || !api->context)return 60;auto& state=*static_cast<State*>(raw);GuestStack stack(state.display10?16*1024*1024:65536,4096);auto* guest=static_cast<unsigned char*>(stack.bottom());for(const uint32_t format:{10u,3u,4u}){if((format==4 && !state.include_float) || (state.display10 && format!=10))continue;const uint64_t texel_bytes=format==3?2u:4u;const uint32_t pitch=state.display10?1920u:128u,height=state.display10?1088u:(format==3?128u:64u),tile=state.display10?10u:14u;const size_t bytes=size_t(pitch)*height*texel_bytes;std::fill_n(guest,bytes,static_cast<unsigned char>(0x25));
  CtrGraphicsColorRequirementV1 shape{1,sizeof(shape),0,pitch,height,1,1,tile,format,format==4?7u:0u,reinterpret_cast<uint64_t>(guest),bytes};uint64_t color=0;
  int status=ctr_video_color_attachment_create_v1(api->context,&shape,0,&color);if(status)return status;
  struct Cleanup{void* session;uint64_t color;~Cleanup(){ctr_video_color_attachment_release_v1(session,color);}}cleanup{api->context,color};
  std::vector<unsigned char> pixels(bytes,0x6b);
  if(format==4)for(size_t k=0;k<bytes/4;++k){const uint32_t bits=0x3f000000u|static_cast<uint32_t>(k&0x7fffff);std::memcpy(pixels.data()+k*4,&bits,4);}
  status=ctr_video_color_attachment_upload_linear_v1(api->context,color,pixels.data(),bytes,uint64_t(pitch)*texel_bytes);if(status)return status;
  ComputeGuestWriteback writer;writer.address=shape.guest_address;writer.initial.assign(guest,guest+bytes);ColorGuestDownload download{api->context,color,shape};
  status=writer.commit(make_owned_guest_copy_api(),ColorGuestDownload::download,&download);if(status)return status;
  if(!writer.completed || writer.poisoned)return 112;
  if(format==4){
   status=initialize_float_color(api->context,color,shape,guest,bytes);if(status)return status;
   std::vector<unsigned char> initialized(bytes);status=ctr_video_color_attachment_readback_linear_v1(api->context,color,initialized.data(),bytes,128*4);if(status || initialized!=pixels)return status?status:112;
   auto invalid=shape;invalid.number_format=0;if(initialize_float_color(api->context,color,invalid,guest,bytes)!=88)return 112;
  }
  CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=p.pitch=pitch;p.height=height;p.depth=p.layers=p.levels=p.samples=1;p.data_format=format;p.number_format=format==4?7u:0u;p.tile_mode=tile;p.guest_address=shape.guest_address;
  uint64_t sampled=0;status=format!=10?ctr_video_storage_image_create_v1(api->context,&p,&sampled):ctr_video_sampled_image_create_v1(api->context,&p,&sampled);if(status)return status;
  struct SampleCleanup{void* session;uint64_t token;~SampleCleanup(){ctr_video_sampled_image_release_v1(session,token);}}sample_cleanup{api->context,sampled};
  CtrGraphicsImageExtentV1 extent{1,sizeof(extent),pitch,height,shape.guest_address,bytes};status=ctr_video_sampled_image_upload_tiled_v1(api->context,sampled,guest,bytes,&extent);if(status)return status;
  std::vector<unsigned char> returned(bytes);status=ctr_video_sampled_image_readback_linear_v1(api->context,sampled,returned.data(),bytes,uint64_t(pitch)*texel_bytes);if(status)return status;if(returned!=pixels)return 112;
  }state.observed=true;return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};const int status=ctr_video_run_window_session_v1(&options,body,&state);if(status || !state.observed)return status?status:112;
 std::cout<<"{\"scope\":\"color_guest_writeback_actual_GPU_control\",\"display10\":"<<(display10?"true":"false")<<",\"passed\":true,\"RGBA8_guest_bytes\":"<<(display10?8355840:32768)<<",\"RG8_guest_bytes\":"<<(display10?0:32768)<<",\"R32float_guest_bytes\":"<<(include_float?32768:0)<<",\"actual_color_transfer_retiled_guest_reupload_compared\":true,\"authored_GPU_upload_source\":true,\"runtime_bound\":false,\"R3\":false}\n";return 0;
}
}
