// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gpu_resident_memory.hpp"
#include "gnm_eop_completion.hpp"
#include "gnm_completion_batch.hpp"
#include "depth_publication_owners.hpp"
#include "gnm_command_cursor.hpp"
#include "video/session_api.h"
#include "video/graphics_companion.h"
namespace ctrport {
// Whole supported WriteData memory-copy branch from Liverpool::ProcessGraphics,
// engine/shadPS4/src/video_core/amdgpu/liverpool.cpp:764-778 and PM4CmdWriteData,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Sequential address writes only; wr_one_addr and non-memory destinations fail.
// A genuine preceding-work completion provider is mandatory, including for
// label writes. This transport helper never produces a GPU fence or flip IRQ.
inline int complete_write_data(const uint32_t* packet,size_t words,const CtrGuestCopyApi& memory,
 GnmCompletePriorGpuWork complete,void* context){
 if(!packet || words<5 || words>16385)return 60;
 if((packet[0]&0xc000ff00u)!=0xc0003700u || size_t((packet[0]>>16)&0x3fffu)+2!=words)return 60;
 if(packet[0]&255u)return 88;
 const auto control=packet[1],destination=(control>>8)&15u;
 if((destination!=2 && destination!=5) || (control&0x10000u))return 88;
 // Only public memory-copy controls have a handler here; reserved fields fail.
 if(control&~0x40100f00u)return 88;
 const uint64_t address=uint64_t(packet[2])|(uint64_t(packet[3])<<32);
 const uint64_t bytes=uint64_t(words-4)*4;
 if(!address || (address&3u) || address>UINT64_MAX-bytes)return 60;
 if(memory.size!=sizeof(memory) || memory.version!=1 || !memory.write)return 60;
 if(!complete)return 88;
 try{const int status=complete(context);if(status)return status;
  return memory.write(memory.context,address,packet+4,bytes);
 }catch(...){return 112;}
}
// Both label transports require the same authoritative image generations.
// Residency satisfies completion without retiring GPU dirtiness or exposing
// stale CPU bytes. Color publication remains mandatory for current linear paths.
struct GnmImageCompletion {
 size_t colors;DepthPublicationOwners* depths;CtrGuestCopyApi memory;
 DepthPublicationOwners::Resident resident;void* resident_context;
 GnmCompletePriorGpuWork visibility;void* visibility_context;
 static int finish(void* raw){
  if(!raw)return 60;auto& work=*static_cast<GnmImageCompletion*>(raw);
  if(!work.depths || !work.visibility)return 60;if(work.colors)return 88;
  const int status=work.depths->finish(work.memory,work.resident,work.resident_context);
  return status?status:work.visibility(work.visibility_context);
 }
};
inline int gnm_write_data_control(){
 GuestStack storage(32768,4096);const auto address=reinterpret_cast<uint64_t>(storage.bottom());
 auto* values=reinterpret_cast<uint32_t*>(address);values[0]=17;values[1]=19;values[2]=23;
 uint32_t packet[]={0xc0043700,0x500,uint32_t(address),uint32_t(address>>32),123,456};
 struct State{unsigned calls=0;int result=88;};State state;
 const auto completion=+[](void* raw)->int{auto& s=*static_cast<State*>(raw);++s.calls;return s.result;};
 const auto memory=make_owned_guest_copy_api();
 if(complete_write_data(packet,6,memory,nullptr,nullptr)!=88 || values[0]!=17)return 1;
 if(complete_write_data(packet,6,memory,completion,&state)!=88 || state.calls!=1 || values[0]!=17 || values[1]!=19)return 2;
 state.result=0;if(complete_write_data(packet,6,memory,completion,&state) || state.calls!=2 || values[0]!=123 || values[1]!=456 || values[2]!=23)return 3;
 const auto calls=state.calls;packet[1]|=0x10000;
 if(complete_write_data(packet,6,memory,completion,&state)!=88 || state.calls!=calls)return 4;
 packet[1]=0x100;if(complete_write_data(packet,6,memory,completion,&state)!=88 || state.calls!=calls)return 5;
 packet[1]=0x500;packet[2]|=1;if(complete_write_data(packet,6,memory,completion,&state)!=60 || state.calls!=calls)return 6;
 packet[2]&=~3u;if(complete_write_data(packet,5,memory,completion,&state)!=60 || state.calls!=calls)return 7;
 packet[1]=0x200;packet[4]=789;packet[5]=987;
 if(complete_write_data(packet,6,memory,completion,&state) || values[0]!=789 || values[1]!=987 || values[2]!=23)return 8;
 std::cout<<"{\"scope\":\"public_write_data_completion\",\"passed\":true,\"actual_owned_guest_writes\":true,\"both_memory_destinations\":true,\"pending_preserves_memory\":true,\"synthetic_completion_provider\":true,\"runtime_bound\":false,\"GPU_execution\":false}\n";return 0;
}
inline int gnm_write_data_gpu_control(){
 struct State{int status=88;uint32_t writes=0;};State state;
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  auto& result=*static_cast<State*>(raw);if(!api || !api->context)return 60;
  GuestStack stack(32768,4096);const auto address=reinterpret_cast<uint64_t>(stack.bottom());auto* value=reinterpret_cast<uint32_t*>(address);*value=17;
  uint32_t packet[]={0xc0033700,0x500,uint32_t(address),uint32_t(address>>32),123};
  const auto memory=make_owned_guest_copy_api();
  const auto complete=+[](void* context)->int{return ctr_video_queue_memory_visibility_v1(context);};
  result.status=complete_write_data(packet,5,memory,complete,api->context);
  if(result.status || *value!=123)return result.status?result.status:112;++result.writes;
  packet[1]=0x200;packet[4]=456;
  result.status=complete_write_data(packet,5,memory,complete,api->context);
  if(result.status || *value!=456)return result.status?result.status:112;++result.writes;
  packet[1]=0x10500;packet[4]=789;
  if(complete_write_data(packet,5,memory,complete,api->context)!=88 || *value!=456)return 112;
  return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};
 const int status=ctr_video_run_window_session_v1(&options,body,&state);
 if(status || state.status || state.writes!=2)return status?status:112;
 std::cout<<"{\"scope\":\"gnm_WriteData_actual_queue_fence_control\",\"passed\":true,\"authored_packets\":true,\"real_Vulkan_queue_barriers_and_fences\":2,\"actual_owned_guest_writes\":2,\"unsupported_no_write\":true,\"game_packet_executed\":false,\"frame_or_flip\":false}\n";return 0;
}
inline int gnm_eop_gpu_control(){
 struct State{unsigned writes=0,fences=0;};State state;
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  if(!api || !api->context)return 60;auto& state=*static_cast<State*>(raw);
  GuestStack stack(32768,4096);const auto address=reinterpret_cast<uint64_t>(stack.bottom());
  auto* value=reinterpret_cast<uint64_t*>(address);*value=0xa5a5a5a5a5a5a5a5ull;
  uint32_t stream[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(1u<<29),0x12345678,0x87654321,
   0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(2u<<29),0x12345678,0x87654321};
  const auto memory=make_owned_guest_copy_api();GnmCommandCursor cursor;
  if(cursor.capture(1,0,0,stream,12))return 112;
  // A genuinely missing completion provider keeps the first packet pending;
  // retry consumes the immutable captured packet exactly once after real fences.
  auto unavailable=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* packet){return complete_eop_writeback(packet,p.words,memory,nullptr,nullptr);};
  if(cursor.resume(unavailable)!=88 || cursor.completed() || *value!=0xa5a5a5a5a5a5a5a5ull)return 112;
  struct Completion{void* session;State* state;} completion{api->context,&state};
  const auto complete=+[](void* raw)->int{auto& c=*static_cast<Completion*>(raw);const int status=ctr_video_queue_memory_visibility_v1(c.session);if(!status)++c.state->fences;return status;};
  auto execute=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t* packet){
   const int status=complete_eop_writeback(packet,p.words,memory,complete,&completion);if(status)return status;
   const uint64_t expected=p.word==0?0xa5a5a5a512345678ull:0x8765432112345678ull;
   if(*value!=expected)return 112;++state.writes;return 0;
  };
  // Mutating the caller's packet after capture must not change actual execution.
  stream[4]=0;stream[10]=0;
  if(cursor.resume(execute) || cursor.completed()!=2 || state.writes!=2 || state.fences!=2)return 112;
  if(cursor.resume(execute) || state.writes!=2 || state.fences!=2)return 112;
  const auto before=*value;uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(2u<<29)|(2u<<24),0,0};
  if(complete_eop_writeback(packet,6,memory,complete,&completion)!=88 || *value!=before || state.fences!=2)return 112;
  packet[3]=uint32_t(address>>32)|(3u<<29);
  if(complete_eop_writeback(packet,6,memory,complete,&completion)!=88 || *value!=before || state.fences!=2)return 112;
  packet[3]=uint32_t(address>>32)|(2u<<29);packet[1]|=1u<<16;
  if(complete_eop_writeback(packet,6,memory,complete,&completion)!=88 || *value!=before || state.fences!=2)return 112;
  packet[1]=0x504;packet[3]|=1u<<16;
  if(complete_eop_writeback(packet,6,memory,complete,&completion)!=88 || *value!=before || state.fences!=2)return 112;
  return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};
 const int status=ctr_video_run_window_session_v1(&options,body,&state);
 if(status || state.writes!=2 || state.fences!=2)return status?status:112;
 std::cout<<"{\"scope\":\"gnm_EOP_actual_queue_control\",\"passed\":true,\"authored_packets\":true,\"real_queue_visibility_fences\":2,\"guest_writes\":2,\"missing_provider_keeps_cursor\":true,\"owned_packets_mutation_isolated\":true,\"resume_without_duplicate_signal\":true,\"interrupt_and_clock_rejected\":true,\"unhandled_control_bits_rejected_before_fence\":true,\"prior_game_images_drained\":false,\"runtime_bound\":false,\"frame_or_flip\":false}\n";return 0;
}

