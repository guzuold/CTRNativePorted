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
}
