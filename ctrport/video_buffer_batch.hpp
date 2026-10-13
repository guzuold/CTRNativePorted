// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include "video_session_owner.hpp"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
namespace ctrport {
// Synchronous selected-session ownership only. This scope must run wholly on
// the video owner. Close only after all consumers (and any future fences) finish.
// No GPU completion or coherence is manufactured by retaining a token.
class VideoBufferBatch {
 void* context_;
 std::vector<uint64_t> tokens_;
 bool closed_=false;
public:
 explicit VideoBufferBatch(void* context):context_(context){tokens_.reserve(256);}
 VideoBufferBatch(const VideoBufferBatch&)=delete;
 VideoBufferBatch& operator=(const VideoBufferBatch&)=delete;
 int create(const void* source,uint64_t bytes,uint32_t usage,uint64_t* output){
  if(closed_ || !output)return 60;
  if(tokens_.size()==256)return 88;
  uint64_t token=0;const int status=ctr_video_buffer_create_v1(context_,source,bytes,usage,&token);
  if(status)return status;
  if(!token)return 112;
  tokens_.push_back(token);*output=token;return 0;
 }
 size_t size()const{return tokens_.size();}
 int close(unsigned* released=nullptr)noexcept{
  if(closed_)return 0;
  int failure=0;unsigned count=0;
  for(auto it=tokens_.rbegin();it!=tokens_.rend();++it){
   const int status=ctr_video_buffer_release_v1(context_,*it);
   if(!status)++count;else if(!failure)failure=status;
  }
  closed_=true;if(released)*released=count;return failure;
 }
 ~VideoBufferBatch(){const int status=close();if(status){std::fprintf(stderr,"ERROR selected-session buffer batch cleanup status=%d\n",status);std::fflush(stderr);}}
};
// Authored ownership failure cases, not game rendering or a graphics stub.
inline int video_buffer_batch_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;
 auto session=runtime_video_session;
 const int result=session->invoke([](const CtrVideoSessionApiV1* api){
  const uint32_t source[4]={7,19,23,29};uint32_t returned[4]={};
  VideoBufferBatch batch(api->context);uint64_t first=0,second=0;
  if(batch.create(source,sizeof(source),1,&first) || batch.create(source,sizeof(source),2,&second) || batch.size()!=2)return 2;
  uint64_t unchanged=123;
  if(batch.create(source,sizeof(source),99,&unchanged)!=88 || unchanged!=123 || batch.size()!=2)return 3;
  if(ctr_video_buffer_copy_initial_v1(api->context,first,returned,sizeof(returned)) || std::memcmp(source,returned,sizeof(source)))return 4;
  unsigned released=0;if(batch.close(&released) || released!=2 || batch.close())return 5;
  if(ctr_video_buffer_copy_initial_v1(api->context,second,returned,sizeof(returned))!=62)return 6;
  if(batch.create(source,sizeof(source),1,&unchanged)!=60 || unchanged!=123)return 7;
  uint64_t unwound=0;
  try{VideoBufferBatch interrupted(api->context);if(interrupted.create(source,sizeof(source),1,&unwound))return 8;throw std::runtime_error("authored interruption");}
  catch(const std::runtime_error&){}
  if(!unwound || ctr_video_buffer_copy_initial_v1(api->context,unwound,returned,sizeof(returned))!=62)return 9;
  return 0;
 });
 session.reset();if(runtime_video_close(1))return 10;
 if(!result)std::cout<<"{\"scope\":\"video_buffer_batch_control\",\"simultaneously_live\":2,\"failure_preserves_live_buffers\":true,\"exception_cleanup\":true,\"synthetic\":true,\"GPU_execution\":false}\n";
 return result;
}
}