// Authored completion integration: actual owner-thread queue visibility/fence,
// owned EOP write, then real kernel waiter delivery. No prior game-image claim.
inline int gnm_graphics_equeue_gpu_control(){
 uint64_t queue=0;if(kernel_equeue_create(&queue,"graphics-gpu"))return 1;
 if(kernel_gnm_add_eq_event(queue,64,0x4455))return 2;
 struct State{uint64_t queue;bool observed=false;};State state{queue};
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  auto& state=*static_cast<State*>(raw);if(!api || !api->context)return 60;
  GuestStack stack(32768,4096);auto* output=reinterpret_cast<unsigned char*>(stack.bottom());
  auto* label=reinterpret_cast<uint64_t*>(output);*label=0;
  auto* event=reinterpret_cast<DisplayKernelEvent*>(output+64);auto* count=reinterpret_cast<int*>(output+128);*count=0;
  auto waiting=std::async(std::launch::async,[&](){return kernel_equeue_wait(state.queue,event,1,count,nullptr);});
  const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(!kernel_display_events.queues.waiting_count(state.queue) && std::chrono::steady_clock::now()<limit)std::this_thread::yield();
  if(!kernel_display_events.queues.waiting_count(state.queue)){kernel_equeue_delete(state.queue);waiting.wait();return 112;}
  const auto address=reinterpret_cast<uint64_t>(label);uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(2u<<29)|(2u<<24),0x12345678,0x87654321};
  const auto completion=+[](void* context)->int{return ctr_video_queue_memory_visibility_v1(context);};
  const auto signal=+[](void*)->int{kernel_display_events.queues.graphics_signal(64);return 0;};
  GnmEopTransaction transaction;
  GnmEopImageGate gate{0,2,completion,api->context};
  const int blocked=transaction.commit(packet,6,make_owned_guest_copy_api(),GnmEopImageGate::complete,&gate,signal,nullptr);
  if(blocked!=88 || *label || transaction.completed || transaction.poisoned || waiting.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout){kernel_equeue_delete(state.queue);waiting.wait();return 112;}
  gate.depth_dirty=0;gate.color_dirty=1;
  if(transaction.commit(packet,6,make_owned_guest_copy_api(),GnmEopImageGate::complete,&gate,signal,nullptr)!=88 || *label){kernel_equeue_delete(state.queue);waiting.wait();return 112;}
  gate.color_dirty=0;
  const int status=transaction.commit(packet,6,make_owned_guest_copy_api(),GnmEopImageGate::complete,&gate,signal,nullptr);
  if(status || *label!=0x8765432112345678ull){kernel_equeue_delete(state.queue);waiting.wait();return status?status:112;}
  if(!transaction.completed || transaction.poisoned)return 112;
  if(waiting.get() || *count!=1 || event->filter!=-14 || event->ident!=64 || event->data!=64 || event->udata!=0x4455)return 112;
  state.observed=true;return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};
 const int status=ctr_video_run_window_session_v1(&options,body,&state);kernel_equeue_delete(queue);
 if(status || !state.observed)return status?status:112;
 std::cout<<"{\"scope\":\"gnm_graphics_equeue_actual_queue_control\",\"passed\":true,\"real_queue_fence\":true,\"owned_label_write64\":true,\"real_kernel_waiter\":true,\"dirty_color_and_depth_block_label_and_IRQ\":true,\"graphics_filter\":-14,\"IRQ_id\":64,\"authored_completion_source\":true,\"prior_game_images_drained\":false,\"runtime_bound\":false,\"R3\":false}\n";return 0;
}

