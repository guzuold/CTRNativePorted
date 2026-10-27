// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include "video_session_owner.hpp"
#include <vector>
#include <stdexcept>
namespace ctrport {
// Undefined-content actual attachment owners. No LOAD or draw admission.
class VideoDepthBatch {
 void* session_;std::vector<uint64_t> tokens_;bool closed_=false;
public:
 explicit VideoDepthBatch(void* session):session_(session){tokens_.reserve(128);}
 VideoDepthBatch(const VideoDepthBatch&)=delete;VideoDepthBatch& operator=(const VideoDepthBatch&)=delete;
 ~VideoDepthBatch(){const int status=close();if(status)std::cerr<<"depth batch cleanup status="<<status<<std::endl;}
 int create(const CtrDepthAttachmentParametersV1& demand,uint64_t* output){
  if(closed_ || !session_ || !output)return 60;if(tokens_.size()>=128)return 88;
  uint64_t token=0;
  const int status=ctr_video_depth_attachment_create_v1(session_,&demand,&token);
  if(status)return status;if(!token)return 112;tokens_.push_back(token);*output=token;return 0;
 }
 int retain(uint64_t source,uint64_t* output){
  if(closed_ || !session_ || !source || !output)return 60;if(tokens_.size()>=128)return 88;
  uint64_t token=0;const int status=ctr_video_depth_attachment_retain_v1(session_,source,&token);
  if(status)return status;if(!token)return 112;tokens_.push_back(token);*output=token;return 0;
 }
 size_t size()const{return tokens_.size();}
 int close(){if(closed_)return 0;closed_=true;int first=0;
  for(auto i=tokens_.rbegin();i!=tokens_.rend();++i){const int s=ctr_video_depth_attachment_release_v1(session_,*i);if(s && !first)first=s;}
  tokens_.clear();return first;
 }
};
}
