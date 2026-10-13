// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_key_client.hpp"
#include "video_buffer_batch.hpp"
namespace ctrport {
struct LiveVertexBuffer {uint32_t word,binding,location,stride,input_rate,divisor,format;uint64_t token,bytes,guest_address=0;};
inline int retain_vertex_buffers(void* context,VideoBufferBatch& batch,uint32_t word,const GraphicsVertexFetchOwner& fetch,
 std::vector<LiveVertexBuffer>& live,std::ostream& log,uint64_t& compared){
 if(fetch.status || fetch.format_status || fetch.physical_status)return 88;
 if(fetch.value.count!=fetch.buffers.size() || fetch.value.count>32)return 60;
 for(uint32_t i=0;i<fetch.value.count;++i){const auto& input=fetch.value.inputs[i];const auto& source=fetch.buffers[i];
  if(source.address!=input.guest_address || source.bytes!=input.buffer_size || source.snapshot.size()!=source.bytes || source.capture_status)return 112;
  uint64_t token=0;int status=batch.create(source.snapshot.data(),source.bytes,4,&token);uint64_t differences=0;
  if(!status){std::vector<unsigned char> returned(source.snapshot.size(),0xa5);status=ctr_video_buffer_copy_initial_v1(context,token,returned.data(),returned.size());
   if(!status)for(size_t n=0;n<returned.size();++n)if(returned[n]!=source.snapshot[n])++differences;
   if(!status && !differences){compared+=source.bytes;live.push_back({word,input.binding,input.location,input.stride,input.input_rate,input.divisor,fetch.formats[i],token,source.bytes,source.address});}
  }
  log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_buffer\",\"word\":"<<word<<",\"binding\":"<<input.binding<<",\"location\":"<<input.location<<",\"usage\":4,\"bytes\":"<<source.bytes<<",\"status\":"<<status<<",\"differences\":"<<differences<<",\"retained\":"<<(!status&&!differences?"true":"false")<<",\"host_initialization_only\":true,\"GPU_execution\":false}\n";
  if(status || differences)return status?status:112;
 }
 return 0;
}
inline int vertex_buffer_owners_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;auto session=runtime_video_session;
 const int status=session->invoke([](const CtrVideoSessionApiV1* api){
  GraphicsVertexFetchOwner fetch;fetch.status=fetch.format_status=fetch.physical_status=0;fetch.value.count=1;
  fetch.value.inputs[0].guest_address=0x1000;fetch.value.inputs[0].buffer_size=32;fetch.formats[0]=37;
  GraphicsBufferCapture source;source.address=0x1000;source.bytes=32;source.snapshot.assign(32,0x27);source.capture_status=0;fetch.buffers.push_back(source);
  std::ostringstream log;std::vector<LiveVertexBuffer> live;uint64_t compared=0;VideoBufferBatch batch(api->context);
  if(retain_vertex_buffers(api->context,batch,255,fetch,live,log,compared) || live.size()!=1 || compared!=32 || batch.size()!=1)return 2;
  const auto token=live[0].token;fetch.buffers[0].snapshot[0]=99;unsigned char value[32]{};
  if(ctr_video_buffer_copy_initial_v1(api->context,token,value,32) || value[0]!=0x27)return 3;
  fetch.physical_status=88;if(retain_vertex_buffers(api->context,batch,255,fetch,live,log,compared)!=88 || batch.size()!=1)return 4;
  fetch.physical_status=0;fetch.buffers[0].bytes=31;if(retain_vertex_buffers(api->context,batch,255,fetch,live,log,compared)!=112 || batch.size()!=1)return 5;
  unsigned released=0;if(batch.close(&released) || released!=1 || ctr_video_buffer_copy_initial_v1(api->context,token,value,32)!=62)return 6;
  return 0;
 });session.reset();if(runtime_video_close(1))return 7;
 if(!status)std::cout<<"{\"scope\":\"vertex_buffer_owners_control\",\"passed\":true,\"source_mutation_isolated\":true,\"failed_preflight_preserves_batch\":true,\"identity_mismatch_rejected\":true,\"stale_after_close\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return status;
}

}