inline int gnm_image_completion_gpu_control(){
 uint64_t queue=0;if(kernel_equeue_create(&queue,"image-completion"))return 1;struct QueueCleanup{uint64_t queue;~QueueCleanup(){kernel_equeue_delete(queue);}}queue_cleanup{queue};if(kernel_gnm_add_eq_event(queue,64,0x6677))return 2;
 struct State{uint64_t queue;bool observed=false;};State state{queue};
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  if(!api || !api->context)return 60;auto& state=*static_cast<State*>(raw);
  GuestStack stack(131072,4096);auto* bytes=static_cast<unsigned char*>(stack.bottom());
  const auto memory=make_owned_guest_copy_api();constexpr size_t imageBytes=128*64*4;
  std::fill_n(bytes,imageBytes*2,static_cast<unsigned char>(0x25));auto* label=reinterpret_cast<uint64_t*>(bytes+imageBytes*2);*label=0;
  struct Download {void* session;uint64_t token;CtrGraphicsImageExtentV1 extent;bool pending=false;unsigned calls=0;};
  std::array<Download,2> downloads{};
  struct Cleanup{void* session;std::array<Download,2>& values;~Cleanup(){for(const auto& d:values)if(d.token)ctr_video_sampled_image_release_v1(session,d.token);}}cleanup{api->context,downloads};
  GnmCompletionBatch batch;
  const auto download=+[](void* context,const unsigned char* prior,uint64_t length,unsigned char* output,uint64_t capacity)->int{
   auto& d=*static_cast<Download*>(context);++d.calls;if(d.pending)return 88;if(length!=capacity)return 60;
   return ctr_video_image_readback_tiled_v1(d.session,d.token,prior,length,&d.extent,output);
  };
  for(size_t i=0;i<downloads.size();++i){auto& d=downloads[i];d.session=api->context;
   const uint64_t address=reinterpret_cast<uint64_t>(bytes+i*imageBytes);d.extent={1,sizeof(d.extent),128,64,address,imageBytes};
   CtrGraphicsImageParametersV1 p{};p.version=1;p.size=sizeof(p);p.type=9;p.width=16;p.height=1;p.pitch=128;p.tile_mode=14;p.guest_address=address;p.depth=p.layers=p.levels=p.samples=1;p.data_format=4;p.number_format=7;
   int status=ctr_video_storage_image_create_v1(api->context,&p,&d.token);if(status)return status;
   std::array<float,16> pixels{};pixels.fill(i?17.0f:9.0f);
   status=ctr_video_sampled_image_upload_linear_v1(api->context,d.token,pixels.data(),sizeof(pixels),sizeof(pixels));if(status)return status;
   std::vector<unsigned char> prior(bytes+i*imageBytes,bytes+(i+1)*imageBytes);
   status=batch.add(address,std::move(prior),download,&d);if(status)return status;
  }
  batch.seal();downloads[1].pending=true;
  struct Completion{void* session;GnmCompletionBatch* batch;CtrGuestCopyApi memory;unsigned fences=0;} completion{api->context,&batch,memory};
  const auto finish=+[](void* context)->int{auto& c=*static_cast<Completion*>(context);
   const auto fence=+[](void* raw)->int{auto& completion=*static_cast<Completion*>(raw);++completion.fences;return ctr_video_queue_memory_visibility_v1(completion.session);};
   return c.batch->finish(c.memory,fence,&c);};
  auto* event=reinterpret_cast<DisplayKernelEvent*>(bytes+imageBytes*2+64);auto* event_count=reinterpret_cast<int*>(bytes+imageBytes*2+128);*event_count=0;
  auto waiting=std::async(std::launch::async,[&](){return kernel_equeue_wait(state.queue,event,1,event_count,nullptr);});
  struct WaitCleanup{uint64_t queue;std::future<int>& waiting;~WaitCleanup(){if(waiting.valid()){kernel_equeue_delete(queue);waiting.wait();}}}wait_cleanup{state.queue,waiting};
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!kernel_display_events.queues.waiting_count(state.queue) && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();if(!kernel_display_events.queues.waiting_count(state.queue))return 112;
  struct Irq{uint64_t* label;GnmCompletionBatch* batch;unsigned signals=0;} irq{label,&batch};
  const auto signal=+[](void* context)->int{auto& s=*static_cast<Irq*>(context);if(!s.batch->completed() || *s.label!=0x8765432112345678ull)return 112;kernel_display_events.queues.graphics_signal(64);++s.signals;return 0;};
  const auto address=reinterpret_cast<uint64_t>(label);uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(2u<<29)|(2u<<24),0x12345678,0x87654321};GnmEopTransaction eop;
  const int paused=eop.commit(packet,6,memory,finish,&completion,signal,&irq);
  std::cout<<"{\"scope\":\"gnm_image_completion_pause\",\"status\":"<<paused<<",\"published\":"<<batch.published()<<",\"first_calls\":"<<downloads[0].calls<<",\"second_calls\":"<<downloads[1].calls<<"}\n";
  if(paused!=88 || batch.published()!=1 || *label || irq.signals || completion.fences || downloads[0].calls!=1 || downloads[1].calls!=1)return 112;
  if(waiting.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout || *event_count)return 112;
  const std::vector<unsigned char> first(bytes,bytes+imageBytes);
  if(std::all_of(first.begin(),first.end(),[](unsigned char v){return v==0x25;}) || !std::all_of(bytes+imageBytes,bytes+2*imageBytes,[](unsigned char v){return v==0x25;}))return 112;
  downloads[1].pending=false;
  bytes[0]^=1;
  if(eop.commit(packet,6,memory,finish,&completion,signal,&irq)!=88 || batch.published()!=2 || batch.completed() || *label || irq.signals || completion.fences || downloads[0].calls!=1 || downloads[1].calls!=2)return 112;
  if(waiting.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout || *event_count)return 112;
  bytes[0]^=1;
  const int resumed=eop.commit(packet,6,memory,finish,&completion,signal,&irq);
  std::cout<<"{\"scope\":\"gnm_image_completion_resume\",\"status\":"<<resumed<<",\"published\":"<<batch.published()<<",\"fences\":"<<completion.fences<<",\"signals\":"<<irq.signals<<"}\n";
  if(resumed || !batch.completed() || batch.published()!=2 || downloads[0].calls!=1 || downloads[1].calls!=2 || completion.fences!=1 || irq.signals!=1 || !std::equal(first.begin(),first.end(),bytes))return 112;
  if(waiting.wait_for(std::chrono::seconds(2))!=std::future_status::ready || waiting.get() || *event_count!=1 || event->ident!=64 || event->filter!=-14 || event->data!=64 || event->udata!=0x6677)return 112;
  for(auto& d:downloads){std::array<float,16> actual{};int status=ctr_video_sampled_image_upload_tiled_v1(api->context,d.token,reinterpret_cast<void*>(d.extent.guest_address),imageBytes,&d.extent);if(status)return status;
   status=ctr_video_sampled_image_readback_linear_v1(api->context,d.token,actual.data(),sizeof(actual),sizeof(actual));if(status)return status;
   const float expected=&d==&downloads[0]?9.0f:17.0f;for(float value:actual)if(value!=expected)return 112;
  }
  if(eop.commit(packet,6,memory,finish,&completion,signal,&irq)!=91 || irq.signals!=1 || downloads[0].calls!=1 || downloads[1].calls!=2)return 112;
  state.observed=true;return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};const int status=ctr_video_run_window_session_v1(&options,body,&state);
 if(status || !state.observed)return status?status:112;
 std::cout<<"{\"scope\":\"gnm_image_completion_actual_GPU_control\",\"passed\":true,\"real_images\":2,\"tiled_guest_bytes\":65536,\"pending_second_preserves_label_IRQ\":true,\"resume_skips_published_first\":true,\"stale_prior_guest_generation_rejected\":true,\"guest_reupload_linear_compare\":true,\"final_real_queue_fence\":true,\"real_kernel_waiter\":true,\"graphics_IRQ_id\":64,\"synthetic_pending_only\":true,\"authored_upload_source\":true,\"runtime_bound\":false,\"R3\":false}\n";return 0;
}

