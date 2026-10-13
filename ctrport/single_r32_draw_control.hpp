// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "color_guest_writeback.hpp"
#include "published_color_generations.hpp"
#include <fstream>
namespace ctrport {
inline std::vector<uint32_t> r32_control_shader(const char* path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return {};auto n=f.tellg();if(n<=0 || n>65536 || n%4)return {};std::vector<uint32_t> w(static_cast<size_t>(n)/4);f.seekg(0);f.read(reinterpret_cast<char*>(w.data()),n);if(!f)return {};return w;}
inline int single_r32_draw_control(bool retained=false,bool single_depth=false,bool inactive_ms2=false){
 struct State{bool observed=false,retained=false,single_depth=false,inactive_ms2=false;std::vector<uint32_t> vs,ps;};State state;state.retained=retained;state.single_depth=single_depth;state.inactive_ms2=inactive_ms2;state.vs=r32_control_shader("build/CTRPort-infra/controls/single_r32.vert.spv");state.ps=r32_control_shader("build/CTRPort-infra/controls/single_r32.frag.spv");if(state.vs.empty() || state.ps.empty())return 62;
 auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
 if(!api || !api->context)return 60;auto& s=*static_cast<State*>(raw);GuestStack stack(65536,4096);auto* guest=static_cast<unsigned char*>(stack.bottom());std::fill_n(guest,32768,static_cast<unsigned char>(0));
 CtrGraphicsColorRequirementV1 shape{1,sizeof(shape),0,128,64,1,1,14,(s.inactive_ms2 || (s.retained && !s.single_depth))?10u:4u,(s.inactive_ms2 || (s.retained && !s.single_depth))?0u:7u,reinterpret_cast<uint64_t>(guest),32768};uint64_t color=0,layout=0,pipeline=0,depth=0;
 struct Cleanup{void* context;uint64_t &color,&layout,&pipeline,&depth;~Cleanup(){if(pipeline)ctr_video_graphics_pipeline_release_v1(context,pipeline);if(layout)ctr_video_graphics_layout_release_v1(context,layout);if(depth)ctr_video_depth_attachment_release_v1(context,depth);if(color)ctr_video_color_attachment_release_v1(context,color);}}cleanup{api->context,color,layout,pipeline,depth};
 int status=ctr_video_color_attachment_create_v1(api->context,&shape,0,&color);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}
 status=ctr_video_color_attachment_upload_linear_v1(api->context,color,guest,32768,512);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}
 CtrGraphicsBindingContractV1 ps{},vs{};ps.version=vs.version=1;ps.size=sizeof(ps);vs.size=sizeof(vs);status=ctr_video_graphics_layout_create_v1(api->context,&ps,&vs,&layout);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}
 CtrGraphicsKeyV1 key{};key.version=1;key.size=sizeof(key);key.mrt_mask=key.num_color_attachments=key.cb_shader_mask=key.num_samples=key.depth_samples=1;key.logic_op=0xcc;key.primitive_type=4;key.polygon_mode=2;key.colors[0].data_format=shape.data_format;key.colors[0].number_format=shape.number_format;key.colors[0].write_mask=key.colors[0].samples=1;
 if(s.retained){CtrDepthAttachmentParametersV1 parameters{1,sizeof(parameters),128,64,s.single_depth?1u:2u,3,1};status=ctr_video_depth_attachment_create_v1(api->context,&parameters,&depth);if(status)return status;if(!s.single_depth){status=ctr_video_color_backing_copy_v1(api->context,color,0);if(status)return status;}key.num_samples=key.depth_samples=key.colors[0].samples=s.single_depth?1u:2u;key.z_format=3;key.stencil_format=1;}
 if(s.inactive_ms2){key.num_samples=key.depth_samples=key.colors[0].samples=2;key.z_format=3;key.stencil_format=1;status=ctr_video_color_backing_copy_v1(api->context,color,0);if(status)return status;}
 CtrGraphicsVertexInputsV1 vertices{};vertices.version=1;vertices.size=sizeof(vertices);
 status=s.inactive_ms2?ctr_video_graphics_mrt_pipeline_create_v1(api->context,&key,&vertices,nullptr,0,layout,&color,1,s.vs.data(),static_cast<uint32_t>(s.vs.size()),s.ps.data(),static_cast<uint32_t>(s.ps.size()),0,&pipeline):s.retained?ctr_video_graphics_mrt_depth_pipeline_create_v1(api->context,&key,&vertices,nullptr,0,layout,&color,1,depth,s.vs.data(),static_cast<uint32_t>(s.vs.size()),s.ps.data(),static_cast<uint32_t>(s.ps.size()),0,&pipeline):ctr_video_graphics_pipeline_create_v1(api->context,&key,&vertices,nullptr,0,layout,color,s.vs.data(),static_cast<uint32_t>(s.vs.size()),s.ps.data(),static_cast<uint32_t>(s.ps.size()),0,&pipeline);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}
 CtrGraphicsViewportStateV1 viewport{};viewport.version=1;viewport.size=sizeof(viewport);viewport.count=1;viewport.viewports[0]={0,0,128,64,0,1};viewport.scissors[0].extent={128,64};
 CtrGraphicsDynamicStateV1 dynamic{};dynamic.version=1;dynamic.size=sizeof(dynamic);dynamic.line_width=1;dynamic.depth_max=1;
 CtrGraphicsPushDataV1 push{};push.version=1;push.size=sizeof(push);CtrGraphicsDrawV1 draw{};draw.version=1;draw.size=sizeof(draw);draw.vertex_count=3;draw.instance_count=1;
 if(s.retained){dynamic.stencil_test=1;dynamic.front.compare_op=dynamic.back.compare_op=2;dynamic.front.reference=dynamic.back.reference=16;dynamic.front.compare_mask=dynamic.back.compare_mask=16;
 status=ctr_video_depth_attachment_clear_v1(api->context,depth,0x3f800000,0);if(status)return status;
 status=ctr_video_graphics_mrt_depth_draw_v1(api->context,pipeline,&viewport,&dynamic,&push,&draw);if(status)return status;
 if(!s.single_depth){status=ctr_video_color_backing_copy_v1(api->context,color,1);if(status)return status;}std::vector<uint32_t> rejected(8192);status=ctr_video_color_attachment_readback_linear_v1(api->context,color,rejected.data(),32768,512);if(status)return status;for(auto value:rejected)if(value)return 112;
 status=ctr_video_depth_attachment_clear_v1(api->context,depth,0x3f800000,16);if(status)return status;}
 status=s.inactive_ms2?ctr_video_graphics_mrt_draw_v1(api->context,pipeline,&viewport,&dynamic,&push,&draw):s.retained?ctr_video_graphics_mrt_depth_draw_v1(api->context,pipeline,&viewport,&dynamic,&push,&draw):ctr_video_graphics_draw_v1(api->context,pipeline,color,&viewport,&dynamic,&push,&draw);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}
 if(s.inactive_ms2 || (s.retained && !s.single_depth)){status=ctr_video_color_backing_copy_v1(api->context,color,1);if(status)return status;}
 std::vector<uint32_t> actual(8192);status=ctr_video_color_attachment_readback_linear_v1(api->context,color,actual.data(),32768,512);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}for(auto value:actual)if(value!=((s.inactive_ms2 || (s.retained && !s.single_depth))?0x40u:0x3e800000u))return 112;
 ComputeGuestWriteback writer;writer.address=shape.guest_address;writer.initial.assign(guest,guest+32768);ColorGuestDownload download{api->context,color,shape};status=writer.commit(make_owned_guest_copy_api(),ColorGuestDownload::download,&download);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}if(!writer.completed || writer.poisoned)return 112;
 // Concrete repeated-render-target case: the first publication supersedes
 // the initial all-zero snapshot. Reject that stale baseline atomically, then
 // publish the next real GPU draw using the verified prior generation.
 if(s.single_depth){
  PublishedColorGenerations generations;status=generations.publish(writer,1);if(status)return status;
  std::vector<std::pair<uint64_t,uint64_t>> ranges{{shape.guest_address,shape.bytes}};
  status=ctr_video_graphics_mrt_depth_draw_v1(api->context,pipeline,&viewport,&dynamic,&push,&draw);if(status)return status;
  ComputeGuestWriteback stale;stale.address=shape.guest_address;stale.initial.assign(32768,0);
  if(stale.commit(make_owned_guest_copy_api(),ColorGuestDownload::download,&download)!=88 || stale.completed || stale.poisoned)return 112;
  ComputeGuestWriteback next;next.address=shape.guest_address;
  status=generations.baseline(shape.guest_address,shape.bytes,ranges,ranges.size(),next.initial);if(status)return status;
  status=next.commit(make_owned_guest_copy_api(),ColorGuestDownload::download,&download);if(status || !next.completed || next.poisoned)return status?status:112;
  if(next.initial!=writer.initial)return 112;
 }
 CtrGraphicsImageExtentV1 tiled{1,sizeof(tiled),128,64,shape.guest_address,32768};status=(s.inactive_ms2 || (s.retained && !s.single_depth))?ctr_video_color_attachment_upload_tiled_v1(api->context,color,guest,32768,&tiled):initialize_float_color(api->context,color,shape,guest,32768);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}std::fill(actual.begin(),actual.end(),0);status=ctr_video_color_attachment_readback_linear_v1(api->context,color,actual.data(),32768,512);if(status){std::cerr<<"singleR32 failure line="<<__LINE__<<" status="<<status<<"\n";return status;}for(auto value:actual)if(value!=((s.inactive_ms2 || (s.retained && !s.single_depth))?0x40u:0x3e800000u))return 112;s.observed=true;return 0;
 };
 CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};int status=ctr_video_run_window_session_v1(&options,body,&state);if(status || !state.observed)return status?status:112;
 std::cout<<"{\"scope\":\""<<(inactive_ms2?"single_MS2_inactive_depth_authored_GPU_draw":single_depth?"single_R32_sample1_retained_stencil_authored_GPU_draw":retained?"single_MS2_retained_stencil_authored_GPU_draw":"single_R32_authored_actual_GPU_draw")<<"\",\"passed\":true,\"pixels\":8192,\"guest_retiled_bytes\":32768,\"differences\":0,\"repeated_writer_stale_rejected\":"<<(single_depth?"true":"false")<<",\"runtime_bound\":false,\"R3\":false}\n";return 0;
}
}

