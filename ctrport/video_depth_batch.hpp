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
inline int video_depth_batch_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;auto session=runtime_video_session;
 const int result=session->invoke([](const CtrVideoSessionApiV1* api){
  CtrDepthAttachmentParametersV1 p{1,sizeof(p),16,16,1,3,1};
  VideoDepthBatch batch(api->context);uint64_t first=0,second=0;
  if(batch.create(p,&first) || batch.create(p,&second) || batch.size()!=2)return 2;
  auto bad=p;bad.version=0;uint64_t unchanged=123;
  if(batch.create(bad,&unchanged)!=60 || unchanged!=123 || batch.size()!=2)return 3;
  uint64_t retained=0,unchanged_lease=321;
  if(batch.retain(0,&unchanged_lease)!=60 || unchanged_lease!=321 || batch.size()!=2)return 10;
  if(batch.retain(first,&retained) || !retained || retained==first || batch.size()!=3)return 11;
  if(batch.retain(UINT64_MAX,&unchanged_lease)!=62 || unchanged_lease!=321 || batch.size()!=3)return 12;
  if(batch.close() || batch.close() || batch.size())return 4;
  if(ctr_video_depth_attachment_release_v1(api->context,retained)!=62)return 13;
  if(ctr_video_depth_attachment_release_v1(api->context,second)!=62)return 5;
  if(batch.create(p,&unchanged)!=60 || unchanged!=123)return 6;
  uint64_t unwound=0;
  try{VideoDepthBatch interrupted(api->context);if(interrupted.create(p,&unwound))return 7;throw std::runtime_error("authored interruption");}catch(const std::runtime_error&){}
  if(!unwound || ctr_video_depth_attachment_release_v1(api->context,unwound)!=62)return 8;
  return 0;
 });session.reset();if(runtime_video_close(1))return 9;
 if(!result)std::cout<<"{\"scope\":\"video_depth_batch_control\",\"simultaneously_live\":2,\"failure_preserves_live_depths\":true,\"exception_cleanup\":true,\"synthetic\":true,\"GPU_execution\":false}\n";
 return result;
}
}