inline int gnm_depth_publication_eop_gpu_control(){
 uint64_t queue=0;if(kernel_equeue_create(&queue,"depth-EOP"))return 1;
 struct QueueCleanup{uint64_t queue;~QueueCleanup(){kernel_equeue_delete(queue);}}queue_cleanup{queue};if(kernel_gnm_add_eq_event(queue,64,0x7788))return 2;
 struct State{uint64_t queue;bool passed=false;}state{queue};
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  if(!api || !api->context)return 60;auto& state=*static_cast<State*>(raw);
  GuestStack allocation(32768,4096);auto* bytes=static_cast<unsigned char*>(allocation.bottom());std::fill_n(bytes,512,static_cast<unsigned char>(0x25));
  auto* label=reinterpret_cast<uint64_t*>(bytes);*label=0xa5a5a5a500000000ull;
  auto* event=reinterpret_cast<DisplayKernelEvent*>(bytes+64);auto* count=reinterpret_cast<int*>(bytes+128);*count=0;
  auto waiting=std::async(std::launch::async,[&](){return kernel_equeue_wait(state.queue,event,1,count,nullptr);});
  struct WaitCleanup{uint64_t queue;std::future<int>& waiter;~WaitCleanup(){if(waiter.valid()){kernel_equeue_delete(queue);waiter.wait();}}}wait_cleanup{state.queue,waiting};
  const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!kernel_display_events.queues.waiting_count(state.queue) && std::chrono::steady_clock::now()<until)std::this_thread::yield();
  if(!kernel_display_events.queues.waiting_count(state.queue))return 112;
  const auto memory=make_owned_guest_copy_api();DepthPublicationOwners owners;
  struct Provider{unsigned calls=0;bool pending=false;}providers[2]{{0,false},{0,true}};
  const auto download=+[](void* raw,uint32_t plane,const unsigned char* prior,uint64_t length,unsigned char* output,uint64_t capacity)->int{auto& provider=*static_cast<Provider*>(raw);++provider.calls;if(provider.pending)return 88;if(length!=64 || capacity!=64)return 60;std::copy_n(prior,64,output);std::fill_n(output,48,static_cast<unsigned char>(plane?0x72:0x61));return 0;};
  const uint64_t lengths[]={64,64};
  for(unsigned i=0;i<2;++i){uint64_t addresses[]={reinterpret_cast<uint64_t>(bytes+256+128*i),reinterpret_cast<uint64_t>(bytes+320+128*i)};const int enrolled=owners.enroll(memory,7+i,addresses,lengths,2,download,&providers[i]);if(enrolled)return enrolled;if(owners.mark(7+i,1377+i))return 112;}
  struct Completion{DepthPublicationOwners* owners;CtrGuestCopyApi memory;void* session;unsigned fences=0;}completion{&owners,memory,api->context};
  const auto finish=+[](void* raw)->int{auto& work=*static_cast<Completion*>(raw);const int status=work.owners->finish(work.memory);if(status)return status;const int fenced=ctr_video_queue_memory_visibility_v1(work.session);if(!fenced)++work.fences;return fenced;};
  const auto signal=+[](void*)->int{kernel_display_events.queues.graphics_signal(64);return 0;};
  const uint64_t address=reinterpret_cast<uint64_t>(label);uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(1u<<29),0x12345678,0};GnmEopTransaction original_shape;
  if(original_shape.commit(packet,6,memory,finish,&completion,nullptr,nullptr)!=88 || *label!=0xa5a5a5a500000000ull || completion.fences || owners.size()!=1 || providers[0].calls!=2 || providers[1].calls!=1 || original_shape.poisoned)return 112;
  bytes[256]=0x99;providers[1].pending=false;
  if(original_shape.commit(packet,6,memory,finish,&completion,nullptr,nullptr)!=88 || providers[0].calls!=2 || providers[1].calls!=1 || completion.fences || *label!=0xa5a5a5a500000000ull)return 112;
  bytes[256]=0x61;
  if(original_shape.commit(packet,6,memory,finish,&completion,nullptr,nullptr) || *label!=0xa5a5a5a512345678ull || !owners.empty() || completion.fences!=1 || providers[0].calls!=2 || providers[1].calls!=3 || waiting.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout)return 112;
  packet[3]=uint32_t(address>>32)|(2u<<29)|(2u<<24);packet[5]=0x87654321;GnmEopTransaction irq;
  if(irq.commit(packet,6,memory,finish,&completion,signal,nullptr) || *label!=0x8765432112345678ull || completion.fences!=2 || waiting.get() || *count!=1 || event->ident!=64 || event->filter!=-14 || event->udata!=0x7788)return 112;
  for(unsigned i=0;i<256;++i)if(bytes[256+i]!=(i%64<48?(i%128<64?0x61:0x72):0x25))return 112;
  state.passed=true;return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};const int status=ctr_video_run_window_session_v1(&options,body,&state);if(status || !state.passed)return status?status:112;
 std::cout<<"{\"scope\":\"gnm_depth_publication_EOP_GPU_control\",\"passed\":true,\"real_queue_fences\":2,\"real_kernel_waiter\":true,\"data32_noIRQ_preserves_high_word\":true,\"data64_verified_before_IRQ\":true,\"two_owned_two_plane_generations\":true,\"later_pending_preserves_label_and_IRQ\":true,\"resume_skips_published_owner\":true,\"CPU_mutation_blocks_completion\":true,\"guest_padding_preserved\":true,\"synthetic_depth_downloads\":true,\"actual_GPU_depth_publication\":false,\"frame_or_flip\":false}\n";return 0;
}

