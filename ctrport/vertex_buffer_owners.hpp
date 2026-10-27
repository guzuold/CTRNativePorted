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

}
