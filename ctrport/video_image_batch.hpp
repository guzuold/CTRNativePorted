// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include <vector>
#include <cstdio>
#include <stdexcept>
#include "video_session_owner.hpp"
namespace ctrport {
// Owner-thread synchronous retention; destroy descriptor layout owners first.
class VideoImageBatch {
 void* context_;std::vector<uint64_t> tokens_;bool closed_=false;
public:
 explicit VideoImageBatch(void* context):context_(context){tokens_.reserve(256);}
 VideoImageBatch(const VideoImageBatch&)=delete;
 VideoImageBatch& operator=(const VideoImageBatch&)=delete;
 int create(const CtrGraphicsImageParametersV1& parameters,uint64_t* output){
  if(closed_ || !output)return 60;if(tokens_.size()==256)return 88;
  uint64_t token=0;const int status=ctr_video_sampled_image_create_v1(context_,&parameters,&token);
  if(status)return status;if(!token)return 112;
  tokens_.push_back(token);*output=token;return 0;
 }
 int adopt(uint64_t token){
  if(closed_ || !token)return 60;if(tokens_.size()==256)return 88;
  for(auto owned:tokens_)if(owned==token)return 60;
  tokens_.push_back(token);return 0;
 }
 size_t size()const{return tokens_.size();}
 int close(unsigned* released=nullptr)noexcept{
  if(closed_)return 0;int failure=0;unsigned count=0;
  for(auto it=tokens_.rbegin();it!=tokens_.rend();++it){const int status=ctr_video_sampled_image_release_v1(context_,*it);if(!status)++count;else if(!failure)failure=status;}
  closed_=true;if(released)*released=count;return failure;
 }
 ~VideoImageBatch(){const int status=close();if(status){std::fprintf(stderr,"ERROR selected-session image batch cleanup status=%d\n",status);std::fflush(stderr);}}
};inline int video_image_batch_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;
 auto session=runtime_video_session;
 const int result=session->invoke([](const CtrVideoSessionApiV1* api){
  CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=4;p.height=4;p.depth=1;p.layers=1;p.levels=1;p.samples=1;p.data_format=10;
  VideoImageBatch batch(api->context);uint64_t first=0,second=0;
  if(batch.create(p,&first) || batch.create(p,&second) || batch.size()!=2)return 2;
  auto bad=p;bad.version=0;uint64_t unchanged=123;
  if(batch.create(bad,&unchanged)!=60 || unchanged!=123 || batch.size()!=2)return 3;
  if(batch.adopt(first)!=60 || batch.adopt(0)!=60 || batch.size()!=2)return 10;
 unsigned released=0;if(batch.close(&released) || released!=2 || batch.close())return 4;
  if(ctr_video_sampled_image_release_v1(api->context,second)!=62)return 5;
  if(batch.create(p,&unchanged)!=60 || unchanged!=123)return 6;
  uint64_t unwound=0;
  try{VideoImageBatch interrupted(api->context);if(interrupted.create(p,&unwound))return 7;throw std::runtime_error("authored interruption");}
  catch(const std::runtime_error&){}
  if(!unwound || ctr_video_sampled_image_release_v1(api->context,unwound)!=62)return 8;
  return 0;
 });
 session.reset();if(runtime_video_close(1))return 9;
 if(!result)std::cout<<"{\"scope\":\"video_image_batch_control\",\"simultaneously_live\":2,\"failure_preserves_live_images\":true,\"exception_cleanup\":true,\"synthetic\":true,\"descriptor_writes\":false,\"GPU_execution\":false}\n";
 return result;
}

}