// Supervision SUP-028 GPU-resident EOP policy control. Authored guest page,
// genuine retained D32/MS2 depth image, real queue fence; no retail span claim.
inline int gnm_gpu_resident_eop_control(){
 struct State{bool passed=false;}state;
 const auto body=+[](const CtrVideoSessionApiV1* api,uint32_t,uint32_t,uint32_t,void* raw)->int{
  if(!api || !api->context)return 60;auto& state=*static_cast<State*>(raw);
  GuestStack memory_owner(32768,4096);auto* bytes=static_cast<unsigned char*>(memory_owner.bottom());std::fill_n(bytes,8192,static_cast<unsigned char>(0x25));
  auto* label=reinterpret_cast<uint64_t*>(bytes+4096);*label=0xa5a5a5a500000000ull;
  CtrDepthAttachmentParametersV1 parameters{1,sizeof(parameters),64,64,2,3,1};uint64_t token=0;
  int result=ctr_video_depth_attachment_create_v1(api->context,&parameters,&token);if(result)return result;if(!token)return 112;
  // Real provider lease survives release of the submission's public token.
  uint64_t lease=0;result=ctr_video_depth_attachment_retain_v1(api->context,token,&lease);
  if(result || !lease || lease==token){ctr_video_depth_attachment_release_v1(api->context,token);return result?result:112;}
  result=ctr_video_depth_attachment_release_v1(api->context,token);if(result){ctr_video_depth_attachment_release_v1(api->context,lease);return result;}token=lease;
  int released=88;auto owner=std::shared_ptr<void>(new uint64_t(token),[&](void* pointer){released=ctr_video_depth_attachment_release_v1(api->context,*static_cast<uint64_t*>(pointer));delete static_cast<uint64_t*>(pointer);});
  result=ctr_video_depth_attachment_clear_v1(api->context,token,0x3f800000,7);if(result)return result;
  GpuResidentMemory resident;const uint64_t begin=reinterpret_cast<uint64_t>(bytes);
  if(resident.protect(begin,4096,token,1,owner))return 112;owner.reset();
  struct Completion{void* session;unsigned fences=0;}completion{api->context};
  const auto finish=+[](void* raw)->int{auto& c=*static_cast<Completion*>(raw);const int status=ctr_video_queue_memory_visibility_v1(c.session);if(!status)++c.fences;return status;};
  const uint64_t address=reinterpret_cast<uint64_t>(label);uint32_t packet[]={0xc0044700,0x504,uint32_t(address),uint32_t(address>>32)|(1u<<29),0x12345678,0};GnmEopTransaction transaction;
  if(transaction.commit(packet,6,make_owned_guest_copy_api(),finish,&completion,nullptr,nullptr) || !transaction.completed || completion.fences!=1 || *label!=0xa5a5a5a512345678ull || resident.size()!=1 || released!=88)return 112;
  for(bool write:{false,true}){uint64_t fault=0,caller=0,operation=9;if(gpu_resident_access_probe(bytes,write,&resident,&fault,&caller,&operation)!=62 || fault!=begin || !caller || operation!=uint64_t(write))return 112;}
  uint64_t retained_token=0,generation=0;std::shared_ptr<void> resolved;
  if(resident.resolve(begin,4096,retained_token,generation,resolved) || retained_token!=token || generation!=1 || !resolved)return 112;
  if(ctr_video_depth_attachment_clear_v1(api->context,retained_token,0x3f000000,3) || resident.advance(begin,4096,token,1,2) || resident.advance(begin,4096,token,1,3)!=88)return 112;
  resolved.reset();if(resident.resolve(begin,4096,retained_token,generation,resolved) || generation!=2)return 112;resolved.reset();
  uint64_t alias=0;if(ctr_video_depth_sampled_alias_v1(api->context,token,&alias) || !alias || ctr_video_sampled_image_release_v1(api->context,alias))return 112;
  // Reproduce the runtime's post-EOP pending-depth WriteData failure with an
  // actual GPU owner. A stale generation cannot write the later label or fence.
  DepthPublicationOwners dirty;if(dirty.mark(token,2))return 112;
  struct Coverage{GpuResidentMemory* resident;uint64_t address,token,generation;}coverage{&resident,begin,token,1};
  const auto covered=+[](void* raw,uint64_t image,uint32_t writer)->int{auto& c=*static_cast<Coverage*>(raw);uint64_t token=0,generation=0;std::shared_ptr<void> owner;
   const int status=c.resident->resolve(c.address,4096,token,generation,owner);
   return status?status:(image==c.token && token==image && generation==c.generation && writer==generation && owner?0:88);
  };
  GnmImageCompletion work{0,&dirty,make_owned_guest_copy_api(),covered,&coverage,finish,&completion};
  uint32_t write[]={0xc0033700,0x500,uint32_t(address),uint32_t(address>>32),0x87654321};
  if(complete_write_data(write,5,work.memory,GnmImageCompletion::finish,&work)!=88 || completion.fences!=1 || *label!=0xa5a5a5a512345678ull)return 112;
  coverage.generation=2;work.colors=1;
  if(complete_write_data(write,5,work.memory,GnmImageCompletion::finish,&work)!=88 || completion.fences!=1)return 112;
  work.colors=0;
  if(complete_write_data(write,5,work.memory,GnmImageCompletion::finish,&work) || completion.fences!=2 || *label!=0xa5a5a5a587654321ull || dirty.empty())return 112;
  write[2]=uint32_t(begin);write[3]=uint32_t(begin>>32);
  if(complete_write_data(write,5,work.memory,GnmImageCompletion::finish,&work)!=1 || completion.fences!=3 || resident.size()!=1)return 112;

  if(resident.shutdown() || released || bytes[0]!=0x25 || ctr_video_depth_attachment_release_v1(api->context,token)!=62)return 112;
  state.passed=true;return 0;
 };
 const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,0,0};const int status=ctr_video_run_window_session_v1(&options,body,&state);if(status || !state.passed)return status?status:112;
 std::cout<<"{\"scope\":\"gnm_GPU_resident_EOP_control\",\"passed\":true,\"actual_D32_MS2_depth_owner\":true,\"actual_GPU_clear\":true,\"real_queue_fence\":true,\"EOP_label_verified\":true,\"owner_retained_after_EOP\":true,\"provider_lease_survives_original_token_release\":true,\"CPU_read_write_faults\":true,\"later_GPU_alias_retains_owner\":true,\"second_real_GPU_generation\":true,\"exact_address_lookup\":true,\"post_EOP_WriteData_real_fence\":true,\"stale_generation_blocks_label\":true,\"pending_color_blocks_label\":true,\"WriteData_cannot_overwrite_GPU_backing\":true,\"guest_depth_publication\":false,\"authored_page_not_retail_span\":true,\"R3\":false}\n";return 0;
}

}
