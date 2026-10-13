// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include "video_session_owner.hpp"
#include <vector>
#include <cstdio>
#include <stdexcept>
namespace ctrport {
// Synchronous owner-thread scope: declare after buffer owners so layouts and
// their descriptor sets are destroyed first. Retention alone is not a fence.
class VideoLayoutBatch {
 void* context_;std::vector<uint64_t> tokens_;bool closed_=false;
public:
 explicit VideoLayoutBatch(void* context):context_(context){tokens_.reserve(128);}
 VideoLayoutBatch(const VideoLayoutBatch&)=delete;
 VideoLayoutBatch& operator=(const VideoLayoutBatch&)=delete;
 int create(const CtrGraphicsBindingContractV1& ps,const CtrGraphicsBindingContractV1& vs,uint64_t* output){
  if(closed_ || !output)return 60;
  if(tokens_.size()==128)return 88;
  uint64_t token=0;const int status=ctr_video_graphics_layout_create_v1(context_,&ps,&vs,&token);
  if(status)return status;
  if(!token)return 112;
  tokens_.push_back(token);*output=token;return 0;
 }
 int create_vertex(const CtrGraphicsBindingContractV1& vs,uint64_t* output){
  if(closed_ || !output)return 60;if(tokens_.size()==128)return 88;
  uint64_t token=0;const int status=ctr_video_graphics_vertex_layout_create_v1(context_,&vs,&token);
  if(status)return status;if(!token)return 112;tokens_.push_back(token);*output=token;return 0;
 }
 size_t size()const{return tokens_.size();}
 int close(unsigned* released=nullptr)noexcept{
  if(closed_)return 0;
  int failure=0;unsigned count=0;
  for(auto it=tokens_.rbegin();it!=tokens_.rend();++it){const int status=ctr_video_graphics_layout_release_v1(context_,*it);if(!status)++count;else if(!failure)failure=status;}
  closed_=true;if(released)*released=count;return failure;
 }
 ~VideoLayoutBatch(){const int status=close();if(status){std::fprintf(stderr,"ERROR selected-session layout batch cleanup status=%d\n",status);std::fflush(stderr);}}
};
inline int video_layout_batch_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;
 auto session=runtime_video_session;
 const int result=session->invoke([](const CtrVideoSessionApiV1* api){
  CtrGraphicsBindingContractV1 ps{},vs{};ps.version=vs.version=1;ps.size=vs.size=sizeof(ps);
  ps.descriptor_count=1;ps.unified_after=1;ps.buffer_after=1;ps.descriptors[0]={0,0,0,1,0,1};
  vs.unified_before=vs.unified_after=1;vs.buffer_before=vs.buffer_after=1;
  VideoLayoutBatch batch(api->context);uint64_t first=0,second=0;
  if(batch.create(ps,vs,&first) || batch.create(ps,vs,&second) || batch.size()!=2)return 2;
  auto bad=ps;bad.version=0;uint64_t unchanged=123;
  if(batch.create(bad,vs,&unchanged)!=60 || unchanged!=123 || batch.size()!=2)return 3;
  unsigned released=0;if(batch.close(&released) || released!=2 || batch.close())return 4;
  if(ctr_video_graphics_layout_release_v1(api->context,second)!=62)return 5;
  if(batch.create(ps,vs,&unchanged)!=60 || unchanged!=123)return 6;
  uint64_t unwound=0;
  try{VideoLayoutBatch interrupted(api->context);if(interrupted.create(ps,vs,&unwound))return 7;throw std::runtime_error("authored interruption");}
  catch(const std::runtime_error&){}
  if(!unwound || ctr_video_graphics_layout_release_v1(api->context,unwound)!=62)return 8;
  return 0;
 });
 session.reset();if(runtime_video_close(1))return 9;
 if(!result)std::cout<<"{\"scope\":\"video_layout_batch_control\",\"simultaneously_live\":2,\"failure_preserves_live_layouts\":true,\"exception_cleanup\":true,\"synthetic\":true,\"descriptor_writes\":false,\"GPU_execution\":false}\n";
 return result;
}
inline int video_vertex_layout_batch_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;auto session=runtime_video_session;
 const int result=session->invoke([](const CtrVideoSessionApiV1* api){
  CtrGraphicsBindingContractV1 vs{};vs.version=1;vs.size=sizeof(vs);
  VideoLayoutBatch batch(api->context);uint64_t first=0,second=0;
  if(batch.create_vertex(vs,&first) || batch.create_vertex(vs,&second) || batch.size()!=2)return 2;
  auto bad=vs;bad.unified_before=1;uint64_t unchanged=123;
  if(batch.create_vertex(bad,&unchanged)!=60 || unchanged!=123 || batch.size()!=2)return 3;
  unsigned released=0;if(batch.close(&released) || released!=2 || batch.close())return 4;
  if(ctr_video_graphics_layout_release_v1(api->context,second)!=62 || batch.create_vertex(vs,&unchanged)!=60 || unchanged!=123)return 5;
  uint64_t unwound=0;
  try{VideoLayoutBatch interrupted(api->context);if(interrupted.create_vertex(vs,&unwound))return 6;throw std::runtime_error("authored vertex unwind");}
  catch(const std::runtime_error&){}
  if(!unwound || ctr_video_graphics_layout_release_v1(api->context,unwound)!=62)return 7;
  return 0;
 });
 session.reset();if(runtime_video_close(1))return 8;
 if(!result)std::cout<<"{\"scope\":\"video_vertex_layout_batch_control\",\"simultaneously_live\":2,\"failure_preserves_live_layouts\":true,\"exception_cleanup\":true,\"synthetic\":true,\"GPU_execution\":false}\n";
 return result;
}

}
