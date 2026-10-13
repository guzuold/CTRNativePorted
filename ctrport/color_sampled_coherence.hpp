// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <vector>
#include "video_color_batch.hpp"
#include "video_image_batch.hpp"
namespace ctrport {
// Per-session completed-writer generations. Allocation and descriptor writes
// are not writers. Call begin before recording a writer, complete only after
// its real fence; a failed writer stays pending and cannot expose stale pixels.
class ColorSampledCoherence {
 struct Color {uint64_t token,generation=0,word=0;bool pending=false;};
 struct View {uint64_t color,image,generation;};
 std::vector<Color> colors_;std::vector<View> views_;bool running_=false;
 Color* find(uint64_t token){for(auto& c:colors_)if(c.token==token)return &c;return nullptr;}
public:
 int add(uint64_t token){if(!token)return 60;if(find(token))return 0;colors_.push_back({token});return 0;}
 int begin(uint64_t token,uint64_t word){auto* c=find(token);if(!c)return 62;if(c->pending)return 91;if(c->generation==UINT64_MAX)return 88;c->pending=true;c->word=word;return 0;}
 int complete(uint64_t token,uint64_t word){auto* c=find(token);if(!c)return 62;if(!c->pending || c->word!=word)return 91;c->pending=false;++c->generation;return 0;}
 // Validate the complete MRT writer set before mutating any generation.
 // A recording/submit/fence failure deliberately leaves every writer pending.
 int begin_many(const uint64_t* tokens,size_t count,uint64_t word){
  if(!tokens || !count || count>8)return 60;
  for(size_t i=0;i<count;++i){for(size_t j=0;j<i;++j)if(tokens[i]==tokens[j])return 60;
   const auto* c=find(tokens[i]);if(!c)return 62;if(c->pending)return 91;if(c->generation==UINT64_MAX)return 88;}
  for(size_t i=0;i<count;++i){auto* c=find(tokens[i]);c->pending=true;c->word=word;}return 0;
 }
 int complete_many(const uint64_t* tokens,size_t count,uint64_t word){
  if(!tokens || !count || count>8)return 60;
  for(size_t i=0;i<count;++i){for(size_t j=0;j<i;++j)if(tokens[i]==tokens[j])return 60;
   const auto* c=find(tokens[i]);if(!c)return 62;if(!c->pending || c->word!=word)return 91;}
  for(size_t i=0;i<count;++i){auto* c=find(tokens[i]);c->pending=false;++c->generation;}return 0;
 }
 template<class Transfer> int synchronize(uint64_t color,uint64_t image,Transfer&& transfer){
  if(running_)return 91;
  auto* c=find(color);if(!c)return 62;if(!image)return 60;if(c->pending || !c->generation)return 91;
  for(const auto& v:views_)if(v.image==image && v.color!=color)return 88;
  for(const auto& v:views_)if(v.image==image && v.generation==c->generation)return 0;
  running_=true;struct Reset{bool& value;~Reset(){value=false;}}reset{running_};
  const auto generation=c->generation;int status=112;try{status=transfer(color,image);}catch(...){return 112;}
  if(status)return status;
  // A callback cannot publish a generation which changed during transfer.
  c=find(color);if(!c || c->pending || c->generation!=generation)return 91;
  for(auto& v:views_)if(v.image==image){v.generation=generation;return 0;}
  views_.push_back({color,image,generation});return 0;
 }
};
// Transfer the genuine R32 attachment generation to its independently swizzled
// sampled view. Both operations fence; CPU bytes come only from GPU readback.
// Logical view rows exclude padded attachment rows. No tiled-address formula.
inline int transfer_color_sampled_view(void* session,uint64_t color,uint64_t image,const CtrGraphicsColorRequirementV1& c,const CtrGraphicsImageParametersV1& p){
 if(c.data_format!=4)return ctr_video_color_to_sampled_v1(session,color,image);
 if(c.version!=1 || c.size!=sizeof(c) || p.version!=1 || p.size!=sizeof(p) || c.bytes>32*1024*1024 || c.number_format!=7 || p.data_format!=4 || p.number_format!=7 || c.samples!=1 || p.samples!=1 || c.layers!=1 || p.layers!=1 || p.levels!=1 || c.tile_mode!=14 || p.tile_mode!=14 || !p.width || !p.height || p.width>c.pitch || p.height>c.height || p.pitch!=c.pitch || c.pitch>4096 || c.height>4096 || c.guest_address!=p.guest_address || c.bytes!=uint64_t(c.pitch)*c.height*4)return 88;
 const uint64_t pitch=uint64_t(c.pitch)*4;
 std::vector<unsigned char> linear(static_cast<size_t>(c.bytes));
 const int status=ctr_video_color_attachment_readback_linear_v1(session,color,linear.data(),linear.size(),pitch);if(status)return status;
 return ctr_video_sampled_image_upload_linear_v1(session,image,linear.data(),pitch*p.height,pitch);
}
// Compare only the explicit bounded linear view after real conversion fences.
// This verifies transport, not the correctness of the upstream guest detiler.
inline int compare_color_sampled_view(void* session,uint64_t color,uint64_t image,const CtrGraphicsColorRequirementV1& c,const CtrGraphicsImageParametersV1& p,uint64_t& compared,uint64_t& differences,uint64_t& padding){
 if(!p.width || !p.height || p.width>c.pitch || p.height>c.height || p.pitch!=c.pitch || c.pitch>4096 || c.height>4096)return 60;
 if(c.data_format!=p.data_format || c.number_format!=p.number_format || c.samples!=1 || p.samples!=1)return 88;
 const uint32_t texel=c.data_format==3?2u:(c.data_format==10 || (c.data_format==4 && c.number_format==7)?4u:0u);if(!texel)return 88;
 const uint64_t pitch=uint64_t(c.pitch)*texel;
 std::vector<unsigned char> backing(static_cast<size_t>(pitch*c.height)),view(static_cast<size_t>(pitch*p.height+16),0xa5);
 int status=ctr_video_color_attachment_readback_linear_v1(session,color,backing.data(),backing.size(),pitch);if(status)return status;
 status=ctr_video_sampled_image_readback_linear_v1(session,image,view.data(),view.size(),pitch);if(status)return status;
 for(size_t at=0;at<view.size();++at){const bool pixel=at/pitch<p.height && at%pitch<uint64_t(p.width)*texel;
  if(pixel){++compared;differences+=view[at]!=backing[at];}else padding+=view[at]!=0xa5;
 }
 return differences || padding?112:0;
}
inline int color_sampled_coherence_control(){
 ColorSampledCoherence state;unsigned calls=0;auto transfer=[&](uint64_t c,uint64_t i){++calls;return c==1 && i==2?0:112;};
 if(state.add(1) || state.synchronize(1,2,transfer)!=91 || calls)return 1;
 if(state.begin(1,10) || state.synchronize(1,2,transfer)!=91 || state.complete(1,11)!=91 || calls)return 2;
 if(state.complete(1,10) || state.synchronize(1,2,transfer) || calls!=1 || state.synchronize(1,2,transfer) || calls!=1)return 3;
 if(state.begin(1,20) || state.synchronize(1,2,transfer)!=91 || state.complete(1,20))return 4;
 auto failure=[&](uint64_t,uint64_t){++calls;return 88;};
 if(state.synchronize(1,2,failure)!=88 || state.synchronize(1,2,transfer) || calls!=3)return 5;
 if(state.add(3) || state.begin(3,1) || state.complete(3,1) || state.synchronize(3,2,transfer)!=88)return 6;
 auto throwing=[](uint64_t,uint64_t)->int{throw 1;};
 if(state.begin(1,30) || state.complete(1,30) || state.synchronize(1,2,throwing)!=112 || state.synchronize(1,2,transfer) || calls!=4)return 7;
 if(state.begin(1,40) || state.complete(1,40))return 8;
 auto reentrant=[&](uint64_t,uint64_t){return state.synchronize(1,2,transfer);};
 if(state.synchronize(1,2,reentrant)!=91 || state.synchronize(1,2,transfer) || calls!=5)return 9;
 ColorSampledCoherence mrt;if(mrt.add(11) || mrt.add(12))return 10;
 uint64_t targets[]={11,99};if(mrt.begin_many(targets,2,1377)!=62 || mrt.begin(11,1) || mrt.complete(11,1))return 11;
 targets[1]=11;if(mrt.begin_many(targets,2,1377)!=60 || mrt.begin(11,2) || mrt.complete(11,2))return 12;
 targets[1]=12;if(mrt.begin_many(targets,2,1377) || mrt.begin(11,3)!=91 || mrt.begin(12,3)!=91)return 13;
 if(mrt.complete_many(targets,2,1378)!=91 || mrt.begin(11,3)!=91 || mrt.begin(12,3)!=91)return 14;
 if(mrt.complete_many(targets,2,1377) || mrt.begin_many(targets,2,1500))return 15;
 auto no_transfer=[](uint64_t,uint64_t){return 0;};
 if(mrt.synchronize(11,21,no_transfer)!=91 || mrt.synchronize(12,22,no_transfer)!=91)return 16;
 if(mrt.complete_many(targets,2,1500) || mrt.synchronize(11,21,no_transfer) || mrt.synchronize(12,22,no_transfer))return 17;
 std::cout<<"{\"scope\":\"authored_color_sampled_coherence\",\"passed\":true,\"pending_writer_blocks_stale_read\":true,\"failed_transfer_retry\":true,\"generation_deduplication\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int color_sampled_gpu_control(bool r32=false,bool half=false){
 if(runtime_video_open(255,0,0,0)!=1)return 1;auto session=runtime_video_session;
 const int result=session->invoke([r32,half](const CtrVideoSessionApiV1* api){
  CtrGraphicsColorRequirementV1 c{1,sizeof(c),0,128,64,1,1,14,10,0,0x100000,128*64*4};
  CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=128;p.height=64;p.depth=1;p.pitch=128;p.layers=1;p.levels=1;p.samples=1;p.data_format=10;p.tile_mode=14;p.guest_address=c.guest_address;
  if(r32){c.pitch=1920;c.height=1088;c.data_format=4;c.number_format=7;c.bytes=uint64_t(c.pitch)*c.height*4;
   p.width=p.pitch=1920;p.height=1080;p.data_format=4;p.number_format=7;p.swizzle[0]=4;p.swizzle[1]=0;p.swizzle[2]=0;p.swizzle[3]=1;
   if(half){c.pitch=1024;c.height=576;c.bytes=uint64_t(c.pitch)*c.height*4;p.width=960;p.pitch=1024;p.height=540;}}
  VideoImageBatch images(api->context);VideoColorBatch colors(api->context);uint64_t color=0,image=0;
  if(colors.create(c,&color) || images.create(p,&image))return 2;
  ColorSampledCoherence state;if(state.add(color))return 3;unsigned transfers=0;
  auto transfer=[&](uint64_t a,uint64_t b){++transfers;return transfer_color_sampled_view(api->context,a,b,c,p);};
  std::vector<unsigned char> input(c.bytes),output(uint64_t(p.pitch)*p.height*4+16,0xa5);
  for(unsigned pass=0;pass<3;++pass){
   for(size_t i=0;i<input.size();++i)input[i]=static_cast<unsigned char>((i*17+pass*73)^(i>>9));
   if(state.begin(color,pass) || state.synchronize(color,image,transfer)!=91)return 4;
   if(ctr_video_color_attachment_upload_linear_v1(api->context,color,input.data(),input.size(),uint64_t(c.pitch)*4) || state.complete(color,pass))return 5;
   if(state.synchronize(color,image,transfer) || state.synchronize(color,image,transfer) || transfers!=pass+1)return 6;
   if(ctr_video_sampled_image_readback_linear_v1(api->context,image,output.data(),output.size(),uint64_t(p.pitch)*4))return 7;
   const uint64_t pitch=uint64_t(p.pitch)*4;
   for(size_t at=0;at<output.size();++at){const bool pixel=at/pitch<p.height && at%pitch<uint64_t(p.width)*4;
    if(output[at]!=(pixel?input[at]:0xa5))return 8;}

  }
  uint64_t compared=0,differences=0,padding=0;if(compare_color_sampled_view(api->context,color,image,c,p,compared,differences,padding) || compared!=uint64_t(p.width)*p.height*4)return 10;
  if(r32){auto mismatch=p;++mismatch.guest_address;if(transfer_color_sampled_view(api->context,color,image,c,mismatch)!=88)return 11;}
  return 0;
 });session.reset();if(runtime_video_close(1))return 9;
 if(!result)std::cout<<"{\"scope\":\"authored_color_sampled_gpu_coherence\",\"R32_FLOAT\":"<<(r32?"true":"false")<<",\"pitched_half_view\":"<<(half?"true":"false")<<",\"completed_writers\":3,\"transfers\":3,\"pixel_differences\":0,\"guard_differences\":0,\"synthetic\":true,\"game_draw\":false}\n";
 return result;
}

}
