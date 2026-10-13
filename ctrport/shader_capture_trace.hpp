// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/shader_inputs.h"
#include "video/graphics_companion.h"
#include <vector>
#include <cstring>
#include <iostream>
namespace ctrport {
// Diagnostic only: observe the same owner-established captures used by the
// compiler. No new guest reads, pointer guessing or change to token ownership.
struct ShaderCaptureTrace {
 struct Record {uint64_t address=0,size=0,token=0;int capture_status=88,release_status=88;std::vector<unsigned char> bytes;};
 CtrShaderInputsV1 original{};std::vector<Record> records;uint64_t retained=0;int trace_status=0;
 explicit ShaderCaptureTrace(const CtrShaderInputsV1& input):original(input){}
 static int capture(void* raw,uint64_t address,uint64_t bytes,CtrShaderSnapshot* output){
  auto& self=*static_cast<ShaderCaptureTrace*>(raw);
  const int status=self.original.capture(self.original.context,address,bytes,output);
  try{
   if(self.records.size()>=128){self.trace_status=88;return status;}
   Record record;record.address=address;record.size=bytes;record.capture_status=status;
   if(!status){record.token=output->snapshot_token;
    if(!output->bytes || output->guest_address!=address || output->size!=bytes || !record.token)self.trace_status=112;
    else if(bytes<=131072 && bytes<=1024*1024-self.retained){
     record.bytes.resize(static_cast<size_t>(bytes));std::memcpy(record.bytes.data(),output->bytes,record.bytes.size());self.retained+=bytes;
    }else self.trace_status=88;
   }
   self.records.push_back(std::move(record));
  }catch(...){self.trace_status=112;}
  return status;
 }
 static int release(void* raw,const CtrShaderSnapshot* snapshot){
  auto& self=*static_cast<ShaderCaptureTrace*>(raw);
  const int status=self.original.release(self.original.context,snapshot);
  if(!snapshot)return status;
  for(auto& record:self.records)if(record.token==snapshot->snapshot_token && record.address==snapshot->guest_address && record.size==snapshot->size)record.release_status=status;
  return status;
 }
 CtrShaderInputsV1 api(){return {1,sizeof(CtrShaderInputsV1),this,capture,release};}
};
// Copies synchronous diagnostic metadata; no callback exception crosses C ABI.
struct ShaderProvenanceTrace {
 decltype(&ctr_graphics_provenance_span_v1) span=nullptr;
 std::vector<CtrShaderProvenanceV1> records,selected_span;int status=0,span_status=88;
 static void observe(void* raw,const CtrShaderProvenanceV1* record)noexcept{
  if(!raw)return;auto& trace=*static_cast<ShaderProvenanceTrace*>(raw);
  if(trace.status)return;
  if(!record){trace.status=60;return;}
  if(trace.records.size()>=4096){trace.status=88;return;}
  try{trace.records.push_back(*record);
   if(trace.span && record->flat_index==112){CtrShaderProvenanceV1 values[8]{};trace.span_status=trace.span(record->flat_index,8,values,8);if(!trace.span_status)trace.selected_span.assign(values,values+8);}
  }catch(...){trace.status=112;}
 }
};
inline bool contiguous_image_source_span(const std::vector<CtrShaderProvenanceV1>& span,uint32_t first){
 if(span.size()!=8 || span[0].source_kind!=1 || span[0].root_sgpr>=16 || span[0].source_dword>UINT32_MAX-7 || first>UINT32_MAX-7)return false;
 for(uint32_t i=0;i<8;++i)if(span[i].flat_index!=first+i || span[i].source_kind!=1 || span[i].root_sgpr!=span[0].root_sgpr || span[i].source_dword!=span[0].source_dword+i)return false;
 return true;
}
inline int shader_provenance_trace_control(){
 ShaderProvenanceTrace trace;CtrShaderProvenanceV1 record{24,112,17,10,0,1};
 ShaderProvenanceTrace::observe(&trace,&record);record.pc=99;
 if(trace.status || trace.records.size()!=1 || trace.records[0].pc!=24 || trace.records[0].flat_index!=112 || trace.records[0].root_sgpr!=10)return 1;
 ShaderProvenanceTrace::observe(&trace,nullptr);ShaderProvenanceTrace::observe(&trace,&record);
 if(trace.status!=60 || trace.records.size()!=1)return 2;
 ShaderProvenanceTrace bounded;bounded.records.resize(4096);ShaderProvenanceTrace::observe(&bounded,&record);
 if(bounded.status!=88 || bounded.records.size()!=4096)return 3;
 std::cout<<"{\"scope\":\"shader_provenance_trace_control\",\"passed\":true,\"callback_copy_immutable\":true,\"bounded_and_sticky_errors\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
inline int shader_capture_trace_control(){
 struct Owner {unsigned char bytes[32]{};unsigned captures=0,releases=0;bool fail=false;} owner;
 for(unsigned i=0;i<32;++i)owner.bytes[i]=static_cast<unsigned char>(i+1);
 CtrShaderInputsV1 input{1,sizeof(CtrShaderInputsV1),&owner,
  +[](void* raw,uint64_t address,uint64_t bytes,CtrShaderSnapshot* output)->int{auto& o=*static_cast<Owner*>(raw);++o.captures;if(o.fail)return 62;if(address!=4096 || bytes!=32)return 60;*output={o.bytes,32,4096,7};return 0;},
  +[](void* raw,const CtrShaderSnapshot* snapshot)->int{auto& o=*static_cast<Owner*>(raw);++o.releases;return snapshot->snapshot_token==7?0:62;}};
 ShaderCaptureTrace trace(input);auto api=trace.api();CtrShaderSnapshot snapshot{};
 if(api.capture(api.context,4096,32,&snapshot) || snapshot.bytes!=owner.bytes || trace.records.size()!=1 || trace.retained!=32)return 1;
 owner.bytes[0]=99;if(trace.records[0].bytes[0]!=1 || api.release(api.context,&snapshot) || trace.records[0].release_status || owner.captures!=1 || owner.releases!=1)return 2;
 owner.fail=true;const auto saved=snapshot;if(api.capture(api.context,4096,32,&snapshot)!=62 || std::memcmp(&snapshot,&saved,sizeof(snapshot)) || trace.records.back().capture_status!=62)return 3;
 std::cout<<"{\"scope\":\"shader_capture_trace_control\",\"passed\":true,\"same_owner_tokens\":true,\"immutable_copy\":true,\"failed_capture_preserved\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
