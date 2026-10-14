#include "gnm_insert_wait_flip.hpp"
// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "json2_compat.hpp"
#include "gnm_frame_end.hpp"
#include "rgba8_occupancy.hpp"
#include "bank_dispatch.hpp"
#include "exception_dispatch.hpp"
#include "gpu_resident_memory.hpp"
#include "depth_residency.hpp"
#include "depth_gpu_snapshot.hpp"
#include "original_call.hpp"
#include "process_guard.hpp"
#include "thread_keys.hpp"
#include "kernel_condition.hpp"
#include "sysmodule_service.hpp"
#include "direct_memory.hpp"
#include "online_traffic_key.hpp"
#include "gnm_init_dispatch.hpp"
#include "gnm_submission_snapshot.hpp"
#include "gnm_command_cursor.hpp"
#include "gnm_wait_reg_mem.hpp"
#include "graphics_auxiliary_client.hpp"
#include "compute_companion_client.hpp"
#include "compute_guest_writeback.hpp"
#include "color_guest_writeback.hpp"
#include "published_color_generations.hpp"
#include "retained_display_publication.hpp"
#include "gnm_acquire_mem.hpp"
#include "gnm_event_flush.hpp"
#include "gnm_dma_request.hpp"
#include "gnm_register_executor.hpp"
#include "color_sampled_coherence.hpp"
#include "video_color_batch.hpp"
#include "video_depth_batch.hpp"
#include "video/pm4_init_plan.hpp"
#include "video/compute_shader_state.hpp"
#include "video/graphics_shader_state.hpp"
#include "video/pm4_state_capture.h"
#include "pm4_observed_state.hpp"
#include "depth_attachment_metadata.hpp"
#include "video/shader_code_capture.hpp"
#include "gnm_submit_owner.hpp"
#include "gnm_flip_dispatch.hpp"
#include "shader_snapshot_owner.hpp"
#include "graphics_companion_client.hpp"
#include "graphics_pair_client.hpp"
#include "graphics_vertex_only_client.hpp"
#include "graphics_key_client.hpp"
#include "graphics_buffer_bindings.hpp"
#include "graphics_push_resources.hpp"
#include "video_buffer_batch.hpp"
#include "vertex_buffer_owners.hpp"
#include "draw_resource_inputs.hpp"
#include "indexed_draw_transport.hpp"
#include "gnm_route_handoff.hpp"
#include "gnm_eop_completion.hpp"
#include "depth_publication_owners.hpp"
#include "compute_sampled_refresh.hpp"
#include "compute_color_refresh.hpp"
#include "gnm_write_data.hpp"
#include "gnm_eos_completion.hpp"
#include "video_layout_batch.hpp"
#include "video_sampler_batch.hpp"
#include "video_image_batch.hpp"
#include "gnm_shader_dispatch.hpp"
#include "gnm_index_dispatch.hpp"
#include "pad_backend.hpp"
#include "ajm_context.hpp"
#include "audio_output.hpp"
#include "batch_map.hpp"
#include "kernel_usleep.hpp"
#include "video_session_owner.hpp"
#include "original_once_trace.hpp"
#include "tls_storage.hpp"
#include "startup_worker.hpp"
#include "push_marker_dispatch.hpp"
#include "console_output.hpp"
#include "save_data_initialize.hpp"
#include "user_service.hpp"
#include "cxa_guard.hpp"
#include "cxa_atexit.hpp"
#include "net_pool.hpp"
#include "ssl_context.hpp"
#include "http_context.hpp"
#include "np_web_context.hpp"
#include "flexible_memory.hpp"
#include "virtual_query.hpp"
#include "video_gamma_service.hpp"
#include "signal_mask.hpp"
#include "kernel_file.hpp"
#include "app_content.hpp"
#include "playgo_metadata.hpp"
#include "kernel_equeue.hpp"
#include "gpu_library.hpp"
#include "kernel_event_flag.hpp"
#include "startup_write_reload.hpp"
#include "gnm_service_pending.hpp"
#include "startup_mutex_reload.hpp"
#include "unlock_all.hpp"
#include <cstdio>
extern "C" void ctr_sysv_process_time();
extern "C" void ctr_sysv_posix_thread_attr_init();
extern "C" void ctr_sysv_posix_thread_attr_detach();
extern "C" void ctr_sysv_posix_thread_attr_stack_size();
extern "C" void ctr_sysv_posix_thread_attr_destroy();
namespace ctrport {
inline RetainedDisplayPublication retained_display_authority;
inline ComputeGuestWriteback retained_display_candidate;
inline uint64_t retained_display_generation=0;

extern "C" void ctr_jump_startup_owned(void*,void*,void*,void*,void*);
extern "C" void ctr_sysv_tls_address();
extern "C" void ctr_sysv_guest_errno();
extern "C" void ctr_sysv_application_heap_set();
extern "C" void ctr_sysv_heap_trace_query();
extern "C" void ctr_sysv_is_neo_mode();
extern "C" void ctr_sysv_allocate_direct();
extern "C" void ctr_sysv_map_direct();
extern "C" void ctr_sysv_asan_enabled();
extern "C" void ctr_sysv_orbis_mutex_lock();
extern "C" void ctr_sysv_thread_attributes_init();
extern "C" void ctr_sysv_thread_attributes_destroy();
extern "C" void ctr_sysv_thread_key_create();
extern "C" void ctr_sysv_cpu_mode();
extern "C" void ctr_sysv_tsc_frequency();
extern "C" void ctr_sysv_read_tsc();
extern "C" void ctr_sysv_semaphore_create();
extern "C" void ctr_sysv_semaphore_wait();
extern "C" void ctr_sysv_semaphore_poll();
extern "C" void ctr_sysv_system_parameter();
extern "C" void ctr_sysv_system_status();
extern "C" void ctr_sysv_netctl_info();
extern "C" void ctr_sysv_net_connection_state();
extern "C" void ctr_sysv_user_callback_register();
extern "C" void ctr_sysv_np_toolkit_register();
extern "C" void ctr_sysv_np_toolkit_check();
extern "C" void ctr_sysv_np_check();
extern "C" void ctr_sysv_ime_dialog_status();
extern "C" void ctr_sysv_np_toolkit_unregister();
extern "C" void ctr_sysv_net_toolkit_register();
extern "C" void ctr_sysv_net_toolkit_check();
extern "C" void ctr_sysv_posix_clock();
extern "C" void ctr_sysv_kernel_clock();
extern "C" void ctr_sysv_convert_local();
extern "C" void ctr_sysv_condition_init();
extern "C" void ctr_sysv_condition_signal();
extern "C" void ctr_sysv_condition_attributes_destroy();
extern "C" void ctr_sysv_thread_priority();
extern "C" void ctr_sysv_thread_inherit();
extern "C" void ctr_sysv_thread_policy();
extern "C" void ctr_sysv_thread_stack_size();
extern "C" void ctr_sysv_condition_attributes_init();
extern "C" void ctr_sysv_save_dialog_initialize();
extern "C" void ctr_sysv_save_dialog_status();
extern "C" void ctr_sysv_save_dialog_terminate();
extern "C" void ctr_sysv_common_dialog_initialize();
extern "C" void ctr_sysv_semaphore_signal();
extern "C" void ctr_sysv_user_service_initialize();
extern "C" void ctr_sysv_thread_set_affinity();
extern "C" void ctr_sysv_thread_key_get();
extern "C" void ctr_sysv_thread_key_set();
extern "C" void ctr_sysv_thread_key_delete();
extern "C" void ctr_sysv_thread_self();
extern "C" void ctr_sysv_thread_get_affinity();
extern "C" void ctr_sysv_thread_attributes_current();
extern "C" void ctr_sysv_thread_attributes_affinity();
extern "C" void ctr_sysv_orbis_mutex_unlock();
extern "C" void ctr_sysv_get_proc_param();
inline const void* process_parameter=nullptr;
extern "C" void ctr_sysv_thread_atexit_report_set();
extern "C" void ctr_sysv_thread_atexit_set();
extern "C" void ctr_sysv_thread_destructor_set();
extern "C" void ctr_sysv_direct_memory_query();
extern "C" void ctr_sysv_direct_memory_size();
extern "C" void ctr_sysv_font_select_library();
extern "C" void ctr_sysv_font_select_renderer();
extern "C" void ctr_sysv_font_memory_init();
extern "C" void ctr_sysv_font_memory_term();
extern "C" void ctr_sysv_net_init_compat();
extern "C" void ctr_sysv_shareplay_init_compat();
extern "C" void ctr_sysv_shareplay_info_compat();
extern "C" void ctr_sysv_shareplay_prohibition_compat();
extern "C" void ctr_sysv_live_stream_init_compat();
extern "C" void ctr_sysv_netctl_init_compat();
extern "C" void ctr_sysv_json2_0();
extern "C" void ctr_sysv_json2_1();
extern "C" void ctr_sysv_json2_2();
extern "C" void ctr_sysv_json2_3();
extern "C" void ctr_sysv_json2_4();
extern "C" void ctr_sysv_json2_5();
extern "C" void ctr_sysv_json2_6();
extern "C" void ctr_sysv_sysmodule_load();
extern "C" void ctr_sysv_random_bytes();
extern "C" void ctr_sysv_gettimeofday();
extern "C" void ctr_sysv_orbis_gettimeofday();
extern "C" void ctr_sysv_mutex_lock();
extern "C" void ctr_sysv_mutex_unlock();
extern "C" void ctr_sysv_condition_broadcast();
extern "C" void ctr_sysv_orbis_condition_broadcast();
extern "C" void ctr_sysv_mutex_attributes_init();
extern "C" void ctr_sysv_posix_mutexattr_init();
extern "C" void ctr_sysv_posix_mutex_init();
extern "C" void ctr_sysv_posix_mutex_trylock();
extern "C" void ctr_sysv_orbis_mutex_trylock();
extern "C" void ctr_sysv_posix_mutexattr_destroy();
extern "C" void ctr_sysv_posix_mutexattr_settype();
extern "C" void ctr_sysv_mutex_attributes_settype();
extern "C" void ctr_sysv_mutex_init();
extern "C" void ctr_sysv_mutex_attributes_destroy();
inline std::uint64_t boundary_target=0,boundary_source=0;
inline const MissingTrapBank* entry_function_bank=nullptr;
inline const MissingTrapBank* entry_data_bank=nullptr;
inline const std::vector<MappedImage>* entry_images=nullptr;
inline const std::vector<ImageMetadata>* entry_metadata=nullptr;
// Scoped shared execution path: no-flip Submit uses the same owned packet cursor
// and GPU completion/cleanup as SubmitAndFlip, without publishing a presentation.
inline LONG execute_ordered_submission(EXCEPTION_POINTERS* exception,bool flip_submission,uint64_t caller,bool native_bridge=false){

            // Metadata only, no submitted GPU work or persisted command payloads.
            // Public packet fields: pinned shadPS4 amdgpu/pm4_cmds.h (GPL-2.0-or-later).
            const auto memory=make_owned_guest_copy_api();const auto command_count=exception->ContextRecord->Rdi;
            std::array<uint64_t,3> flip_stack{};bool flip_stack_owned=false;const uint64_t flip_handle=exception->ContextRecord->R9;
            if(flip_submission) {
                auto& stack=flip_stack;const auto rsp=exception->ContextRecord->Rsp;
                const bool valid=rsp<=UINT64_MAX-32 && !memory.read(memory.context,rsp+8,stack.data(),24);flip_stack_owned=valid;
                std::cout<<"{\"scope\":\"original_GnmSubmitFlip_arguments\",\"count\":"<<command_count<<",\"video_handle\":"<<exception->ContextRecord->R9<<",\"stack_owned\":"<<(valid?"true":"false")<<",\"buffer_index\":"<<uint32_t(stack[0])<<",\"flip_mode\":"<<uint32_t(stack[1])<<",\"flip_arg\":"<<stack[2]<<",\"effects\":false}\n";
            }
            bool completed_single_submission=false;uint64_t completed_present_count=0,completed_present_token=0,completed_present_argument=0;
            static std::atomic<uint64_t> submission_sequence{0};
            const uint64_t submission_token=++submission_sequence;
            GnmSubmissionSnapshot submission;
            const bool arrays=capture_gnm_submission(static_cast<uint32_t>(command_count),exception->ContextRecord->Rsi,
                exception->ContextRecord->Rdx,exception->ContextRecord->Rcx,exception->ContextRecord->R8,submission);
            std::cout<<"{\"scope\":\"original_GnmSubmit_arrays\",\"command_count\":"<<command_count<<",\"owned_arrays\":"<<(arrays?"true":"false")<<"}\n";
            if(arrays)for(size_t i=0;i<submission.draw.size();++i)for(unsigned kind=0;kind<2;++kind){
                const auto& buffer=kind?submission.constant[i]:submission.draw[i];
                const auto address=buffer.guest_address;const auto& words=buffer.words;const auto bytes=words.size()*4;
                if(!bytes){std::cout<<"{\"scope\":\"original_GnmSubmit_empty\",\"buffer\":"<<i<<",\"ccb\":"<<(kind?"true":"false")<<",\"address\":"<<address<<"}\n";continue;}
                {
                    const ULONGLONG preparation_started=GetTickCount64();
                    std::vector<CtrPm4::Packet> ordered;const int decode=CtrPm4::DecodePackets(words.data(),words.size(),ordered);
                    auto observed=std::make_unique<CtrPm4::Registers>();std::array<bool,80> seen{};std::array<bool,512> graphics_seen{};bool first_draw=false;std::ostringstream graphics_log,census_log;size_t register_failures=0;uint32_t failed_register_opcode=0,failed_register_offset=0;
                    uint64_t completed_init=0;int retained_init_status=88;
                    std::shared_ptr<RuntimeVideoSession> graphics_session;
                    {std::lock_guard<std::mutex> lock(runtime_video_mutex);graphics_session=runtime_video_session;}
                    if(graphics_session)retained_init_status=graphics_session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_capture_pm4_state_v1(api->context,observed->words.data(),static_cast<uint32_t>(observed->words.size()),observed->compute.data(),static_cast<uint32_t>(observed->compute.size()),&completed_init);});
                    CtrGraphicsProfileV1 device_profile{};int profile_status=88;
                    if(graphics_session)profile_status=graphics_session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_capture_graphics_profile_v1(api->context,&device_profile);});
                    uint32_t mixed_depth_authority=0;int mixed_depth_status=88;
                    if(graphics_session)mixed_depth_status=graphics_session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_mixed_depth_authority_v1(api->context,&mixed_depth_authority);});
                    // Capture selected-device authority before any shader/key capture. This
                    // runtime currently admits only the genuinely queried disabled branch.
                    if(!mixed_depth_status && (mixed_depth_authority!=1 && mixed_depth_authority!=3))mixed_depth_status=88;
                    if(mixed_depth_status)profile_status=mixed_depth_status;
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_mixed_depth_authority\",\"status\":"<<mixed_depth_status<<",\"mask\":"<<mixed_depth_authority<<",\"queried\":"<<((mixed_depth_authority&1)?"true":"false")<<",\"AMD_supported\":"<<((mixed_depth_authority&2)?"true":"false")<<",\"enabled\":"<<((mixed_depth_authority&4)?"true":"false")<<",\"before_shader_capture\":true}\n";
                    std::unique_ptr<CtrPm4::Registers> first_graphics_registers;
                    auto first_graphics_binaries=std::make_unique<std::array<CtrPm4::OwnedShaderBinary,2>>();
                    auto first_graphics_states=std::make_unique<std::array<CtrPm4::GraphicsShaderState,2>>();
                    GnmCommandCursor command_cursor;
                    const int command_capture=command_cursor.capture(submission_token,static_cast<uint32_t>(i),kind,words.data(),words.size());
                    // Any potential writer invalidates cross-submission authority before execution.
                    std::vector<CtrPm4::Packet> retained_packets;
                    const int retained_decode=CtrPm4::DecodePackets(words.data(),words.size(),retained_packets);
                    bool retained_readonly=!retained_decode;
                    for(const auto& packet:retained_packets)if(packet.opcode!=0x10 && packet.opcode!=0x28 && packet.opcode!=0x69)retained_readonly=false;
                    if(!retained_readonly){retained_display_authority.invalidate();retained_display_candidate=ComputeGuestWriteback{};}

                    GnmRegisterExecutor ordered_registers(*observed);
                    const int command_pending=command_capture?command_capture:command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){if(packet.opcode==0x3c)return poll_wait_reg_mem(data,packet.words,ordered_registers.state(),memory);const int status=ordered_registers.execute(packet,data);if(!status && packet.opcode==0x50)std::cout<<"{\"scope\":\"original_GnmSubmitFlip_ordered_dma_ignored\",\"word\":"<<packet.word<<",\"destination_low\":"<<data[4]<<",\"public_handler_rule\":true,\"GPU_execution\":false}\n";return status;});
                    const auto* pending_packet=command_cursor.pending();
                    if(pending_packet)census_log<<"{\"scope\":\"original_GnmSubmitFlip_pending_command_kind\",\"word\":"<<pending_packet->word<<",\"opcode\":"<<pending_packet->opcode<<",\"annotation_tag\":"<<(pending_packet->opcode==0x10 && pending_packet->words>1?words[pending_packet->word+1]:0)<<",\"nop_annotation_marker\":"<<(pending_packet->opcode==0x10 && pending_packet->words>1 && (words[pending_packet->word+1]&0xffff0000)==0x68750000?"true":"false")<<",\"GPU_execution\":false}\n";
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_command_cursor\",\"capture_status\":"<<command_capture<<",\"status\":"<<command_pending<<",\"packets\":"<<command_cursor.count()<<",\"words\":"<<command_cursor.words()<<",\"completed_packets\":"<<command_cursor.completed()<<",\"pending_word\":"<<(pending_packet?pending_packet->word:SIZE_MAX)<<",\"pending_opcode\":"<<(pending_packet?pending_packet->opcode:UINT32_MAX)<<",\"GPU_execution\":false}\n";
                    GraphicsSubmissionOwner submission_owner;
                    std::vector<std::shared_ptr<GraphicsVertexOnlyOwner>> vertex_only_stages;
                    std::vector<std::shared_ptr<GraphicsPairOwner>> paired_stages;uint64_t paired_host_bytes=0;uint64_t vertex_fetch_bytes=0;
                    struct DepthAllocationObservation{uint32_t word;CtrDepthAttachmentParametersV1 parameters;bool extent_valid;uint32_t clear_flags,depth_bits,stencil;std::array<uint32_t,12> identity{};};std::vector<DepthAllocationObservation> depth_allocations;
                    auto first_graphics_compiled_stages=std::make_unique<std::array<std::shared_ptr<GraphicsStageOwner>,2>>();
                    auto graphics_backing=direct_memory_backing;
                    auto graphics_inputs=graphics_backing?std::make_unique<ShaderSnapshotOwner>(graphics_backing):nullptr;
                    std::ostringstream prefix_dma;size_t prefix_dma_count=0;
                    std::string initial_targets=graphics_targets_json(*observed);
                    std::vector<std::array<uint32_t,3>> target_writes,full_target_writes;
                    std::ostringstream execution_routes;size_t route_count=0;bool first_compute=false;
                    const auto census_limit=graphics_census_limit();unsigned census_stages=0;size_t first_draw_word=SIZE_MAX;
                    std::vector<std::pair<uint32_t,std::shared_ptr<ComputeStageOwner>>> compiled_computes;
                    bool index_type_observed=false,depth_hint_observed=false;uint32_t depth_hint=0;
                    for(const auto& packet:ordered){const auto* data=words.data()+packet.word;
                     if(packet.opcode==0x58){GnmAcquireMemRequest request;const int acquired=prepare_acquire_mem(data,packet.words,request);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_acquire_mem_request\",\"word\":"<<packet.word<<",\"status\":"<<acquired<<",\"coher_control\":"<<request.control<<",\"size_low\":"<<request.size_low<<",\"size_high\":"<<request.size_high<<",\"base_low\":"<<request.base_low<<",\"base_high\":"<<request.base_high<<",\"poll_interval\":"<<request.poll_interval<<",\"real_provider_missing\":true,\"GPU_execution\":false}\n";}
                     if(packet.opcode==0x50){GnmDmaRequest request;const int status=prepare_dma(data,packet.words,request);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_dma_request\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"control\":"<<request.control<<",\"command\":"<<request.command<<",\"source_kind\":"<<request.source_kind<<",\"destination_kind\":"<<request.destination_kind<<",\"bytes\":"<<request.bytes<<",\"data\":"<<request.data<<",\"source\":"<<request.source<<",\"destination\":"<<request.destination<<",\"GPU_execution\":false}\n";}
                     if(packet.opcode==0x48){GnmEosRequest request;const int status=prepare_eos(data,packet.words,request);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_eos_request\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"event_type\":"<<request.event_type<<",\"event_control_raw\":"<<data[1]<<",\"command_control_raw\":"<<data[3]<<",\"event_index\":"<<request.event_index<<",\"command\":"<<request.command<<",\"address\":"<<request.address<<",\"data\":"<<request.data<<",\"GPU_execution\":false}\n";}
                     if(packet.opcode==0x12)index_type_observed=false;
                     // Public liverpool.cpp DbZInfo hint is a trailing NOP in the
                     // owned command stream, not part of the register payload.
                     if(packet.opcode==0x69 && packet.words>=3 && data[1]==0x10){
                      depth_hint=0;depth_hint_observed=true;
                      if(packet.words==10){
                       if(words.size()-packet.word>=24 && data[22]==0xc0001000)depth_hint=data[23];
                       else depth_hint_observed=false;
                      }
                     }
                     if(packet.opcode==0x2a && packet.words==2){observed->words[0xa29f]=data[1];index_type_observed=true;}
                     // Exact public PM4DmaData layout, diagnostic only. These
                     // commands precede the first draw and cannot be skipped.
                     if(!first_draw && packet.opcode==0x50 && packet.words==7){
                      if(prefix_dma_count++)prefix_dma<<',';
                      prefix_dma<<"{\"word\":"<<packet.word<<",\"source_selector\":"<<((data[1]>>29)&3)
                       <<",\"destination_selector\":"<<((data[1]>>20)&3)<<",\"engine\":"<<(data[1]&1)
                       <<",\"cp_sync\":"<<(data[1]>>31)<<",\"bytes\":"<<(data[6]&0x1fffff)
                       <<",\"command_flags\":"<<(data[6]&~0x1fffffu);
                      const auto source_selector=(data[1]>>29)&3,destination_selector=(data[1]>>20)&3;
                      if(source_selector==0 || source_selector==3)prefix_dma<<",\"source_address\":"<<(uint64_t(data[2])|(uint64_t(data[3])<<32));
                      if(destination_selector==0 || destination_selector==3)prefix_dma<<",\"destination_address\":"<<(uint64_t(data[4])|(uint64_t(data[5])<<32));
                      prefix_dma<<'}';
                     }
                     if(packet.opcode==0x12 || packet.opcode==0x69 || packet.opcode==0x79 || packet.opcode==0x2f || packet.opcode==0x68 || packet.opcode==0x2a){
                      CtrPm4::InitOperation op{packet.opcode,std::vector<uint32_t>(data+1,data+packet.words)};
                      if(observe_pm4_register(op,*observed)){++register_failures;failed_register_opcode=op.opcode;failed_register_offset=op.payload.empty()?UINT32_MAX:op.payload[0];}
                      else if(packet.opcode==0x12){graphics_seen.fill(false);if(!first_draw){initial_targets=graphics_targets_json(*observed);target_writes.clear();}}
                      else if(packet.opcode==0x69 && packet.words>=3){
                       for(size_t j=0;j<packet.words-2;++j){const uint32_t index=0xa000+data[1]+static_cast<uint32_t>(j);
                        if(graphics_target_register(index)){full_target_writes.push_back({static_cast<uint32_t>(packet.word),index,data[j+2]});if(!first_draw)target_writes.push_back(full_target_writes.back());}}
                      }
                     }
                     if(packet.opcode==0x76 && packet.words>=3 && data[1]<0x200 && packet.words-2<=0x200-data[1]){
                      CtrPm4::InitOperation op{0x76,std::vector<uint32_t>(data+1,data+packet.words)};
                      if(!CtrPm4::ApplyRegister(op,*observed))for(size_t j=data[1];j<data[1]+packet.words-2;++j)graphics_seen[j]=true;
                     }
                     if(packet.opcode==0x2d || packet.opcode==0x27 || packet.opcode==0x15)submission_owner.retain_route(static_cast<uint32_t>(packet.word),packet.opcode,data,packet.words,*observed);
                     if(packet.opcode==0x27){
                      CtrGraphicsBufferRequirementV1 requirement{};
                      const int needed=graphics_index_requirement(data,packet.words,observed->words[0xa29f],index_type_observed,requirement);
                      GraphicsBufferCapture capture;capture.requirement_status=needed;
                      if(!needed && graphics_inputs){const auto inputs=graphics_inputs->api();const auto ranges=graphics_inputs->ranges();capture=capture_graphics_buffer(inputs,ranges,requirement,16*1024*1024);}
                      const auto copied=capture.snapshot.size();int retained=88;
                      if(!needed && !capture.capture_status)retained=submission_owner.retain_index(static_cast<uint32_t>(packet.word),std::move(capture));
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_index_capture\",\"word\":"<<packet.word<<",\"type_observed\":"<<(index_type_observed?"true":"false")<<",\"type\":"<<observed->words[0xa29f]<<",\"requirement_status\":"<<needed<<",\"address\":"<<requirement.guest_address<<",\"bytes\":"<<requirement.bytes<<",\"count\":"<<requirement.num_records<<",\"capture_status\":"<<capture.capture_status<<",\"lease_status\":"<<capture.lease_status<<",\"release_status\":"<<capture.release_status<<",\"lease_end_status\":"<<capture.lease_end_status<<",\"copied_bytes\":"<<copied<<",\"retained_status\":"<<retained<<",\"gpu_execution\":false}\n";
                     }
                     if(!first_draw && (packet.opcode==0x2d || packet.opcode==0x27)){
                      first_draw=true;first_draw_word=packet.word;first_graphics_registers=std::make_unique<CtrPm4::Registers>(*observed);
                      // Public whole packet layouts, pm4_cmds.h DrawIndexAuto/DrawIndex2;
                      // metadata only, no index memory access or draw admission.
                      const bool draw_layout=(packet.opcode==0x2d && packet.words==3) || (packet.opcode==0x27 && packet.words==6);
                      const uint32_t draw_count=draw_layout?data[packet.opcode==0x2d?1:4]:0;
                      const uint32_t draw_initiator=draw_layout?data[packet.opcode==0x2d?2:5]:0;
                      for(bool vertex:{true,false}){
                       auto& state=(*first_graphics_states)[vertex?0:1];const int status=CtrPm4::CaptureGraphicsShaderState(*first_graphics_registers,vertex,state);
                       const size_t base=vertex?0x48:8;bool required=true;
                       for(size_t j=0;j<4;++j)required=required&&graphics_seen[base+j];
                       for(size_t j=0;j<state.userRegisters && j<16;++j)required=required&&graphics_seen[base+4+j];
                       uint64_t available=0;{std::lock_guard<std::mutex> guard(owned_output_lock);for(const auto& owner:owned_output_extents)for(const auto& extent:owner.second)if(state.codeAddress>=extent.first && state.codeAddress<extent.second)available=(std::max)(available,extent.second-state.codeAddress);}
                       auto& binary=(*first_graphics_binaries)[vertex?0:1];int captured=88;
                       if(!status && required && available>=8 && graphics_inputs){const auto inputs=graphics_inputs->api();captured=CtrPm4::CaptureShaderBinaryInputs(&inputs,state.codeAddress,static_cast<size_t>((std::min)(available,uint64_t(65564)))&~size_t(3),binary);}
                       std::ostringstream line;line<<"{\"scope\":\"original_GnmSubmitFlip_first_graphics\",\"word\":"<<packet.word<<",\"draw_opcode\":"<<packet.opcode<<",\"draw_layout_valid\":"<<(draw_layout?"true":"false")<<",\"draw_count\":"<<draw_count<<",\"draw_initiator\":"<<draw_initiator<<",\"vertex\":"<<(vertex?"true":"false")<<",\"state_status\":"<<status<<",\"required_registers_observed\":"<<(required?"true":"false")<<",\"code_address\":"<<state.codeAddress<<",\"owned_extent_remaining\":"<<available<<",\"user_register_count\":"<<state.userRegisters<<",\"register_apply_failures\":"<<register_failures<<",\"failed_register_opcode\":"<<failed_register_opcode<<",\"failed_register_offset\":"<<failed_register_offset<<",\"retained_init_state_included\":"<<(!retained_init_status?"true":"false")<<",\"retained_init_status\":"<<retained_init_status<<",\"completed_init_submissions\":"<<completed_init<<",\"binary_capture_status\":"<<captured;
                       GraphicsCompileObservation compiled;
                       if(!captured && !profile_status && !register_failures && !retained_init_status && graphics_inputs){
                        const auto inputs=graphics_inputs->api();const auto ranges=graphics_inputs->ranges();compiled=compile_graphics_observed(inputs,state.codeAddress,static_cast<uint64_t>(binary.words.size()*4),*first_graphics_registers,vertex,device_profile,ranges);
                       }
                       line<<",\"profile_status\":"<<profile_status<<",\"profile_max_ubo\":"<<device_profile.max_ubo_size
                           <<",\"profile_subgroup\":"<<device_profile.subgroup_size<<",\"profile_spirv\":"<<device_profile.supported_spirv
                           <<",\"compiler_status\":"<<compiled.status<<",\"compiler_spirv_words\":"<<compiled.words
                           <<",\"compiler_loader_error\":"<<compiled.loader_error<<",\"compiler_diagnostic\":"<<graphics_json_text(compiled.diagnostic);
                       if(compiled.resources)line<<",\"compiler_resources\":"<<graphics_resources_json(*compiled.resources);
                       if(compiled.stage){(*first_graphics_compiled_stages)[vertex?0:1]=compiled.stage;submission_owner.retain(static_cast<uint32_t>(packet.word),vertex,compiled.stage);line<<",\"owned_compiled_stage\":"<<graphics_stage_json(*compiled.stage);}
                       if(graphics_inputs){const auto counts=graphics_inputs->lifetime_counts();line<<",\"owned_input_lifetime\":{\"range_begins\":"<<counts[0]<<",\"range_ends\":"<<counts[1]<<",\"active_leases\":"<<counts[2]<<",\"active_snapshots\":"<<counts[3]<<'}';}
                       // Metadata only: exact public regs.cpp indices at pinned e3ce810.
                       // No routing admission, Profile defaults or descriptor values persisted.
                       const auto& flat=first_graphics_registers->words;
                       line<<",\"render_target_metadata\":"<<graphics_targets_json(*first_graphics_registers);
                       line<<",\"render_target_initial\":"<<initial_targets<<",\"render_target_writes\":[";
                       for(size_t j=0;j<target_writes.size();++j){if(j)line<<',';const auto& w=target_writes[j];line<<'['<<w[0]<<','<<w[1]<<','<<w[2]<<']';}line<<']';
                       line<<",\"pipeline_metadata\":{\"stage_enable_raw\":"<<flat[0xa2d5]
                           <<",\"primitive_type_raw\":"<<flat[0xc242]<<",\"clipper_control_raw\":"<<flat[0xa204]
                           <<",\"vs_output_control_raw\":"<<flat[0xa207]<<",\"instance_step_rate_0\":"<<flat[0xa2a8]
                           <<",\"instance_step_rate_1\":"<<flat[0xa2a9]<<",\"ps_input_ena_raw\":"<<flat[0xa1b3]
                           <<",\"ps_input_addr_raw\":"<<flat[0xa1b4]<<",\"device_profile_observed\":"<<(!profile_status?"true":"false")<<"}"
                           <<",\"allocated_vgprs\":"<<state.allocatedVgprs<<",\"input_vgprs\":"<<state.inputVgprs
                           <<",\"scratch_enabled\":"<<(state.scratch?"true":"false")<<",\"oc_lds_enabled\":"<<(state.ocLds?"true":"false");
                       if(!captured)line<<",\"validated_code_bytes\":"<<binary.metadata.codeBytes<<",\"footer_offset\":"<<binary.metadata.footer;
                       line<<",\"source_writer_coherence\":false,\"descriptor_binding\":false,\"gpu_execution\":false}\n";
                       graphics_log<<line.str();std::cout<<line.str();
                      }
                     }
                     if(packet.opcode==0x76 && packet.words>=3 && data[1]>=0x200 && data[1]<0x250 && packet.words-2<=0x250-data[1]){
                      CtrPm4::InitOperation op{0x76,std::vector<uint32_t>(data+1,data+packet.words)};
                      if(!CtrPm4::ApplyRegister(op,*observed))for(size_t j=data[1]-0x200;j<data[1]-0x200+packet.words-2;++j)seen[j]=true;
                     }
                     if(packet.opcode==0x27 || packet.opcode==0x2d || packet.opcode==0x15){
                      if(packet.opcode!=0x15 && packet.word>first_draw_word && graphics_inputs && graphics_backing && !profile_status && !retained_init_status && !register_failures){
                       for(bool vertex:{true,false})if(census_stages<census_limit){++census_stages;const auto census_line=graphics_census_stage(static_cast<uint32_t>(packet.word),vertex,*observed,device_profile,*graphics_inputs,*graphics_backing,&submission_owner);census_log<<census_line;std::cout<<census_line;std::cout.flush();}
                      }
                      if(packet.opcode==0x15 && census_limit && graphics_inputs && graphics_backing && !retained_init_status && !register_failures){const auto census_line=compute_census_stage(static_cast<uint32_t>(packet.word),*observed,*graphics_inputs,*graphics_backing,data,packet.words,&submission_owner);census_log<<census_line;std::cout<<census_line;std::cout.flush();
                       if(!profile_status && submission_owner.computes.size()<=4){const auto inputs=graphics_inputs->api();const auto ranges=graphics_inputs->ranges();std::shared_ptr<ComputeStageOwner> compiled;std::string reason;
                        const int status=compile_owned_compute(inputs,ranges,submission_owner.computes.back(),device_profile,compiled,reason);
                        if(!status && compiled)compiled_computes.push_back({static_cast<uint32_t>(packet.word),compiled});
                        census_log<<"{\"scope\":\""<<(submission_owner.computes.size()==1?"original_GnmSubmitFlip_first_compute_compiled":"original_GnmSubmitFlip_compute_compiled")<<"\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"diagnostic\":"<<graphics_json_text(reason)<<",\"spirv_words\":"<<(compiled?compiled->stage->spirv.size():0)<<",\"descriptor_count\":"<<(compiled?compiled->bindings.descriptor_count:0)<<",\"resources_buffers\":"<<(compiled?compiled->stage->resources.buffers:0)<<",\"resources_images\":"<<(compiled?compiled->stage->resources.images:0)<<",\"resources\":"<<(compiled?compiled->resource_metadata:"[]")<<",\"owned_after_DLL_unload\":true,\"GPU_execution\":false}\n";
                       }
                      }
                      if(route_count++)execution_routes<<',';
                      execution_routes<<"{\"word\":"<<packet.word<<",\"opcode\":"<<packet.opcode;
                      if(packet.opcode==0x15){CtrPm4::ComputeShaderState state;const int status=CtrPm4::CaptureComputeShaderState(*observed,state);execution_routes<<",\"compute_status\":"<<status<<",\"compute_code\":"<<state.codeAddress;}
                      else{CtrPm4::GraphicsShaderState vs,ps;const int vs_status=CtrPm4::CaptureGraphicsShaderState(*observed,true,vs),ps_status=CtrPm4::CaptureGraphicsShaderState(*observed,false,ps);
                       std::shared_ptr<GraphicsPairOwner> paired;std::string pair_reason;int pair_status=88;
                       if(!vs_status && !ps_status && graphics_inputs && !profile_status && !retained_init_status && !register_failures){
                        const auto inputs=graphics_inputs->api();const auto ranges=graphics_inputs->ranges();
                        const ULONGLONG pair_compile_started=GetTickCount64();
                        pair_status=compile_graphics_pair(inputs,ranges,*observed,device_profile,ps.codeAddress,vs.codeAddress,paired,pair_reason);
                        char phase_flag[2]{};if(GetEnvironmentVariableA("CTR_TILING_TIMING",phase_flag,sizeof(phase_flag))==1 && phase_flag[0]=='1'){
                         const auto pair_compile_elapsed=GetTickCount64()-pair_compile_started;int fragment_hash_status=-1,vertex_hash_status=-1;
                         const auto fragment_hash=paired?graphics_spirv_sha256(paired->fragment->spirv,fragment_hash_status):std::string{};
                         const auto vertex_hash=paired?graphics_spirv_sha256(paired->vertex->spirv,vertex_hash_status):std::string{};
                         std::cout<<"{\"scope\":\"original_GnmSubmitFlip_pair_compile_timing\",\"submission\":"<<submission_token<<",\"word\":"<<packet.word<<",\"elapsed_ms\":"<<pair_compile_elapsed<<",\"status\":"<<pair_status<<",\"fragment_sha256\":\""<<fragment_hash<<"\",\"vertex_sha256\":\""<<vertex_hash<<"\",\"fragment_hash_status\":"<<fragment_hash_status<<",\"vertex_hash_status\":"<<vertex_hash_status<<",\"includes_host_thread_load_compile_sink\":true,\"hash_time_excluded\":true}\n";
                        }
                        if(!pair_status){const uint64_t pair_bytes=(paired->fragment->spirv.size()+paired->vertex->spirv.size()+paired->fragment->flattened_userdata.size()+paired->vertex->flattened_userdata.size())*4+0xD000*4;
                         if(pair_bytes>64*1024*1024-paired_host_bytes){pair_status=88;paired.reset();pair_reason="Resident paired stage budget exceeded";}
                         else{pair_status=graphics_pair_retain_registers(*paired,*observed,submission_token,static_cast<uint32_t>(i),kind,static_cast<uint32_t>(packet.word));
                          if(!pair_status){if(submission_owner.routes.empty())pair_status=112;else pair_status=graphics_pair_attach_command(*paired,submission_owner.routes.back(),device_profile);}
                          if(!pair_status)pair_status=command_cursor.matches(paired->submission_token,paired->command_buffer_index,paired->command_buffer_kind,paired->word,paired->original_packet.data(),paired->original_packet.size());
                          if(pair_status){paired.reset();pair_reason="Whole paired register/command/profile retention failed";}
                          else{paired->draw_inputs_status=prepare_draw_packet(paired->original_packet.data(),paired->original_packet.size(),paired->draw_inputs);census_log<<draw_packet_event(paired->word,paired->draw_inputs_status,paired->draw_inputs);
                          paired->graphics_key_status=decode_owned_graphics_key(*paired->registers,paired->fragment_pipeline.mrt_mask,paired->graphics_key);
                          census_log<<graphics_key_event(paired->word,true,paired->graphics_key_status,paired->graphics_key);
                          paired->vertex_fetch=capture_vertex_fetch(inputs,ranges,*paired->vertex,paired->vertex_pipeline,vertex_fetch_bytes);
                          census_log<<vertex_fetch_event(paired->word,paired->vertex_fetch);
                          paired_host_bytes+=pair_bytes;paired_stages.push_back(paired);}}}
                       }
                       if(!vs_status && !ps.codeAddress && graphics_inputs && !profile_status && !retained_init_status && !register_failures){
                        std::shared_ptr<GraphicsVertexOnlyOwner> vertex_only;std::string reason;
                        const auto inputs=graphics_inputs->api();const auto ranges=graphics_inputs->ranges();
                        int compiled=compile_graphics_vertex_only(inputs,ranges,*observed,device_profile,vs.codeAddress,vertex_only,reason);
                        if(!compiled){
                         if(submission_owner.routes.empty() || vertex_only_stages.size()>=8)compiled=112;
                         else{const auto& route=submission_owner.routes.back();
                          if(command_cursor.matches(submission_token,static_cast<uint32_t>(i),kind,route.word,route.packet.data(),route.packet.size()) || route.word!=packet.word || !route.registers || std::memcmp(route.registers->words.data(),observed->words.data(),0xD000*4))compiled=112;
                          else{vertex_only->submission=submission_token;vertex_only->buffer=static_cast<uint32_t>(i);vertex_only->kind=kind;vertex_only->word=static_cast<uint32_t>(packet.word);vertex_only->opcode=route.opcode;
                           vertex_only->registers=route.registers;vertex_only->packet=route.packet;vertex_only->profile=device_profile;vertex_only->draw_inputs_status=prepare_draw_packet(vertex_only->packet.data(),vertex_only->packet.size(),vertex_only->draw_inputs);census_log<<draw_packet_event(vertex_only->word,vertex_only->draw_inputs_status,vertex_only->draw_inputs);
                           vertex_only->graphics_key_status=decode_owned_graphics_key(*vertex_only->registers,0,vertex_only->graphics_key);
                           census_log<<graphics_key_event(vertex_only->word,false,vertex_only->graphics_key_status,vertex_only->graphics_key);
                           vertex_only->vertex_fetch=capture_vertex_fetch(inputs,ranges,*vertex_only->vertex,vertex_only->pipeline,vertex_fetch_bytes);
                           census_log<<vertex_fetch_event(vertex_only->word,vertex_only->vertex_fetch);vertex_only_stages.push_back(vertex_only);}}
                        }
                        int attached=88;
                        if(!compiled)for(const auto& entry:submission_owner.entries)if(entry.word==packet.word && entry.vertex){attached=graphics_vertex_only_attach_inputs(*vertex_only,entry.stage);break;}
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only\",\"word\":"<<packet.word<<",\"status\":"<<compiled<<",\"diagnostic\":"<<graphics_json_text(reason)<<",\"vertex_spirv_words\":"<<(vertex_only?vertex_only->vertex->spirv.size():0)<<",\"descriptor_count\":"<<(vertex_only?vertex_only->bindings.descriptor_count:0)<<",\"fetch_attributes\":"<<(vertex_only?vertex_only->pipeline.attribute_count:0)<<",\"register_words\":"<<(vertex_only && vertex_only->registers?vertex_only->registers->words.size():0)<<",\"input_join_status\":"<<attached<<",\"cpu_input_status\":"<<(vertex_only?vertex_only->input_status:88)<<",\"fragment_present\":false,\"GPU_execution\":false}\n";
                       }
                       const auto& depth_bank=observed->words;const uint32_t depth_ctl=depth_bank[0xa200],depth_z=depth_bank[0xa010];
                       if(depth_ctl&3){const uint32_t width=depth_hint&65535,height=depth_hint>>16,view=depth_bank[0xa002];
                        const bool extent_valid=depth_hint_observed && width && height && width<=((depth_bank[0xa016]&2047)+1)*8 && height<=(((depth_bank[0xa016]>>11)&2047)+1)*8 && !(view&0x00ffe7ff);
                        depth_allocations.push_back({static_cast<uint32_t>(packet.word),{1,sizeof(CtrDepthAttachmentParametersV1),width,height,1u<<((depth_z>>2)&3),depth_z&3,depth_bank[0xa011]&1},extent_valid,depth_bank[0xa000]&3,depth_bank[0xa00b],depth_bank[0xa00a]});for(uint32_t identity_index=0;identity_index<10;++identity_index)depth_allocations.back().identity[identity_index]=depth_bank[0xa00e+identity_index];depth_allocations.back().identity[10]=depth_bank[0xa002];depth_allocations.back().identity[11]=depth_bank[0xa005];
                       }
                       if(paired){std::shared_ptr<const GraphicsStageOwner> fragment_input,vertex_input;
                        for(const auto& retained:submission_owner.entries)if(retained.word==packet.word){if(retained.vertex)vertex_input=retained.stage;else fragment_input=retained.stage;}
                        const int retained=graphics_pair_attach_inputs(*paired,fragment_input,vertex_input,0);
                        const int attached=retained?retained:graphics_pair_attach_inputs(*paired,fragment_input,vertex_input);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_pair_inputs\",\"word\":"<<packet.word<<",\"status\":"<<attached<<",\"attached\":"<<(!attached?"true":"false")<<",\"source_writer_coherence\":false,\"gpu_execution\":false}\n";
                       }
                       const auto pair_line=graphics_pair_metadata(static_cast<uint32_t>(packet.word),pair_status,paired,pair_reason);census_log<<pair_line;std::cout<<pair_line;
                       execution_routes<<",\"vertex_status\":"<<vs_status<<",\"pixel_status\":"<<ps_status<<",\"vertex_code\":"<<vs.codeAddress<<",\"pixel_code\":"<<ps.codeAddress<<",\"target0_base_raw\":"<<observed->words[0xa318]
                        <<",\"stage_enable_raw\":"<<observed->words[0xa2d5]<<",\"shader_mask_raw\":"<<observed->words[0xa08f]<<",\"depth_control_raw\":"<<observed->words[0xa200]<<",\"color_control_raw\":"<<observed->words[0xa202];}
                      execution_routes<<",\"depth_attachment\":"<<depth_attachment_metadata(*observed,depth_hint_observed,depth_hint)<<'}';
                     }
                     if(packet.opcode==0x15 && !first_compute){first_compute=true;CtrPm4::ComputeShaderState state;const int status=CtrPm4::CaptureComputeShaderState(*observed,state);
                      bool required=seen[7]&&seen[8]&&seen[9]&&seen[12]&&seen[13]&&seen[18]&&seen[19];
                      for(size_t j=0;j<state.userRegisters && j<16;++j)required=required&&seen[64+j];
                      uint64_t available=0;{std::lock_guard<std::mutex> guard(owned_output_lock);for(const auto& owner:owned_output_extents)for(const auto& extent:owner.second)if(state.codeAddress>=extent.first && state.codeAddress<extent.second)available=(std::max)(available,extent.second-state.codeAddress);}
                      CtrPm4::OwnedShaderBinary binary;int captured=88;bool immutable=false;
                      if(!status && required && available>=8){auto backing=direct_memory_backing;if(backing){ShaderSnapshotOwner snapshots(backing);const auto inputs=snapshots.api();
                       captured=CtrPm4::CaptureShaderBinaryInputs(&inputs,state.codeAddress,static_cast<size_t>((std::min)(available,uint64_t(65564)))&~size_t(3),binary);immutable=!captured;}}
                      std::cout<<"{\"scope\":\"original_GnmSubmitFlip_first_compute\",\"word\":"<<packet.word<<",\"state_status\":"<<status<<",\"required_registers_observed\":"<<(required?"true":"false")<<",\"code_address\":"<<state.codeAddress<<",\"owned_extent_remaining\":"<<available<<",\"user_register_count\":"<<state.userRegisters<<",\"workgroup\":["<<state.workgroup[0]<<","<<state.workgroup[1]<<","<<state.workgroup[2]<<"],\"binary_capture_status\":"<<captured;
                      if(!captured)std::cout<<",\"validated_code_bytes\":"<<binary.metadata.codeBytes<<",\"footer_offset\":"<<binary.metadata.footer;
                      std::cout<<",\"immutable_token_snapshot\":"<<(immutable?"true":"false")<<",\"source_writer_coherence\":false,\"descriptor_binding\":false,\"gpu_execution\":false}\n";
                     }
                     if(packet.opcode==0x27 || packet.opcode==0x2d || packet.opcode==0x15){
                      auto execution_bank=std::make_unique<CtrPm4::Registers>();
                      const int fold=packet.opcode==0x15?prepare_dispatch_execution_bank(data,packet.words,*observed,*execution_bank):prepare_draw_execution_bank(data,packet.words,*observed,*execution_bank);
                      if(fold){++register_failures;}else observed.swap(execution_bank);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_lookahead_execution_inputs\",\"word\":"<<packet.word<<",\"opcode\":"<<packet.opcode<<",\"status\":"<<fold<<",\"metadata_only\":true,\"GPU_execution\":false}\n";
                     }
                    }
                    if(!kind && i==0 && first_draw){const auto line=graphics_log.str();HANDLE log=CreateFileW(L"runtime/user/logs/submit_flip_graphics.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD sent=0;
                     if(log==INVALID_HANDLE_VALUE || !WriteFile(log,line.data(),static_cast<DWORD>(line.size()),&sent,nullptr) || sent!=line.size()){if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);ExitProcess(4);}CloseHandle(log);}
                    std::ostringstream ordered_log;
                    if(pending_packet)for(const auto& pair:paired_stages)if(pair->word==pending_packet->word){
                     size_t differences=0;for(size_t reg=0;reg<0xD000;++reg)if(pair->registers->words[reg]!=ordered_registers.state().words[reg])++differences;
                     const int draw_identity=command_cursor.matches(pair->submission_token,pair->command_buffer_index,pair->command_buffer_kind,pair->word,pair->original_packet.data(),pair->original_packet.size());
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_draw_inputs\",\"word\":"<<pair->word<<",\"register_words\":53248,\"register_differences\":"<<differences<<",\"command_identity_status\":"<<draw_identity<<",\"key_status\":"<<pair->graphics_key_status<<",\"fetch_status\":"<<pair->vertex_fetch.status<<",\"format_status\":"<<pair->vertex_fetch.format_status<<",\"GPU_execution\":false}\n";
                    }
                    // Exercise the actual selected-device sampler lifecycle on its owner
                    // thread. No descriptor binding or in-flight GPU work exists here.
                    // Selected-session image/view lifecycle and synchronous linear transfers.
                    // Success is the C ABI image+memory+view contract; no raw view handle is exposed.
                    // Retain exact per-observation status, including unsupported formats.
                    unsigned image_attempts=0,image_created=0,image_released=0,image_missing=0,image_uploaded=0,image_tiled=0;
                    int image_status=88;
                    unsigned layout_created=0,layout_released=0,layout_missing=0,layout_fully_written=0;int layout_status=88;
                    unsigned sampler_attempts=0,sampler_created=0,sampler_released=0;int sampler_status=88;
                    unsigned sampler_peak=0;
                    unsigned buffer_attempts=0,buffer_created=0,buffer_released=0,buffer_peak=0;uint64_t buffer_compared=0;int buffer_status=88;
                    unsigned image_peak=0;
                    unsigned depth_attempts=0,depth_created=0,depth_released=0,depth_missing=0,depth_cleared=0;int depth_status=88;
                    const bool is_first_draw_buffer=!kind && i==0;
                    const ULONGLONG execution_started=GetTickCount64();
                    const size_t diagnostic_buffer_index=i;
                    if(graphics_session && !submission_owner.status)buffer_status=graphics_session->invoke([&](const CtrVideoSessionApiV1* api){
                     runtime_depth_residency.observe(api->context,[&](uint64_t token,uint64_t lease,uint32_t writer,uint64_t generation,const CtrDepthBackingSpansV1& spans,uint64_t begin,uint64_t bytes){
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_prior_depth_residency\",\"token\":"<<token<<",\"lease\":"<<lease<<",\"writer_word\":"<<writer<<",\"generation\":"<<generation<<",\"depth_address\":"<<spans.depth_address<<",\"depth_bytes\":"<<spans.depth_bytes<<",\"stencil_address\":"<<spans.stencil_address<<",\"stencil_bytes\":"<<spans.stencil_bytes<<",\"guard_begin\":"<<begin<<",\"guard_bytes\":"<<bytes<<",\"owner_thread\":true}\n";
                     });
                     for(const auto& stage:submission_owner.entries)for(const auto& d:stage.stage->descriptors){
                      if(d.kind!=1 || d.image_status || d.image_extent_status || !d.image_payload.capture_status)continue;
                      uint64_t image=0,generation=0,begin=0,bytes=0;
                      if(!runtime_gpu_resident_memory.identify(d.image_extent.guest_address,image,generation,begin,bytes))continue;
                      uint64_t lease=0,alias=0;std::shared_ptr<void> owner;
                      int resolved=runtime_depth_residency.resolve_sampled_interval(api->context,d.image_extent.guest_address,d.image_extent.bytes,generation,lease,owner);
                      if(!resolved && (lease!=image || !owner))resolved=112;
                      int aliased=resolved?resolved:ctr_video_depth_sampled_alias_v1(api->context,lease,&alias);
                      if(!aliased && !alias)aliased=112;
                      int released=alias?ctr_video_sampled_image_release_v1(api->context,alias):88;
                      int restored=alias?ctr_video_depth_attachment_restore_v1(api->context,lease):88;
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_prior_sampled_depth_alias_probe\",\"word\":"<<stage.word<<",\"sharp\":"<<d.sharp<<",\"generation\":"<<generation<<",\"address\":"<<d.image_extent.guest_address<<",\"bytes\":"<<d.image_extent.bytes<<",\"resolve_status\":"<<resolved<<",\"alias_status\":"<<aliased<<",\"release_status\":"<<released<<",\"restore_status\":"<<restored<<",\"GPU_draw\":false,\"CPU_snapshot\":false}\n";
                      if(alias && released)return released;if(alias && restored)return restored;
                     }
                     const auto preflight_vertex=[&](uint32_t word,GraphicsVertexFetchOwner& fetch){
                      fetch.physical_status=fetch.status?fetch.status:(fetch.format_status?fetch.format_status:ctr_video_graphics_vertex_inputs_preflight_v1(api->context,&fetch.value,fetch.formats.data(),fetch.value.count));
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_preflight\",\"word\":"<<word<<",\"status\":"<<fetch.physical_status<<",\"attribute_count\":"<<fetch.value.count<<",\"inputs\":"<<vertex_inputs_metadata(fetch)<<",\"selected_session\":true,\"GPU_execution\":false}\n";
                     };
                     for(auto& pair:paired_stages)preflight_vertex(pair->word,pair->vertex_fetch);
                     for(auto& vertex:vertex_only_stages)preflight_vertex(vertex->word,vertex->vertex_fetch);
                     const int rendering_authority=ctr_video_dynamic_rendering_authority_v1(api->context);
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_dynamic_rendering_authority\",\"status\":"<<rendering_authority<<",\"selected_session\":true,\"GPU_execution\":false}\n";
                     if(rendering_authority && rendering_authority!=88)return rendering_authority;
                     const int dynamic_authority=ctr_video_graphics_dynamic_authority_v1(api->context,3);
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_dynamic_authority\",\"required_mask\":3,\"status\":"<<dynamic_authority<<",\"GPU_execution\":false}\n";
                     if(dynamic_authority && dynamic_authority!=88)return dynamic_authority;
                     uint32_t core_available=0;for(uint32_t bit=1;bit<8192;bit<<=1){const int status=ctr_video_graphics_core_authority_v1(api->context,bit);if(status && status!=88)return status;if(!status)core_available|=bit;}
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_core_authority\",\"available_mask\":"<<core_available<<",\"queried_mask\":8191,\"GPU_execution\":false}\n";
                     VideoImageBatch images(api->context);std::ostringstream image_rows;std::vector<LiveGraphicsImage> live_images;live_images.reserve(256);
                     for(const auto& item:submission_owner.entries)for(const auto& descriptor:item.stage->descriptors){
                      if(descriptor.kind!=1 || descriptor.status || descriptor.image_status || descriptor.image_extent_status)continue;
                      ++image_attempts;uint64_t token=0;
                      const int created=images.create(descriptor.image,&token);
                      const bool compressed_blocks=descriptor.image.data_format==35 || descriptor.image.data_format==37;
                      int released=88,uploaded=88,readback=88,tiled_roundtrip=88;uint64_t compared=0,differences=0,padding_differences=0,readback_pixels=0,tiled_compared=0,tiled_differences=0;
                      if(!created){if(!token)return 112;++image_created;
                       // Distinct APIs: tile8 linear rows; tile13/14 whole-public detiling.
                       const uint64_t texel_bytes=descriptor.image.data_format==1?1u:((descriptor.image.data_format==2 || descriptor.image.data_format==3)?2u:4u);
                       // A tiled readback is never compared against tiled input bytes.
                       const bool exact_r16_tile0=descriptor.image.data_format==2 && descriptor.image.number_format==0 && descriptor.image.type==9 && descriptor.image.width==2048 && descriptor.image.height==8192 && descriptor.image.pitch==2048 && descriptor.image.samples==1 && descriptor.image.depth==1 && descriptor.image.layers==1 && descriptor.image.levels==1 && descriptor.image.tile_mode==0 && descriptor.image.swizzle[0]==4 && descriptor.image.swizzle[1]==4 && descriptor.image.swizzle[2]==4 && descriptor.image.swizzle[3]==4 && descriptor.image_extent.bytes==33554432;
                       if((exact_r16_tile0 || descriptor.image.tile_mode==8 || descriptor.image.tile_mode==13 || descriptor.image.tile_mode==14) && descriptor.image_payload.snapshot.size()==descriptor.image_extent.bytes && !descriptor.image_payload.capture_status){
                        uploaded=descriptor.image.tile_mode==8 ? ctr_video_sampled_image_upload_linear_v1(api->context,token,descriptor.image_payload.snapshot.data(),descriptor.image_payload.snapshot.size(),uint64_t(descriptor.image_extent.pitch)*texel_bytes) : ctr_video_sampled_image_upload_tiled_v1(api->context,token,descriptor.image_payload.snapshot.data(),descriptor.image_payload.snapshot.size(),&descriptor.image_extent);
                        if(!uploaded){if(descriptor.image.tile_mode==8)++image_uploaded;else ++image_tiled;
                         if(!compressed_blocks){std::vector<unsigned char> returned(descriptor.image_payload.snapshot.size(),0xa5);
                         const uint64_t pitch=uint64_t(descriptor.image_extent.pitch)*texel_bytes;
                         readback=ctr_video_sampled_image_readback_linear_v1(api->context,token,returned.data(),returned.size(),pitch);
                         if(!readback){
                          const uint64_t row=uint64_t(descriptor.image.width)*texel_bytes;
                          for(uint64_t offset=0;offset<returned.size();++offset){
                           const bool pixel=offset/pitch<descriptor.image.height && offset%pitch<row;
                           if(pixel){++readback_pixels;if(descriptor.image.tile_mode==8){++compared;if(returned[static_cast<size_t>(offset)]!=descriptor.image_payload.snapshot[static_cast<size_t>(offset)])++differences;}}
                           else if(returned[static_cast<size_t>(offset)]!=0xa5)++padding_differences;
                          }
                         }}
                        }
                       }
                       // BC1/BC3 use compressed blocks and concatenated padded mip/layer spans.
                       // The provider restores actual image blocks via GPU readback,
                       // retaining only padding from prior; never use RGBA row pitch.
                       if(!uploaded && (compressed_blocks || !readback) && (descriptor.image.tile_mode==13 || exact_r16_tile0)){
                        const auto& prior=descriptor.image_payload.snapshot;std::vector<unsigned char> tiled(prior.size(),0xa5);
                        tiled_roundtrip=ctr_video_image_readback_tiled_v1(api->context,token,prior.data(),prior.size(),&descriptor.image_extent,tiled.data());
                        if(!tiled_roundtrip){tiled_compared=prior.size();for(size_t i=0;i<prior.size();++i)if(prior[i]!=tiled[i])++tiled_differences;}
                        if(compressed_blocks)readback=tiled_roundtrip;
                        if(tiled_roundtrip || tiled_differences)return tiled_roundtrip?tiled_roundtrip:112;
                       }
                       released=0;image_peak=static_cast<unsigned>(images.size());if(!uploaded)live_images.push_back({item.stage.get(),descriptor.sharp,token});
                      }else if(created==88){if(token)return 112;++image_missing;}
                      image_rows<<"{\"scope\":\"original_GnmSubmitFlip_image_allocation_observation\",\"word\":"<<item.word<<",\"vertex\":"<<(item.vertex?"true":"false")<<",\"sharp_index\":"<<descriptor.sharp<<",\"create_status\":"<<created<<",\"owned_view_contract\":"<<(!created?"true":"false")<<",\"release_status\":"<<released<<",\"release_deferred_to_batch_end\":true,\"upload_status\":"<<uploaded<<",\"upload\":"<<(!uploaded?"true":"false")<<",\"readback_status\":"<<readback<<",\"compared_pixel_bytes\":"<<compared<<",\"readback_pixel_bytes\":"<<readback_pixels<<",\"source_tile_mode\":"<<descriptor.image.tile_mode<<",\"tiled_roundtrip_status\":"<<tiled_roundtrip<<",\"tiled_compared_bytes\":"<<tiled_compared<<",\"tiled_differences\":"<<tiled_differences<<",\"pixel_differences\":"<<differences<<",\"padding_differences\":"<<padding_differences<<",\"gpu_execution\":false}\n";
                      if(created && created!=88)return created;
                      if(!created && released)return released;
                      if(uploaded && uploaded!=88)return uploaded;
                      if(!uploaded && (readback || differences || padding_differences))return readback?readback:112;
                     }census_log<<image_rows.str();image_status=0;
                    // Real selected-device allocations from immutable CPU-owned snapshots.
                    // Host initialization comparison only, never GPU readback or binding.

                     VideoBufferBatch batch(api->context);VideoBufferBatch vertex_buffers(api->context);VideoSamplerBatch samplers(api->context);VideoColorBatch colors(api->context);VideoDepthBatch depths(api->context);VideoImageBatch depth_aliases(api->context);VideoLayoutBatch layouts(api->context);VideoLayoutBatch vertex_layouts(api->context);
                     std::vector<LiveVertexBuffer> live_vertices;live_vertices.reserve(75);uint64_t vertex_compared=0;
                     for(const auto& pair:paired_stages){const int retained=retain_vertex_buffers(api->context,vertex_buffers,pair->word,pair->vertex_fetch,live_vertices,census_log,vertex_compared);if(retained)return retained;}
                     for(const auto& vertex:vertex_only_stages){const int retained=retain_vertex_buffers(api->context,vertex_buffers,vertex->word,vertex->vertex_fetch,live_vertices,census_log,vertex_compared);if(retained)return retained;}
                     struct LiveDepth{uint32_t word;uint64_t token;bool initialized;};std::vector<LiveDepth> live_depths;std::ostringstream depth_rows;std::ostringstream buffer_rows,layout_rows;std::vector<LiveGraphicsBuffer> live_buffers;live_buffers.reserve(256);std::vector<LiveGraphicsSampler> live_samplers;live_samplers.reserve(256);
                     struct LiveIndex{uint32_t word,stride;uint64_t token,bytes;};std::vector<LiveIndex> live_indices;
                     const auto initialize=[&](uint32_t word,const void* source,uint64_t size,uint32_t usage,uint32_t sharp,const GraphicsStageOwner* stage,uint32_t type)->int{
                      ++buffer_attempts;uint64_t token=0;const int created=batch.create(source,size,usage,&token);
                      int copied=88;uint64_t differences=0;
                      if(!created){if(!token)return 112;++buffer_created;
                       try{std::vector<unsigned char> returned(static_cast<size_t>(size),0xa5);copied=ctr_video_buffer_copy_initial_v1(api->context,token,returned.data(),size);
                        if(!copied){const auto* original=static_cast<const unsigned char*>(source);for(size_t offset=0;offset<returned.size();++offset)if(returned[offset]!=original[offset])++differences;buffer_compared+=size;}}
                       catch(...){copied=112;}
                       buffer_peak=static_cast<unsigned>(batch.size());if(stage)live_buffers.push_back({stage,sharp,type,token,size});if(!stage && (usage==2 || usage==3))live_indices.push_back({word,usage==2?2u:4u,token,size});
                      }
                      buffer_rows<<"{\"scope\":\"original_GnmSubmitFlip_buffer_initialization\",\"word\":"<<word<<",\"sharp_index\":"<<sharp<<",\"usage\":"<<usage<<",\"bytes\":"<<size<<",\"create_status\":"<<created<<",\"copy_status\":"<<copied<<",\"release_status\":0,\"release_deferred_to_batch_end\":true"<<",\"differences\":"<<differences<<",\"host_initialization_only\":true,\"gpu_execution\":false}\n";
                      if(created)return created;if(copied)return copied;return differences?112:0;
                     };
                     for(const auto& item:submission_owner.entries){
                      for(const auto& buffer:item.stage->buffers){if(buffer.capture_status || buffer.snapshot.empty())continue;
                       const int status=initialize(item.word,buffer.snapshot.data(),buffer.snapshot.size(),1,buffer.sharp,item.stage.get(),0);if(status)return status;}
                      for(const auto& buffer:item.stage->flatbufs){if(buffer.status || buffer.words.empty())continue;
                       const int status=initialize(item.word,buffer.words.data(),buffer.words.size()*4,1,buffer.sharp,item.stage.get(),1);if(status)return status;}
                     }
                     for(const auto& index:submission_owner.indices){const auto& buffer=index.capture;if(buffer.capture_status || buffer.snapshot.empty())return 112;
                      if(buffer.stride!=2 && buffer.stride!=4)return 88;
                      const int status=initialize(index.word,buffer.snapshot.data(),buffer.snapshot.size(),buffer.stride==2?2u:3u,UINT32_MAX,nullptr,0);if(status)return status;}
                     for(const auto& item:submission_owner.entries)for(const auto& descriptor:item.stage->descriptors){
                      if(descriptor.kind!=2 || descriptor.status || descriptor.sampler_status)continue;
                      ++sampler_attempts;uint64_t token=0;const int created=samplers.create(descriptor.sampler,&token);
                      if(created)return created;if(!token)return 112;++sampler_created;sampler_peak=static_cast<unsigned>(samplers.size());live_samplers.push_back({item.stage.get(),descriptor.sharp,token});
                     }
                     DepthPublicationOwners pending_depth_guest_owners;
                     struct D16Publication {void* session;uint64_t token;CtrDepthD16LayoutV1 layout;};
                     std::vector<D16Publication> d16_publications;d16_publications.reserve(depth_allocations.size());
                     std::vector<std::pair<const DepthAllocationObservation*,uint64_t>> retained_depth_identities;
                     std::vector<uint64_t> cross_submission_depths;
                     for(const auto& item:depth_allocations){++depth_attempts;uint64_t token=0;uint32_t prior_depth_word=0;
                      for(const auto& previous:retained_depth_identities){const auto& prior=*previous.first;
                       if(!item.clear_flags && item.extent_valid && prior.extent_valid && item.identity==prior.identity && !std::memcmp(&item.parameters,&prior.parameters,sizeof(item.parameters))){token=previous.second;prior_depth_word=prior.word;break;}}
                      if(token){live_depths.push_back({item.word,token,false});census_log<<"{\"scope\":\"original_GnmSubmitFlip_depth_identity_reuse\",\"word\":"<<item.word<<",\"status\":0,\"source_word\":"<<prior_depth_word<<",\"same_retail_addresses_pitch_view\":true,\"preserved_GPU_generation\":true}\n";continue;}

                      const auto& ir=item.identity;const auto& ip=item.parameters;
                      CtrDepthMS2LayoutV1 retained_layout{1,sizeof(CtrDepthMS2LayoutV1),ip.width,ip.height,((ir[8]&2047)+1)*8,ip.samples,ip.z_format,ip.stencil_format,(ir[2]>>20)&7,ir[9]&0x3fffff,ir[10]&2047,(ir[10]>>13)&2047,0,0,uint64_t(ir[6])<<8};
                      uint64_t retained_lease=0;int reuse_status=62;
                      if(item.extent_valid && ip.samples==1 && ip.z_format==3 && ip.stencil_format==1)reuse_status=runtime_depth_residency.resolve_attachment(api->context,retained_layout,uint64_t(ir[7])<<8,retained_lease);
                      if(reuse_status!=0 && reuse_status!=62)return reuse_status;
                      const bool reused=!reuse_status;
                      if(reused)census_log<<"{\"scope\":\"exact_retained_depth_attachment_reuse\",\"word\":"<<item.word<<",\"lease\":"<<retained_lease<<",\"clear_flags\":"<<item.clear_flags<<",\"status\":0,\"CPU_snapshot_uploaded\":false}\n";
                      const int created=reused?depths.retain(retained_lease,&token):(item.extent_valid?depths.create(item.parameters,&token):88);int released=88,cleared=88;
                      if(!created){if(!token)return 112;++depth_created;retained_depth_identities.push_back({&item,token});if(reused)cross_submission_depths.push_back(token);
                       // Public shadPS4 ImageInfo(DepthBuffer) keeps default alt_tile/bank_swizzle=0.
                       // Capture the exact write backing BEFORE any standalone GPU initialization.
                       const auto& r=item.identity;const auto& p=item.parameters;
                       CtrDepthD16LayoutV1 layout{1,sizeof(CtrDepthD16LayoutV1),p.width,p.height,((r[8]&2047)+1)*8,p.samples,p.z_format,p.stencil_format,(r[2]>>20)&7,r[9]&0x3fffff,r[10]&2047,(r[10]>>13)&2047,0,0,uint64_t(r[6])<<8};
                       CtrGraphicsImageExtentV1 extent{};const int extent_status=ctr_video_depth_d16_extent_v1(&layout,&extent);int enrolled=88;
                       if(!extent_status){d16_publications.push_back({api->context,token,layout});
                        const auto download=+[](void* raw,uint32_t plane,const unsigned char* prior,uint64_t bytes,unsigned char* output,uint64_t capacity)->int{
                         auto& d=*static_cast<D16Publication*>(raw);if(plane || bytes!=capacity)return 60;
                         return ctr_video_depth_d16_read_tiled_v1(d.session,d.token,&d.layout,prior,bytes,output);
                        };
                        enrolled=pending_depth_guest_owners.enroll(memory,token,&extent.guest_address,&extent.bytes,1,download,&d16_publications.back());
                        if(enrolled){census_log<<"{\"scope\":\"D16_pre_writer_capture_failure\",\"word\":"<<item.word<<",\"status\":"<<enrolled<<",\"address\":"<<extent.guest_address<<",\"bytes\":"<<extent.bytes<<",\"before_GPU_writer\":true}\n";return enrolled;}
                       }else if(extent_status!=88)return extent_status;
                       if(!reused && extent_status==88 && p.samples==1 && p.z_format==3 && p.stencil_format==1){
                        CtrDepthBackingSpansV1 spans{};
                        const int span_status=ctr_video_depth_single_spans_v1(&layout,uint64_t(r[7])<<8,&spans);int capture=88;
                        if(!span_status){const uint64_t addresses[]={spans.depth_address,spans.stencil_address},lengths[]={spans.depth_bytes,spans.stencil_bytes};
                         capture=pending_depth_guest_owners.enroll(memory,token,addresses,lengths,2,nullptr,nullptr);if(capture){census_log<<"{\"scope\":\"D32_single_pre_writer_capture_failure\",\"word\":"<<item.word<<",\"status\":"<<capture<<",\"depth_address\":"<<spans.depth_address<<",\"depth_bytes\":"<<spans.depth_bytes<<",\"stencil_address\":"<<spans.stencil_address<<",\"stencil_bytes\":"<<spans.stencil_bytes<<",\"before_GPU_writer\":true}\n";return capture;}
                        }else if(span_status!=88)return span_status;
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_D32_single_pre_writer_capture\",\"word\":"<<item.word<<",\"span_status\":"<<span_status<<",\"capture_status\":"<<capture<<",\"depth_bytes\":"<<spans.depth_bytes<<",\"stencil_bytes\":"<<spans.stencil_bytes<<",\"before_GPU_clear\":true,\"pixel_export_provider\":false,\"guest_writeback\":false}\n";
                       }
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_D16_pre_writer_capture\",\"word\":"<<item.word<<",\"token\":"<<token<<",\"extent_status\":"<<extent_status<<",\"capture_status\":"<<enrolled<<",\"bytes\":"<<extent.bytes<<",\"before_GPU_clear\":true,\"guest_writeback\":false}\n";
                       // Standalone full-aspect initialization using the exact observed values.
                       // Does not consume this guest command, update HTILE, or admit LOAD.
                       if(!reused && (item.clear_flags&1) && (!item.parameters.stencil_format || (item.clear_flags&2))){cleared=ctr_video_depth_attachment_clear_v1(api->context,token,item.depth_bits,item.stencil);if(!cleared)++depth_cleared;}
                       released=0;live_depths.push_back({item.word,token,!cleared});}
                      else if(created==88){if(token)return 112;++depth_missing;}
                      depth_rows<<"{\"scope\":\"original_GnmSubmitFlip_depth_allocation\",\"word\":"<<item.word<<",\"width\":"<<item.parameters.width<<",\"height\":"<<item.parameters.height<<",\"samples\":"<<item.parameters.samples<<",\"z_format\":"<<item.parameters.z_format<<",\"stencil_format\":"<<item.parameters.stencil_format<<",\"extent_hint_valid\":"<<(item.extent_valid?"true":"false")<<",\"create_status\":"<<created<<",\"release_deferred_to_batch_end\":true,\"release_status\":"<<released<<",\"clear_status\":"<<cleared<<",\"clear_float_bits\":"<<item.depth_bits<<",\"stencil_clear\":"<<item.stencil<<",\"standalone_initialization\":true,\"undefined_contents\":"<<(cleared?"true":"false")<<",\"gpu_execution\":false}\n";
                      if(created && created!=88)return created;if(!created && released)return released;if(cleared && cleared!=88)return cleared;
                     }
                     depth_status=0;
                     struct LiveDepthSample {const GraphicsStageOwner* stage;uint32_t sharp,consumer;uint64_t image,depth;};
                     std::vector<LiveDepthSample> live_depth_samples;
                     // Only this submit owns the allocation tokens; callbacks cannot outlive it.
                     auto submission_depth_tokens=std::make_shared<std::vector<uint64_t>>();
                     for(const auto& retained:retained_depth_identities)submission_depth_tokens->push_back(retained.second);
                     // A depth-only sampled view retains the real attachment owner.
                     // Match the complete bounded retail shape, never upload its stale snapshot.
                     for(const auto& item:submission_owner.entries)for(auto& d:item.stage->descriptors){
                      const auto& p=d.image;const bool prior_tall_d16=DepthResidency::tall_d16_view(p,d.image_extent);
                      const bool prior_ms2=p.type==14 && p.samples==2 && p.data_format==4 && p.number_format==7 && p.tile_mode==1 && p.layers==1 && p.levels==1 && !p.base_level && !p.base_array && !p.last_array && !p.compression && !p.alt_tile;
                      if(item.word<2336 || d.kind!=1 || d.status || d.image_status || d.image_extent_status || (!prior_tall_d16 && !prior_ms2))continue;
                      uint64_t owner=0;for(const auto& retained:retained_depth_identities){const auto& a=*retained.first;
                       // Existing retained-depth compute route uses the same public raw registers.
                       if(a.word!=680 || !a.extent_valid || p.width!=a.parameters.width || p.height!=a.parameters.height)continue;
                       for(const auto& vertex:vertex_only_stages)if(vertex->word==680){const auto& bank=vertex->registers->words;
                        const uint64_t address=uint64_t(bank[0xa014])<<8,bytes=(uint64_t(bank[0xa017]&0x3fffffu)+1)*64*4*2;
                        if(p.pitch==(uint64_t(bank[0xa016]&2047u)+1)*8 && p.guest_address==address && d.image_extent.guest_address==address && d.image_extent.bytes==bytes){if(owner && owner!=retained.second)return 112;owner=retained.second;}
                       }
                      }
                      uint64_t generation=0,guard_image=0,guard_begin=0,guard_bytes=0;std::shared_ptr<void> resident_owner;
                      if(!owner && runtime_gpu_resident_memory.identify(p.guest_address,guard_image,generation,guard_begin,guard_bytes)){
                       const int resolved=runtime_depth_residency.resolve_sampled_view(api->context,p,d.image_extent,generation,owner,resident_owner);
                       if(resolved){if(resolved!=88)return resolved;continue;}if(owner!=guard_image || !resident_owner)return 112;
                      }
                      if(!owner)continue;uint64_t token=0;const int aliased=ctr_video_depth_sampled_alias_v1(api->context,owner,&token);if(aliased)return aliased;
                      const int owned=depth_aliases.adopt(token);if(owned){ctr_video_sampled_image_release_v1(api->context,token);return owned;}
                      if(resident_owner){
                       d.resident_image_authority=[context=api->context,p,extent=d.image_extent,generation,owner,resident_owner](){
                        uint64_t current=0;std::shared_ptr<void> retained;const int status=runtime_depth_residency.resolve_sampled_view(context,p,extent,generation,current,retained);
                        return status?status:(current==owner && retained==resident_owner?0:112);
                       };
                      }else{
                       std::weak_ptr<std::vector<uint64_t>> lifetime=submission_depth_tokens;
                       d.submission_image_authority=[lifetime,owner](){const auto tokens=lifetime.lock();return tokens && std::count(tokens->begin(),tokens->end(),owner)==1?0:88;};
                      }
                      live_images.push_back({item.stage.get(),d.sharp,token});live_depth_samples.push_back({item.stage.get(),d.sharp,item.word,token,owner});
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_alias_prepared\",\"word\":"<<item.word<<",\"sharp\":"<<d.sharp<<",\"status\":0,\"same_retained_GPU_owner\":true,\"CPU_snapshot_uploaded\":false,\"GPU_execution\":false}\n";
                     }
                     struct AttachmentJoin {const GraphicsPairOwner* pair;std::array<uint64_t,8> colors{};uint64_t depth=0,layout=0;bool depth_initialized=false;int color_status=88;bool descriptors_complete=false;};
                     std::vector<AttachmentJoin> attachment_joins;attachment_joins.reserve(paired_stages.size());
                     std::vector<uint64_t> initialized_colors;
                     ColorSampledCoherence color_coherence;
                     for(const auto& pair:paired_stages){uint64_t token=0;
                      AttachmentJoin attachments{pair.get()};
                      for(const auto& d:live_depths)if(d.word==pair->word){if(attachments.depth)return 112;attachments.depth=d.token;attachments.depth_initialized=d.initialized;}
                      std::vector<const GraphicsStageOwner::Color*> color_inputs;
                      const int color_status=graphics_pair_color_inputs(*pair,color_inputs);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_pair_color_inputs\",\"word\":"<<pair->word<<",\"status\":"<<color_status<<",\"mrt_mask\":"<<pair->fragment->resources.mrt_mask<<",\"owned_colors\":"<<color_inputs.size()<<",\"GPU_execution\":false}\n";
                      attachments.color_status=color_status;
                      if(color_status && color_status!=88)return color_status;
                      if(!color_status)for(const auto* color:color_inputs){uint64_t color_token=0;bool reused_color=false;
                       const int allocated=colors.resolve(color->requirement,&color_token,&reused_color);
                       int uploaded=88,readback=88;uint64_t readback_bytes=0;size_t padding_diff=0;
                       const bool initialized_reuse=!allocated && std::find(initialized_colors.begin(),initialized_colors.end(),color_token)!=initialized_colors.end();
                       if(initialized_reuse){uploaded=0;}
                       else if(!allocated){const auto& r=color->requirement;const uint32_t texel_bytes=r.data_format==3?2u:((r.data_format==10 || r.data_format==4)?4u:0u);const uint64_t row_bytes=uint64_t(r.pitch)*texel_bytes;
                        // Exact backing extent transport; the data-owned public detiler owns tile interpretation.
                        if(row_bytes && r.bytes%row_bytes==0 && r.bytes/row_bytes<=UINT32_MAX && color->initial.snapshot.size()==r.bytes){
                         CtrGraphicsImageExtentV1 extent{1,sizeof(CtrGraphicsImageExtentV1),r.pitch,static_cast<uint32_t>(r.bytes/row_bytes),r.guest_address,r.bytes};
                         uploaded=r.data_format==4?initialize_float_color(api->context,color_token,r,color->initial.snapshot.data(),r.bytes):ctr_video_color_attachment_upload_tiled_v1(api->context,color_token,color->initial.snapshot.data(),r.bytes,&extent);
                         if(!uploaded){if(color_coherence.add(color_token) || color_coherence.begin(color_token,pair->word) || color_coherence.complete(color_token,pair->word))return 112;initialized_colors.push_back(color_token);std::vector<unsigned char> returned(static_cast<size_t>(r.bytes),0xa5);
                          readback=ctr_video_color_attachment_readback_linear_v1(api->context,color_token,returned.data(),r.bytes,static_cast<uint32_t>(row_bytes));
                          if(!readback){readback_bytes=row_bytes*r.height;for(size_t at=static_cast<size_t>(readback_bytes);at<returned.size();++at)padding_diff+=returned[at]!=0xa5;if(padding_diff)return 112;}
                         }
                        }
                       }
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_initialization\",\"word\":"<<pair->word<<",\"slot\":"<<color->slot<<",\"upload_status\":"<<uploaded<<",\"initialization_reused\":"<<(initialized_reuse?"true":"false")<<",\"readback_status\":"<<readback<<",\"readback_pixel_bytes\":"<<readback_bytes<<",\"padding_differences\":"<<padding_diff<<",\"GPU_execution\":false}\n";
                       if(uploaded && uploaded!=88)return uploaded;if(readback && readback!=88)return readback;
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_allocation\",\"word\":"<<pair->word<<",\"slot\":"<<color->slot<<",\"status\":"<<allocated<<",\"pitch\":"<<color->requirement.pitch<<",\"height\":"<<color->requirement.height<<",\"samples\":"<<color->requirement.samples<<",\"live_token\":"<<(color_token?"true":"false")<<",\"reused_backing\":"<<(reused_color?"true":"false")<<",\"undefined_contents\":"<<(uploaded?"true":"false")<<",\"requires_blend_capability\":true,\"GPU_execution\":false}\n";
                       if(!allocated && !uploaded){if(attachments.colors[color->slot])return 112;attachments.colors[color->slot]=color_token;}else attachments.color_status=allocated?allocated:uploaded;
                       if(allocated && allocated!=88)return allocated;
                      }
                      std::vector<GraphicsBufferWrite> writes;const int prepared=prepare_graphics_buffer_writes(*pair,live_buffers,writes);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_buffer_write_inputs\",\"word\":"<<pair->word<<",\"status\":"<<prepared<<",\"writes\":[";
                      for(size_t at=0;at<writes.size();++at){const auto& write=writes[at];if(at)census_log<<',';
                       census_log<<"{\"binding\":"<<write.binding<<",\"buffer_slot\":"<<write.slot<<",\"vertex\":"<<(write.vertex?"true":"false")<<",\"sharp_index\":"<<write.sharp<<",\"buffer_type\":"<<write.type<<",\"bytes\":"<<write.bytes<<",\"null_clip_planes\":"<<(write.null_clip_planes?"true":"false")<<",\"live_token\":"<<(write.token?"true":"false")<<'}';}
                      census_log<<"],\"descriptor_writes\":false,\"gpu_execution\":false}\n";
                      if(prepared && prepared!=88)return prepared;
                      const int created=layouts.create(pair->fragment_bindings,pair->vertex_bindings,&token);int released=88;
                      if(!created){if(!token)return 112;++layout_created;released=0;}
                      else if(created==88){if(token)return 112;++layout_missing;}
                      int written=88;std::vector<CtrGraphicsBufferWriteV1> actual_writes;
                      if(!created && !prepared){actual_writes.reserve(writes.size());for(const auto& write:writes)actual_writes.push_back({write.binding,write.slot,1u,write.null_clip_planes?1u:0u,write.token});
                       written=ctr_video_graphics_layout_buffers_v1(api->context,token,actual_writes.data(),static_cast<uint32_t>(actual_writes.size()));}
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_buffer_descriptor_update\",\"word\":"<<pair->word<<",\"prepare_status\":"<<prepared<<",\"layout_status\":"<<created<<",\"write_status\":"<<written<<",\"buffer_binding_count\":"<<actual_writes.size()<<",\"live_buffer_tokens\":"<<batch.size()<<",\"live_layout_tokens\":"<<layouts.size()<<",\"images_samplers_written\":false,\"gpu_execution\":false}\n";
                      if(written && written!=88)return written;
                      std::vector<GraphicsSamplerWrite> sampler_writes;const int sampler_prepared=prepare_graphics_sampler_writes(*pair,live_samplers,sampler_writes);
                      std::vector<uint32_t> sampler_bindings;std::vector<uint64_t> sampler_tokens;
                      for(const auto& write:sampler_writes){sampler_bindings.push_back(write.binding);sampler_tokens.push_back(write.token);}
                      int sampler_written=88;if(!created && !sampler_prepared)sampler_written=ctr_video_graphics_layout_samplers_v1(api->context,token,sampler_bindings.data(),sampler_tokens.data(),static_cast<uint32_t>(sampler_tokens.size()));
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_sampler_descriptor_update\",\"word\":"<<pair->word<<",\"prepare_status\":"<<sampler_prepared<<",\"layout_status\":"<<created<<",\"write_status\":"<<sampler_written<<",\"sampler_binding_count\":"<<sampler_tokens.size()<<",\"live_sampler_tokens\":"<<samplers.size()<<",\"writes\":[";
                      for(size_t at=0;at<sampler_writes.size();++at){const auto& write=sampler_writes[at];if(at)census_log<<',';census_log<<"{\"binding\":"<<write.binding<<",\"sharp_index\":"<<write.sharp<<",\"vertex\":"<<(write.vertex?"true":"false")<<",\"live_token\":"<<(write.token?"true":"false")<<'}';}
                      census_log<<"],\"images_written\":false,\"gpu_execution\":false}\n";
                      if(sampler_prepared && sampler_prepared!=88)return sampler_prepared;if(sampler_written && sampler_written!=88)return sampler_written;
                      std::vector<GraphicsImageWrite> image_writes;const int image_prepared=prepare_graphics_image_writes(*pair,live_images,image_writes);
                      std::vector<uint32_t> image_bindings,image_nulls;std::vector<uint64_t> image_tokens;
                      for(const auto& write:image_writes){image_bindings.push_back(write.binding);image_tokens.push_back(write.token);image_nulls.push_back(write.semantic_null?1u:0u);}
                      int image_written=88;if(!created && !image_prepared)image_written=ctr_video_graphics_layout_images_null_v1(api->context,token,image_bindings.data(),image_tokens.data(),image_nulls.data(),static_cast<uint32_t>(image_tokens.size()));
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_image_descriptor_update\",\"word\":"<<pair->word<<",\"prepare_status\":"<<image_prepared<<",\"layout_status\":"<<created<<",\"write_status\":"<<image_written<<",\"image_binding_count\":"<<image_tokens.size()<<",\"live_image_tokens\":"<<images.size()<<",\"writes\":[";
                      for(size_t at=0;at<image_writes.size();++at){const auto& write=image_writes[at];if(at)census_log<<',';census_log<<"{\"binding\":"<<write.binding<<",\"sharp_index\":"<<write.sharp<<",\"vertex\":"<<(write.vertex?"true":"false")<<",\"live_token\":"<<(write.token?"true":"false")<<",\"semantic_null\":"<<(write.semantic_null?"true":"false")<<'}';}
                      census_log<<"],\"pipeline_created\":false,\"gpu_execution\":false}\n";
                      if(image_prepared && image_prepared!=88)return image_prepared;if(image_written && image_written!=88)return image_written;
                      attachments.layout=token;attachment_joins.push_back(attachments);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_attachment_join\",\"word\":"<<pair->word<<",\"submission_token\":"<<pair->submission_token<<",\"color_status\":"<<attachments.color_status<<",\"color_tokens\":"<<std::count_if(attachments.colors.begin(),attachments.colors.end(),[](uint64_t t){return t!=0;})<<",\"depth_token\":"<<(attachments.depth?"true":"false")<<",\"depth_standalone_initialized\":"<<(attachments.depth_initialized?"true":"false")<<",\"layout_token\":"<<(token?"true":"false")<<",\"dynamic_rendering_authority\":"<<rendering_authority<<",\"live_depths\":"<<depths.size()<<",\"GPU_execution\":false}\n";
                      const bool fully_written=!written && !sampler_written && !image_written && !graphics_input_capture_status(*pair->fragment_inputs) && !graphics_input_capture_status(*pair->vertex_inputs);attachment_joins.back().descriptors_complete=fully_written;if(fully_written)++layout_fully_written;
                      layout_rows<<"{\"scope\":\"original_GnmSubmitFlip_paired_layout\",\"word\":"<<pair->word<<",\"create_status\":"<<created<<",\"release_status\":"<<released<<",\"live_buffer_tokens\":"<<batch.size()<<",\"release_deferred_to_batch_end\":true,\"descriptor_set_written\":"<<(fully_written?"true":"false")<<",\"pipeline_created\":false,\"gpu_execution\":false}\n";
                      if(created && created!=88)return created;if(!created && released)return released;
                     }
                     struct VertexAttachmentJoin {const GraphicsVertexOnlyOwner* vertex;uint64_t layout=0,depth=0;bool initialized=false,descriptors_complete=false;};
                     std::vector<VertexAttachmentJoin> vertex_attachment_joins;
                     for(const auto& vertex:vertex_only_stages){uint64_t vertex_token=0;std::vector<CtrGraphicsBufferWriteV1> writes;
                      const int prepared=graphics_vertex_only_null_writes(*vertex,writes);
                      const int created=prepared?prepared:vertex_layouts.create_vertex(vertex->bindings,&vertex_token);
                      int buffers=88,sampler=88,image=88;
                      if(!created){buffers=ctr_video_graphics_layout_buffers_v1(api->context,vertex_token,writes.data(),static_cast<uint32_t>(writes.size()));
                       if(!buffers){sampler=ctr_video_graphics_layout_samplers_v1(api->context,vertex_token,nullptr,nullptr,0);image=ctr_video_graphics_layout_images_v1(api->context,vertex_token,nullptr,nullptr,0);}}
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_layout\",\"word\":"<<vertex->word<<",\"prepare_status\":"<<prepared<<",\"create_status\":"<<created<<",\"buffer_write_status\":"<<buffers<<",\"sampler_write_status\":"<<sampler<<",\"image_write_status\":"<<image<<",\"descriptor_set_written\":"<<(!created && !buffers && !sampler && !image?"true":"false")<<",\"fragment_present\":false,\"GPU_execution\":false}\n";
                      VertexAttachmentJoin joined{vertex.get(),vertex_token};
                      for(const auto& d:live_depths)if(d.word==vertex->word){if(joined.depth)return 112;joined.depth=d.token;joined.initialized=d.initialized;}
                      joined.descriptors_complete=!created && !buffers && !sampler && !image;vertex_attachment_joins.push_back(joined);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_attachment_join\",\"word\":"<<vertex->word<<",\"submission_token\":"<<vertex->submission<<",\"layout_token\":"<<(joined.layout?"true":"false")<<",\"depth_token\":"<<(joined.depth?"true":"false")<<",\"depth_standalone_initialized\":"<<(joined.initialized?"true":"false")<<",\"fragment_present\":false,\"GPU_execution\":false}\n";
                      for(int result:{prepared,created,buffers,sampler,image})if(result && result!=88)return result;
                     }
                     struct InitialColorSample {const GraphicsStageOwner* stage;uint32_t sharp;uint64_t address;std::vector<unsigned char> bytes;};
                     std::vector<InitialColorSample> initial_color_samples;
                     // Standalone initial-content projection only. No original draw has
                     // completed; this is never a command-cursor completion or draw admission.
                     for(const auto& attachment:attachment_joins){std::vector<const GraphicsStageOwner::Color*> alias_colors;
                      if(graphics_pair_color_inputs(*attachment.pair,alias_colors))continue;
                      for(const auto* color_ptr:alias_colors){const auto& color=*color_ptr;
                      const auto color_token=attachment.colors[color.slot];if(!color_token)continue;
                      for(const auto& image:live_images)for(const auto& descriptor:image.stage->descriptors){
                       if(descriptor.kind!=1 || descriptor.sharp!=image.sharp || descriptor.image.guest_address!=color.requirement.guest_address)continue;
                       const int converted=color_coherence.synchronize(color_token,image.token,[&](uint64_t c,uint64_t t){return transfer_color_sampled_view(api->context,c,t,color.requirement,descriptor.image);});
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_initial_color_to_sampled\",\"word\":"<<attachment.pair->word<<",\"sharp_index\":"<<image.sharp<<",\"status\":"<<converted<<",\"standalone_initial_contents\":true,\"game_writer_completed\":false,\"GPU_execution\":false}\n";
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_sampled_identity\",\"word\":"<<attachment.pair->word<<",\"image_sharp\":"<<image.sharp<<",\"color_format\":"<<color.requirement.data_format<<",\"image_format\":"<<descriptor.image.data_format<<",\"color_number_format\":"<<color.requirement.number_format<<",\"image_number_format\":"<<descriptor.image.number_format<<",\"color_pitch\":"<<color.requirement.pitch<<",\"image_pitch\":"<<descriptor.image.pitch<<",\"color_height\":"<<color.requirement.height<<",\"image_height\":"<<descriptor.image.height<<",\"color_tile\":"<<color.requirement.tile_mode<<",\"image_tile\":"<<descriptor.image.tile_mode<<",\"image_width\":"<<descriptor.image.width<<",\"status\":"<<converted<<",\"GPU_execution\":false}\n";
                       if(!converted){uint64_t compared=0,differences=0,padding=0;const int checked=compare_color_sampled_view(api->context,color_token,image.token,color.requirement,descriptor.image,compared,differences,padding);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_sampled_readback\",\"word\":"<<attachment.pair->word<<",\"sharp_index\":"<<image.sharp<<",\"status\":"<<checked<<",\"compared_pixel_bytes\":"<<compared<<",\"pixel_differences\":"<<differences<<",\"padding_differences\":"<<padding<<",\"game_writer_completed\":false,\"GPU_execution\":false}\n";
                        if(checked)return checked;
                        if(descriptor.image_extent.bytes==color.requirement.bytes && color.initial.snapshot.size()==color.requirement.bytes)
                         initial_color_samples.push_back({image.stage,image.sharp,color.requirement.guest_address,color.initial.snapshot});}
                       if(converted && converted!=88 && converted!=91)return converted;
                      }
                     }
                     }
                     for(const auto& index:live_indices){unsigned joined=0;uint64_t layout=0;
                      for(const auto& draw:attachment_joins)if(draw.pair->word==index.word){++joined;layout=draw.layout;}
                      for(const auto& draw:vertex_attachment_joins)if(draw.vertex->word==index.word){++joined;layout=draw.layout;}
                      if(joined!=1 || !layout)return 112;
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_index_layout_join\",\"word\":"<<index.word<<",\"stride\":"<<index.stride<<",\"bytes\":"<<index.bytes<<",\"index_token\":true,\"layout_token\":true,\"unique_join\":true,\"GPU_execution\":false}\n";
                     }
                     std::vector<DrawResourceInputs> prepared_draw_resources;prepared_draw_resources.reserve(43);
                     const auto prepare_resources=[&](uint32_t word,const DrawPacketInputs& packet,int packet_status,const CtrGraphicsKeyV1& key,int key_status,const GraphicsVertexFetchOwner& fetch,uint64_t layout,bool complete,const std::array<uint64_t,8>& color_tokens,uint64_t depth,const CtrPm4::Registers& bank){
                      uint64_t index_token=0;for(const auto& index:live_indices)if(index.word==word){if(index_token)return 60;index_token=index.token;}
                      CtrGraphicsDynamicStateV1 dynamic{};const int dynamic_status=decode_owned_dynamic_state(bank,packet.indexed,dynamic);
                      DrawResourceInputs prepared;const int status=dynamic_status?dynamic_status:(packet_status?packet_status:(key_status?key_status:prepare_draw_resources(word,packet,key,fetch,layout,complete,color_tokens,depth,index_token,live_vertices,prepared,&dynamic)));
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_draw_resource_inputs\",\"word\":"<<word<<",\"status\":"<<status<<",\"vertices\":"<<prepared.vertices.size()<<",\"descriptor_complete\":"<<(complete?"true":"false")<<",\"indexed\":"<<(packet.indexed?"true":"false")<<",\"GPU_execution\":false}\n";
                      if(!status)prepared_draw_resources.push_back(std::move(prepared));return status==88?0:status;
                     };
                     for(const auto& draw:attachment_joins){const auto& pair=*draw.pair;const int prepared=prepare_resources(pair.word,pair.draw_inputs,pair.draw_inputs_status,pair.graphics_key,pair.graphics_key_status,pair.vertex_fetch,draw.layout,draw.descriptors_complete,draw.colors,draw.depth,*pair.registers);if(prepared)return prepared;}
                     for(const auto& draw:vertex_attachment_joins){const auto& vertex=*draw.vertex;const std::array<uint64_t,8> no_colors{};const int prepared=prepare_resources(vertex.word,vertex.draw_inputs,vertex.draw_inputs_status,vertex.graphics_key,vertex.graphics_key_status,vertex.vertex_fetch,draw.layout,draw.descriptors_complete,no_colors,draw.depth,*vertex.registers);if(prepared)return prepared;}
                     // Consume only the exact current route. Each complete draw commits its
                     // register effects after the real GPU fence; no lookahead is authority.
                     uint32_t first_draw_word=UINT32_MAX;uint64_t first_pipeline=0;CtrGraphicsViewportStateV1 first_viewports{};int first_viewport_status=88;CtrGraphicsDynamicStateV1 first_dynamic{};int first_dynamic_status=88;int first_dynamic_preflight_status=88;CtrGraphicsDrawParametersV1 first_draw_parameters{};int first_draw_parameter_status=88;GraphicsPushResources first_push_resources{};int first_push_status=88;CtrGraphicsPushDataV1 first_push_data{};int first_push_data_status=88;
                     struct PipelineCleanup {void* context;uint64_t& token;~PipelineCleanup(){if(token)ctr_video_graphics_pipeline_release_v1(context,token);}} pipeline_cleanup{api->context,first_pipeline};
                     std::vector<std::pair<uint64_t,uint64_t>> completed_color_ranges;std::vector<std::pair<uint64_t,uint64_t>> pending_color_guest_ranges;completed_color_ranges.reserve(attachment_joins.size());
                     for(const auto& sample:live_depth_samples){const int restored=ctr_video_depth_attachment_restore_v1(api->context,sample.depth);if(restored)return restored;}
                     std::vector<std::pair<uint32_t,GnmEopTransaction>> ordered_eops;ordered_eops.reserve(58);
                     std::cout<<"{\"scope\":\"original_GnmSubmitFlip_resource_preparation_timing\",\"submission\":"<<submission_token<<",\"buffer\":"<<diagnostic_buffer_index<<",\"ccb\":"<<(kind?"true":"false")<<",\"elapsed_since_dispatch_ms\":"<<(GetTickCount64()-execution_started)<<",\"includes_owner_queue_wait\":true,\"before_ordered_GPU_commands\":true}\n";
                     const auto acquire_packet=[&](const CtrPm4::Packet& packet,const uint32_t* data){
                      const auto provider=+[](void* context,const GnmAcquireMemRequest& request)->int{
                       if(!acquire_queue_visibility_supported(request))return 88;
                       return ctr_video_queue_memory_visibility_v1(context);
                      };
                      const int result=complete_acquire_mem(data,packet.words,provider,api->context);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_acquire_mem_GPU_visibility\",\"word\":"<<packet.word<<",\"status\":"<<result<<",\"real_fence\":"<<(!result?"true":"false")<<",\"queue_wide_memory_dependency\":"<<(!result?"true":"false")<<",\"guest_writeback\":false,\"GPU_execution\":"<<(!result?"true":"false")<<"}\n";return result;
                     };

                     const auto ordered_transport=[&](const CtrPm4::Packet& packet,const uint32_t* data)->int{
                      if(packet.opcode==0x47){
                       CtrPm4::Eop decoded{};const int decode_eop=CtrPm4::DecodeEop(data,packet.words,decoded);if(decode_eop)return decode_eop;
                       // Only the observed public CACHE_FLUSH_AND_INV_TS_EVENT /
                       // end-of-pipe index5 is admitted; other events stay explicit.
                       if(data[1]!=0x504u)return 88;
                       GnmEopTransaction* transaction=nullptr;for(auto& item:ordered_eops)if(item.first==packet.word)transaction=&item.second;
                       if(!transaction){ordered_eops.emplace_back(static_cast<uint32_t>(packet.word),GnmEopTransaction{});transaction=&ordered_eops.back().second;}
                       int residency_status=0;
                       pending_depth_guest_owners.observe([&](uint64_t token,uint32_t writer,bool dirty,uint32_t planes,bool,bool){
                        if(!dirty || residency_status)return;
                        const DepthAllocationObservation* allocation=nullptr;for(const auto& retained:retained_depth_identities)if(retained.second==token)allocation=retained.first;
                        if(!allocation){residency_status=88;return;}const auto& r=allocation->identity;const auto& p=allocation->parameters;
                        if(planes && (p.samples!=1 || p.z_format!=3 || p.stencil_format!=1))return;
                        if(planes){residency_status=pending_depth_guest_owners.verify_pre_writer(token,memory);if(residency_status)return;}
                        CtrDepthMS2LayoutV1 layout{1,sizeof(CtrDepthMS2LayoutV1),p.width,p.height,((r[8]&2047)+1)*8,p.samples,p.z_format,p.stencil_format,(r[2]>>20)&7,r[9]&0x3fffff,r[10]&2047,(r[10]>>13)&2047,0,0,uint64_t(r[6])<<8};
                        census_log<<"{\"scope\":\"original_depth_retention_layout\",\"word\":"<<packet.word<<",\"allocation_word\":"<<allocation->word<<",\"writer\":"<<writer<<",\"pitch\":"<<layout.pitch<<",\"tile_mode\":"<<layout.tile_mode<<",\"slice_tile_max\":"<<layout.slice_tile_max<<",\"slice_start\":"<<layout.slice_start<<",\"slice_max\":"<<layout.slice_max<<",\"alt_tile\":"<<layout.alt_tile<<",\"bank_swizzle\":"<<layout.bank_swizzle<<"}\n";
                        residency_status=runtime_depth_residency.retain(api,graphics_session,token,writer,layout,uint64_t(r[7])<<8);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_depth_retain_attempt\",\"word\":"<<packet.word<<",\"token\":"<<token<<",\"writer_word\":"<<writer<<",\"allocation_word\":"<<allocation->word<<",\"status\":"<<residency_status<<",\"width\":"<<p.width<<",\"height\":"<<p.height<<",\"samples\":"<<p.samples<<",\"z_format\":"<<p.z_format<<",\"stencil_format\":"<<p.stencil_format<<",\"depth_address\":"<<(uint64_t(r[6])<<8)<<",\"stencil_address\":"<<(uint64_t(r[7])<<8)<<"}\n";

                       });
                       runtime_depth_residency.observe(api->context,[&](uint64_t token,uint64_t lease,uint32_t writer,uint64_t generation,const CtrDepthBackingSpansV1& spans,uint64_t begin,uint64_t bytes){census_log<<"{\"scope\":\"original_GnmSubmitFlip_depth_GPU_resident\",\"word\":"<<packet.word<<",\"status\":"<<residency_status<<",\"token\":"<<token<<",\"lease\":"<<lease<<",\"writer_word\":"<<writer<<",\"generation\":"<<generation<<",\"depth_address\":"<<spans.depth_address<<",\"depth_bytes\":"<<spans.depth_bytes<<",\"stencil_address\":"<<spans.stencil_address<<",\"stencil_bytes\":"<<spans.stencil_bytes<<",\"guard_begin\":"<<begin<<",\"guard_bytes\":"<<bytes<<",\"CPU_padding_bytes\":"<<(bytes-spans.depth_bytes-spans.stencil_bytes)<<",\"guest_publication\":false,\"retained_session\":true}\n";});
                       if(residency_status)return residency_status;
                       const auto visibility=+[](void* context)->int{return ctr_video_queue_memory_visibility_v1(context);};
                       const auto color_dirty=pending_color_guest_ranges.size(),depth_dirty=pending_depth_guest_owners.size();
                       const auto resident=+[](void* context,uint64_t token,uint32_t writer)->int{return runtime_depth_residency.covers(context,token,writer);};
                       GnmImageCompletion completion{color_dirty,&pending_depth_guest_owners,memory,resident,api->context,visibility,api->context};
                       const auto finish=GnmImageCompletion::finish;
                       const auto signal=+[](void*)->int{kernel_display_events.queues.graphics_signal(64);return 0;};
                       const int status=transaction->commit(data,packet.words,memory,finish,&completion,signal,nullptr);
                       pending_depth_guest_owners.observe([&](uint64_t token,uint32_t writer,bool dirty,uint32_t planes,bool completed,bool poisoned){census_log<<"{\"scope\":\"original_GnmSubmitFlip_depth_publication_owner\",\"word\":"<<packet.word<<",\"writer_word\":"<<writer<<",\"token\":"<<token<<",\"dirty\":"<<(dirty?"true":"false")<<",\"captured_planes\":"<<planes<<",\"publication_completed\":"<<(completed?"true":"false")<<",\"poisoned\":"<<(poisoned?"true":"false")<<"}\n";});
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_EOP_completion\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"event_control\":"<<data[1]<<",\"data_select\":"<<decoded.dataSelect<<",\"interrupt_select\":"<<decoded.interruptSelect<<",\"write_bytes\":"<<decoded.writeBytes<<",\"prior_color_writers\":"<<color_dirty<<",\"prior_depth_writers\":"<<depth_dirty<<",\"label_written_and_verified\":"<<(transaction->completed?"true":"false")<<",\"IRQ_delivered\":"<<(transaction->completed && decoded.interruptSelect?"true":"false")<<",\"poisoned\":"<<(transaction->poisoned?"true":"false")<<",\"GPU_execution\":"<<(transaction->completed?"true":"false")<<"}\n";return status;
                      }
                      if(packet.opcode==0x58)return acquire_packet(packet,data);
                      if(packet.opcode==0x46){
                       const auto visibility=+[](void* context)->int{return ctr_video_queue_memory_visibility_v1(context);};
                       const int status=complete_event_metadata_flush(data,packet.words,visibility,api->context);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_event_metadata_flush\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"control\":"<<(packet.words>1?data[1]:0)<<",\"real_queue_barrier_fence\":"<<(!status?"true":"false")<<",\"guest_writeback\":false,\"depth_dirty_owners\":"<<pending_depth_guest_owners.size()<<",\"GPU_execution\":"<<(!status?"true":"false")<<"}\n";return status;
                      }
                      if(packet.opcode==0x3c)return poll_wait_reg_mem(data,packet.words,ordered_registers.state(),memory);
                      if(packet.opcode==0x37){
                       const auto visibility=+[](void* context)->int{return ctr_video_queue_memory_visibility_v1(context);};
                       const auto resident=+[](void* context,uint64_t token,uint32_t writer)->int{return runtime_depth_residency.covers(context,token,writer);};
                       GnmImageCompletion completion{pending_color_guest_ranges.size(),&pending_depth_guest_owners,memory,resident,api->context,visibility,api->context};
                       const int status=complete_write_data(data,packet.words,memory,GnmImageCompletion::finish,&completion);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_WriteData_completed\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"actual_queue_fence\":"<<(!status?"true":"false")<<",\"guest_write\":"<<(!status?"true":"false")<<"}\n";return status;
                      }
                      if(packet.opcode==0x10 && packet.words>2 && (data[1]==0x68750001u || data[1]==0x68750002u || data[1]==0x6875000eu)){
                       const int status=ordered_registers.execute(packet,data);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_optional_debug_marker\",\"word\":"<<packet.word<<",\"marker\":"<<data[1]<<",\"words\":"<<packet.words<<",\"status\":"<<status<<",\"guest_debug_markers_enabled\":false,\"public_disabled_branch\":true,\"GPU_execution\":false}\n";return status;
                      }
                      return ordered_registers.execute(packet,data);
                     };
                     struct RefreshedComputeSample {const GraphicsStageOwner* stage;uint32_t sharp,consumer;uint64_t token,address;size_t epoch;std::vector<unsigned char> bytes;};
                     std::vector<RefreshedComputeSample> refreshed_compute_samples;
                     std::vector<std::pair<uint64_t,uint64_t>> completed_compute_ranges;
                     struct RefreshedComputeColor {uint64_t token,address;uint32_t writer;size_t epoch;std::vector<unsigned char> bytes;};
                     std::vector<RefreshedComputeColor> refreshed_compute_colors;
                     struct RefreshedColorSample {const GraphicsStageOwner* stage;uint32_t sharp;uint64_t address;std::vector<unsigned char> bytes;};
                     std::vector<RefreshedColorSample> refreshed_color_samples;
                     int graphics_depth_restore_status=0;
                     uint32_t rejected_graphics_word=UINT32_MAX;
                     PublishedColorGenerations published_color_generations;ComputeGuestWriteback last_display_color;size_t last_display_compute_epoch=0;
                     if(retained_readonly && retained_display_candidate.completed && !retained_display_candidate.poisoned){
                      auto& candidate=retained_display_candidate;std::vector<unsigned char> current(candidate.initial.size()),checked;
                      int retained_status=image_guest_copy(memory,candidate.address,current.data(),current.size(),false);
                      if(!retained_status)retained_status=retained_display_authority.acquire(reinterpret_cast<uint64_t>(api->context),retained_display_generation,candidate.address,candidate.initial.size(),current,checked);
                      if(!retained_status){last_display_color=candidate;const int recorded=published_color_generations.publish(last_display_color,0);if(recorded)return recorded;}
                      census_log<<"{\"scope\":\"cross_submission_display_authority\",\"status\":"<<retained_status<<",\"bytes\":"<<current.size()<<",\"readonly_commands\":true,\"current_guest_compared\":true,\"presentation\":false}\n";
                     }

                     std::vector<uint64_t> indexed_ms_initialized;
                     // Re-enter graphics after a completed compute/vertex run. Progress
                     // is mandatory; an unsupported route remains pending without spin.
                     for(;;){const auto execution_before=command_cursor.completed();
                     // Ordered register/visibility/EOP work is valid even without a draw.
                     // The same cursor stops at the first draw or unsupported packet.
                     const int prefix_status=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){return ordered_transport(packet,data);});
                     if(prefix_status && prefix_status!=88)return prefix_status;
                     while(const auto* pending=command_cursor.pending()){
                      const auto completed_before=command_cursor.completed();
                      for(auto& draw:attachment_joins){const auto& pair=*draw.pair;
                       if(pair.word!=pending->word || pair.word==rejected_graphics_word)continue;
                       char draw_phase_flag[2]{};const bool draw_phase_enabled=GetEnvironmentVariableA("CTR_TILING_TIMING",draw_phase_flag,sizeof(draw_phase_flag))==1 && draw_phase_flag[0]=='1';
                       if(draw_phase_enabled)std::fprintf(stderr,"CTR_GRAPHICS_CONTEXT begin submission=%llu word=%u\n",static_cast<unsigned long long>(submission_token),pair.word);
                       struct DrawTiming {bool enabled;uint64_t submission;uint32_t word;ULONGLONG started;~DrawTiming(){if(enabled)std::fprintf(stderr,"CTR_GRAPHICS_CONTEXT end submission=%llu word=%u\n",static_cast<unsigned long long>(submission),word);std::cout<<"{\"scope\":\"original_GnmSubmitFlip_draw_timing\",\"submission\":"<<submission<<",\"word\":"<<word<<",\"elapsed_ms\":"<<(GetTickCount64()-started)<<",\"includes_pipeline_coherence_draw_readback_and_resume\":true}\n";}} draw_timing{draw_phase_enabled,submission_token,pair.word,GetTickCount64()};
                       IndexedCommandParameters command{};
                       if(pair.draw_inputs.indexed){
                        const GraphicsBufferCapture* captured=nullptr;uint64_t token=0,bytes=0;uint32_t stride=0;
                        for(const auto& index:submission_owner.indices)if(index.word==pair.word){if(captured)return 112;captured=&index.capture;}
                        for(const auto& index:live_indices)if(index.word==pair.word){if(token)return 112;token=index.token;bytes=index.bytes;stride=index.stride;}
                        IndexedDrawTransport transport;const int index_status=captured?prepare_indexed_transport(pair.draw_inputs,*captured,token,stride,bytes,memory,completed_color_ranges,completed_compute_ranges,transport):88;
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_transport\",\"word\":"<<pair.word<<",\"status\":"<<index_status<<",\"count\":"<<transport.count<<",\"type\":"<<transport.type<<",\"bytes\":"<<transport.bytes<<",\"minimum\":"<<transport.minimum<<",\"maximum\":"<<transport.maximum<<",\"live_index_token\":"<<(transport.token?"true":"false")<<",\"current_guest_and_writer_generation_checked\":true,\"GPU_execution\":false}\n";
                        if(index_status && index_status!=88)return index_status;if(index_status)continue;
                        uint64_t indexed_vertex_compared=0;const int vertex_generation=prepare_indexed_vertices(pair.word,pair.vertex_fetch,live_vertices,memory,completed_color_ranges,completed_compute_ranges,indexed_vertex_compared);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_vertex_generation\",\"word\":"<<pair.word<<",\"status\":"<<vertex_generation<<",\"compared_bytes\":"<<indexed_vertex_compared<<",\"current_guest_and_writer_generation_checked\":true,\"GPU_execution\":false}\n";
                        if(vertex_generation && vertex_generation!=88)return vertex_generation;if(vertex_generation)continue;
                        auto index_bank=std::make_unique<CtrPm4::Registers>();CtrGraphicsDrawParametersV1 offsets{};
                        int offsets_status=prepare_draw_execution_bank(pair.original_packet.data(),pair.original_packet.size(),ordered_registers.state(),*index_bank);
                        if(!offsets_status)offsets_status=decode_owned_draw_parameters(*index_bank,pair.vertex_pipeline,*pair.vertex,offsets);
                        CtrGraphicsDynamicStateV1 index_dynamic{};const int dynamics_status=decode_owned_dynamic_state(ordered_registers.state(),true,index_dynamic);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_draw_handoff\",\"word\":"<<pair.word<<",\"offsets_status\":"<<offsets_status<<",\"dynamic_status\":"<<dynamics_status<<",\"vertex_offset_raw\":"<<offsets.vertex_offset<<",\"first_instance\":"<<offsets.instance_offset<<",\"instance_count\":"<<offsets.instance_count<<",\"depth_test\":"<<index_dynamic.depth_test<<",\"depth_write\":"<<index_dynamic.depth_write<<",\"stencil_test\":"<<index_dynamic.stencil_test<<",\"stencil_front_write_mask\":"<<index_dynamic.front.write_mask<<",\"stencil_back_write_mask\":"<<index_dynamic.back.write_mask<<",\"stencil_front_fail\":"<<index_dynamic.front.fail_op<<",\"stencil_front_pass\":"<<index_dynamic.front.pass_op<<",\"stencil_front_depth_fail\":"<<index_dynamic.front.depth_fail_op<<",\"stencil_back_fail\":"<<index_dynamic.back.fail_op<<",\"stencil_back_pass\":"<<index_dynamic.back.pass_op<<",\"stencil_back_depth_fail\":"<<index_dynamic.back.depth_fail_op<<",\"original_execution_bank_retained\":true,\"GPU_execution\":false}\n";
                        if(offsets_status && offsets_status!=88)return offsets_status;if(dynamics_status && dynamics_status!=88)return dynamics_status;
                        const int command_status=offsets_status?offsets_status:prepare_indexed_command(transport,offsets,command);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_command_parameters\",\"word\":"<<pair.word<<",\"status\":"<<command_status<<",\"index_count\":"<<command.index_count<<",\"index_type\":"<<command.index_type<<",\"first_index\":"<<command.first_index<<",\"index_buffer_offset\":"<<command.index_buffer_offset<<",\"vertex_offset\":"<<command.vertex_offset<<",\"first_instance\":"<<command.first_instance<<",\"instance_count\":"<<command.instance_count<<",\"GPU_execution\":false}\n";
                        if(command_status && command_status!=88)return command_status;
                        uint64_t checked_ranges=0;const int range_status=command_status?command_status:prepare_indexed_vertex_ranges(transport,command,pair.vertex_fetch,checked_ranges);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_vertex_ranges\",\"word\":"<<pair.word<<",\"status\":"<<range_status<<",\"checked_bindings\":"<<checked_ranges<<",\"GPU_execution\":false}\n";
                        if(range_status && range_status!=88)return range_status;if(range_status)continue;
                        CtrGraphicsDrawV1 retained_vertices{};retained_vertices.version=1;retained_vertices.size=sizeof(retained_vertices);retained_vertices.vertex_buffer_count=pair.vertex_fetch.value.count;
                        for(uint32_t v=0;v<retained_vertices.vertex_buffer_count;++v)for(const auto& live:live_vertices)if(live.word==pair.word && live.binding==pair.vertex_fetch.value.inputs[v].binding)retained_vertices.vertex_buffers[v]=live.token;
                        CtrGraphicsIndexedDrawV1 indexed_preflight{};const int indexed_cabi_status=prepare_indexed_call(command,retained_vertices,indexed_preflight);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_CABI_transport\",\"word\":"<<pair.word<<",\"status\":"<<indexed_cabi_status<<",\"index_count\":"<<indexed_preflight.index_count<<",\"index_bytes\":"<<indexed_preflight.index_bytes<<",\"vertex_buffers\":"<<indexed_preflight.vertex_buffer_count<<",\"signed_vertex_offset\":"<<indexed_preflight.vertex_offset<<",\"original_tokens_preserved\":true,\"descriptor_admission_pending\":true,\"GPU_execution\":false}\n";
                        if(indexed_cabi_status && indexed_cabi_status!=88)return indexed_cabi_status;if(indexed_cabi_status)continue;
                        // Prepare the genuine two-sample backing from the original
                        // initialized single-sample fallback, before shader admission.
                        // Only an untouched CPU-owned generation may take this path.
                        if(pair.graphics_key.num_color_attachments==1 && pair.graphics_key.mrt_mask==1 && pair.graphics_key.num_samples==2 && draw.colors[0] &&
                           std::find(indexed_ms_initialized.begin(),indexed_ms_initialized.end(),draw.colors[0])==indexed_ms_initialized.end()){
                         std::vector<const GraphicsStageOwner::Color*> sources;const int source_status=graphics_pair_color_inputs(pair,sources);
                         if(source_status)return source_status;const GraphicsStageOwner::Color* source=nullptr;for(const auto* candidate:sources)if(candidate->slot==0)source=candidate;
                         if(!source)return 112;const auto& shape=source->requirement;
                         if(shape.data_format!=10 || shape.number_format || shape.samples!=1 || shape.layers!=1 || !shape.bytes || shape.bytes>16*1024*1024 || shape.bytes!=uint64_t(shape.pitch)*shape.height*4)return 88;
                         if(!draw_snapshot_disjoint(shape.guest_address,shape.bytes,completed_color_ranges) || !draw_snapshot_disjoint(shape.guest_address,shape.bytes,completed_compute_ranges))continue;
                         std::vector<unsigned char> before(static_cast<size_t>(shape.bytes)),after(before.size());
                         int prepared=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],before.data(),shape.bytes,uint64_t(shape.pitch)*4);
                         if(!prepared)prepared=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],0);
                         if(!prepared)prepared=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],1);
                         if(!prepared)prepared=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],after.data(),shape.bytes,uint64_t(shape.pitch)*4);
                         uint64_t differences=0;if(!prepared)for(size_t n=0;n<before.size();++n)differences+=before[n]!=after[n];
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_color_MS_initialization\",\"word\":"<<pair.word<<",\"status\":"<<prepared<<",\"compared_bytes\":"<<shape.bytes<<",\"differences\":"<<differences<<",\"sample0_roundtrip\":true,\"original_key_unchanged\":true,\"guest_writeback\":false,\"original_draw\":false,\"GPU_execution\":"<<(!prepared?"true":"false")<<"}\n";
                         if(prepared || differences)return prepared?prepared:112;
                         indexed_ms_initialized.push_back(draw.colors[0]);
                        }



                       }
                       unsigned ready=0;for(const auto& resource:prepared_draw_resources)if(resource.word==pair.word)++ready;
                       if(ready!=1)continue;
                       if(graphics_depth_restore_status)return graphics_depth_restore_status;
                       struct GraphicsDepthReadScope {void* context;std::vector<uint64_t> depths;int& status;std::ostream& log;uint32_t word;
                        ~GraphicsDepthReadScope(){for(auto depth:depths){const int restored=ctr_video_depth_attachment_restore_v1(context,depth);if(restored && !status)status=restored;log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_restore\",\"word\":"<<word<<",\"status\":"<<restored<<",\"real_fence\":"<<(!restored?"true":"false")<<"}\n";}}
                       } depth_read{api->context,{},graphics_depth_restore_status,census_log,pair.word};
                       // The tall D16 descriptor was captured before its original writer.
                       // Discover its exact retained GPU generation only at the consumer;
                       // the existing refresh below rebuilds the complete descriptor set.
                       for(const auto* stage:{pair.fragment_inputs.get(),pair.vertex_inputs.get()})if(stage)for(auto& d:stage->descriptors){
                        if(d.kind!=1 || !DepthResidency::tall_d16_view(d.image,d.image_extent))continue;
                        bool exists=false;for(const auto& sample:live_depth_samples)if(sample.stage==stage && sample.sharp==d.sharp && sample.consumer==pair.word)exists=true;
                        if(exists)continue;
                        uint64_t image=0,generation=0,begin=0,bytes=0,lease=0;std::shared_ptr<void> owner;
                        if(!runtime_gpu_resident_memory.identify(d.image.guest_address,image,generation,begin,bytes))return 88;
                        const int resolved=runtime_depth_residency.resolve_sampled_view(api->context,d.image,d.image_extent,generation,lease,owner);
                        if(resolved)return resolved;if(image!=lease || !owner)return 112;
                        d.resident_image_authority=[context=api->context,p=d.image,e=d.image_extent,generation,lease,owner](){uint64_t current=0;std::shared_ptr<void> retained;const int status=runtime_depth_residency.resolve_sampled_view(context,p,e,generation,current,retained);return status?status:(current==lease && retained==owner?0:112);};
                        live_depth_samples.push_back({stage,d.sharp,pair.word,0,lease});
                       }
                       for(auto& sample:live_depth_samples)if(sample.consumer==pair.word){
                        for(const auto& descriptor:sample.stage->descriptors)if(descriptor.kind==1 && descriptor.sharp==sample.sharp && descriptor.submission_image_authority){
                         const int current=descriptor.submission_image_authority();
                         const auto& extent=descriptor.image_extent;
                         const bool disjoint=draw_snapshot_disjoint(extent.guest_address,extent.bytes,completed_compute_ranges);
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_submission_depth_authority\",\"word\":"<<pair.word<<",\"sharp\":"<<sample.sharp<<",\"owner_status\":"<<current<<",\"compute_disjoint\":"<<(disjoint?"true":"false")<<"}\n";
                         if(current || !disjoint)return current?current:88;
                        }
                        // Refresh the real sampled descriptor at execution, after prior fenced writers.
                        for(auto& descriptor:sample.stage->descriptors)if(descriptor.kind==1 && descriptor.sharp==sample.sharp && descriptor.resident_image_authority){
                         int writer_status=0;
                         pending_depth_guest_owners.observe([&](uint64_t token,uint32_t writer,bool dirty,uint32_t planes,bool,bool){
                          if(!dirty || writer_status)return;
                          for(const auto& retained:retained_depth_identities)if(retained.second==token){const auto& a=*retained.first;const auto& r=a.identity;const auto& p=a.parameters;
                           if((uint64_t(r[6])<<8)!=descriptor.image_extent.guest_address)return;
                           if(planes || !((p.samples==2 && p.z_format==3 && p.stencil_format==1) || (p.samples==1 && p.z_format==1 && !p.stencil_format && p.width==2048 && p.height==8192))){writer_status=88;return;}
                           CtrDepthMS2LayoutV1 layout{1,sizeof(layout),p.width,p.height,((r[8]&2047)+1)*8,p.samples,p.z_format,p.stencil_format,(r[2]>>20)&7,r[9]&0x3fffff,r[10]&2047,(r[10]>>13)&2047,0,0,uint64_t(r[6])<<8};
                           writer_status=runtime_depth_residency.retain(api,graphics_session,token,writer,layout,uint64_t(r[7])<<8);return;
                          }
                         });
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_refresh_writer\",\"word\":"<<pair.word<<",\"status\":"<<writer_status<<"}\n";
                         if(writer_status)return writer_status;
                         uint64_t owner=0,generation=0,begin=0,bytes=0;
                         if(!runtime_gpu_resident_memory.identify(descriptor.image.guest_address,owner,generation,begin,bytes))return 88;
                         std::shared_ptr<void> lease;uint64_t resolved=0;
                         const int status=runtime_depth_residency.resolve_sampled_view(api->context,descriptor.image,descriptor.image_extent,generation,resolved,lease);
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_refresh_resolve\",\"word\":"<<pair.word<<",\"generation\":"<<generation<<",\"status\":"<<status<<"}\n";
                         if(status)return status;if(resolved!=owner || !lease)return 112;
                         uint64_t alias=0;const int created=ctr_video_depth_sampled_alias_v1(api->context,owner,&alias);census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_refresh_alias\",\"word\":"<<pair.word<<",\"status\":"<<created<<"}\n";if(created)return created;
                         const int adopted=depth_aliases.adopt(alias);if(adopted){ctr_video_sampled_image_release_v1(api->context,alias);return adopted;}
                         unsigned matches=0;for(auto& image:live_images)if(image.stage==sample.stage && image.sharp==sample.sharp){image.token=alias;++matches;}
                         if(matches!=1)return 112;
                         descriptor.resident_image_authority=[context=api->context,p=descriptor.image,extent=descriptor.image_extent,generation,owner,lease](){
                          uint64_t current=0;std::shared_ptr<void> retained;const int checked=runtime_depth_residency.resolve_sampled_view(context,p,extent,generation,current,retained);
                          return checked?checked:(current==owner && retained==lease?0:112);
                         };
                         std::vector<GraphicsImageWrite> writes;const int prepared=prepare_graphics_image_writes(pair,live_images,writes);if(prepared)return prepared;
                         std::vector<uint32_t> bindings,nulls;std::vector<uint64_t> tokens;
                         for(const auto& write:writes){bindings.push_back(write.binding);tokens.push_back(write.token);nulls.push_back(write.semantic_null?1u:0u);}
                         // Layouts deliberately permit one descriptor write. Build a fresh
                         // complete set before creating this draw's pipeline; retain the old
                         // set until batch teardown for any earlier fenced consumers.
                         uint64_t replacement_layout=0;int rebound=layouts.create(pair.fragment_bindings,pair.vertex_bindings,&replacement_layout);
                         std::vector<GraphicsBufferWrite> buffer_writes;if(!rebound)rebound=prepare_graphics_buffer_writes(pair,live_buffers,buffer_writes);
                         std::vector<CtrGraphicsBufferWriteV1> actual_buffers;
                         for(const auto& write:buffer_writes)actual_buffers.push_back({write.binding,write.slot,1u,write.null_clip_planes?1u:0u,write.token});
                         if(!rebound)rebound=ctr_video_graphics_layout_buffers_v1(api->context,replacement_layout,actual_buffers.data(),static_cast<uint32_t>(actual_buffers.size()));
                         std::vector<GraphicsSamplerWrite> sampler_writes;if(!rebound)rebound=prepare_graphics_sampler_writes(pair,live_samplers,sampler_writes);
                         std::vector<uint32_t> sampler_bindings;std::vector<uint64_t> sampler_tokens;
                         for(const auto& write:sampler_writes){sampler_bindings.push_back(write.binding);sampler_tokens.push_back(write.token);}
                         if(!rebound)rebound=ctr_video_graphics_layout_samplers_v1(api->context,replacement_layout,sampler_bindings.data(),sampler_tokens.data(),static_cast<uint32_t>(sampler_tokens.size()));
                         if(!rebound)rebound=ctr_video_graphics_layout_images_null_v1(api->context,replacement_layout,bindings.data(),tokens.data(),nulls.data(),static_cast<uint32_t>(tokens.size()));census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_refresh_descriptor\",\"word\":"<<pair.word<<",\"status\":"<<rebound<<"}\n";if(rebound)return rebound;
                         draw.layout=replacement_layout;for(auto& resource:prepared_draw_resources)if(resource.word==pair.word)resource.layout=replacement_layout;
                         sample.image=alias;sample.depth=owner;
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_rebound\",\"word\":"<<pair.word<<",\"sharp\":"<<sample.sharp<<",\"generation\":"<<generation<<",\"lease\":"<<owner<<",\"status\":0,\"actual_descriptor_updated\":true,\"CPU_snapshot_uploaded\":false}\n";
                        }
                        bool authorized=false;
                        for(const auto& descriptor:sample.stage->descriptors)if(descriptor.kind==1 && descriptor.sharp==sample.sharp && descriptor.resident_image_authority){
                         const int checked=descriptor.resident_image_authority();
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_generation_check\",\"word\":"<<pair.word<<",\"sharp\":"<<sample.sharp<<",\"status\":"<<checked<<",\"before_GPU_draw\":true}\n";
                         if(checked)return checked;authorized=true;
                        }
                        // In-submit aliases have a live allocation token rather than
                        // a cross-submit registry callback. Require that actual token.
                        if(!authorized)for(const auto& retained:retained_depth_identities)if(retained.second==sample.depth)authorized=true;
                        if(!authorized)return 88;
                        if(draw.depth==sample.depth)return 88;
                        bool active=false;for(auto owner:depth_read.depths)if(owner==sample.depth)active=true;if(active)continue;
                        uint64_t transition=0;const int activated=ctr_video_depth_sampled_alias_v1(api->context,sample.depth,&transition);if(activated)return activated;
                        depth_read.depths.push_back(sample.depth);const int released=ctr_video_sampled_image_release_v1(api->context,transition);if(released)return released;
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_graphics_depth_sampled_active\",\"word\":"<<pair.word<<",\"status\":0,\"same_retained_GPU_owner\":true,\"CPU_snapshot_uploaded\":false}\n";
                       }
                       // CPU snapshots may be reused only when no earlier GPU color
                       // writer overlaps any consumed buffer. Sampled images require
                       // a separate completed-generation transfer before admission.
                       const ULONGLONG source_coherence_started=GetTickCount64();
                       int source_coherence=0;
                       std::vector<const GraphicsStageOwner::Color*> color_sources;
                       const bool inactive_attachments=!(pair.registers->words[0xa200]&7u) && std::all_of(std::begin(pair.graphics_key.colors),std::end(pair.graphics_key.colors),[](const auto& color){return !color.data_format && !color.write_mask;});
                       if(graphics_pair_color_inputs(pair,color_sources) && !inactive_attachments)source_coherence=88;
                       for(const auto* source:color_sources){const auto& demand=source->requirement;
                        if(draw_snapshot_disjoint(demand.guest_address,demand.bytes,completed_compute_ranges))continue;
                        bool fresh=false;for(const auto& generation:refreshed_compute_colors)if(generation.token==draw.colors[source->slot] && generation.address==demand.guest_address && generation.bytes.size()==demand.bytes){
                         bool superseded=false;for(size_t k=generation.epoch;k<completed_compute_ranges.size();++k){const std::vector<std::pair<uint64_t,uint64_t>> later{completed_compute_ranges[k]};if(!draw_snapshot_disjoint(demand.guest_address,demand.bytes,later))superseded=true;}
                         std::vector<unsigned char> current(generation.bytes.size());if(!superseded && !memory.read(memory.context,generation.address,current.data(),current.size()) && current==generation.bytes)fresh=true;
                        }
                        if(!fresh)source_coherence=88;
                       }

                       for(const auto* stage:{pair.fragment_inputs.get(),pair.vertex_inputs.get()}){
                        if(!stage){source_coherence=88;continue;}
                        for(const auto& descriptor:stage->descriptors)if(descriptor.kind==1){
                         if(verified_effective_null_image(descriptor)){census_log<<"{\"scope\":\"original_GnmSubmitFlip_sampled_semantic_null_admission\",\"word\":"<<pair.word<<",\"sharp\":"<<descriptor.sharp<<",\"status\":0,\"eight_DWORD_source_verified\":true,\"public_GetSharp_fallback\":true,\"guest_payload_required\":false}\n";continue;}
                         bool refreshed=false;
                         for(const auto& sample:live_depth_samples)if(sample.stage==stage && sample.sharp==descriptor.sharp && sample.consumer==pair.word){
                          for(auto owner:depth_read.depths)if(owner==sample.depth)refreshed=true;
                         }
                         for(const auto& sample:refreshed_compute_samples)if(sample.stage==stage && sample.sharp==descriptor.sharp && sample.consumer==pair.word){
                          std::vector<unsigned char> current(sample.bytes.size());const int read=memory.read(memory.context,sample.address,current.data(),current.size());
                          bool superseded=false;for(size_t k=sample.epoch;k<completed_compute_ranges.size();++k){const std::vector<std::pair<uint64_t,uint64_t>> later{completed_compute_ranges[k]};if(!draw_snapshot_disjoint(sample.address,sample.bytes.size(),later))superseded=true;}
                          if(!read && !superseded && current==sample.bytes && draw_snapshot_disjoint(sample.address,sample.bytes.size(),completed_color_ranges))refreshed=true;
                         }
                         for(const auto& sample:refreshed_color_samples)if(sample.stage==stage && sample.sharp==descriptor.sharp){
                          std::vector<unsigned char> current(sample.bytes.size());
                          if(!image_guest_copy(memory,sample.address,current.data(),current.size(),false) && current==sample.bytes && published_color_generations.matches(sample.address,sample.bytes.size(),current,completed_color_ranges))refreshed=true;
                         }
                         for(const auto& sample:initial_color_samples)if(sample.stage==stage && sample.sharp==descriptor.sharp &&
                          draw_snapshot_disjoint(sample.address,sample.bytes.size(),completed_color_ranges) && draw_snapshot_disjoint(sample.address,sample.bytes.size(),completed_compute_ranges)){
                          std::vector<unsigned char> current(sample.bytes.size());
                          if(!image_guest_copy(memory,sample.address,current.data(),current.size(),false) && current==sample.bytes)refreshed=true;
                         }
                         if(!refreshed && !descriptor.image_payload.capture_status && descriptor.image_payload.snapshot.size()==descriptor.image_extent.bytes &&
                          draw_snapshot_disjoint(descriptor.image_extent.guest_address,descriptor.image_extent.bytes,completed_color_ranges) && draw_snapshot_disjoint(descriptor.image_extent.guest_address,descriptor.image_extent.bytes,completed_compute_ranges)){
                          bool uploaded=false;for(const auto& image:live_images)if(image.stage==stage && image.sharp==descriptor.sharp)uploaded=true;
                          std::vector<unsigned char> current(descriptor.image_payload.snapshot.size());
                          if(uploaded && !image_guest_copy(memory,descriptor.image_extent.guest_address,current.data(),current.size(),false) && current==descriptor.image_payload.snapshot)refreshed=true;
                         }
                         // Reached 1D palette: its complete padded backing was written
                         // and retiled by an earlier original color draw. Require the
                         // exact published generation, never the prefetched CPU image.
                         if(!refreshed && descriptor.image.type==8 && !descriptor.image_extent_status && descriptor.image_extent.bytes && descriptor.image_extent.bytes<=32*1024*1024){
                          const auto& extent=descriptor.image_extent;std::vector<unsigned char> current(static_cast<size_t>(extent.bytes));
                          if(!image_guest_copy(memory,extent.guest_address,current.data(),current.size(),false) && published_color_generations.matches(extent.guest_address,extent.bytes,current,completed_color_ranges) && draw_snapshot_disjoint(extent.guest_address,extent.bytes,completed_compute_ranges)){
                           int transferred=88;for(const auto& image:live_images)if(image.stage==stage && image.sharp==descriptor.sharp)transferred=ctr_video_sampled_image_upload_tiled_v1(api->context,image.token,current.data(),current.size(),&extent);
                           std::vector<unsigned char> checked(current.size());const int read=transferred?transferred:image_guest_copy(memory,extent.guest_address,checked.data(),checked.size(),false);
                           if(!read && checked==current && published_color_generations.matches(extent.guest_address,extent.bytes,checked,completed_color_ranges)){refreshed=true;refreshed_color_samples.push_back({stage,descriptor.sharp,extent.guest_address,std::move(checked)});}
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_published_color_to_1D_sampled\",\"word\":"<<pair.word<<",\"sharp\":"<<descriptor.sharp<<",\"status\":"<<(refreshed?0:read?read:91)<<",\"guest_bytes\":"<<extent.bytes<<",\"exact_published_GPU_generation\":true,\"actual_tiled_upload\":"<<(!transferred?"true":"false")<<",\"guest_rechecked_after_fence\":"<<(refreshed?"true":"false")<<",\"payload_persisted\":false}\n";
                          }
                         }
                         if(!refreshed){const auto& extent=descriptor.image_extent;std::vector<unsigned char> checked(descriptor.image_payload.snapshot.size());const int read=checked.empty()?88:image_guest_copy(memory,extent.guest_address,checked.data(),checked.size(),false);uint64_t differences=0;if(!read)for(size_t i=0;i<checked.size();++i)differences+=checked[i]!=descriptor.image_payload.snapshot[i];census_log<<"{\"scope\":\"original_GnmSubmitFlip_sampled_coherence_missing\",\"word\":"<<pair.word<<",\"sharp\":"<<descriptor.sharp<<",\"read_status\":"<<read<<",\"snapshot_bytes\":"<<checked.size()<<",\"extent_bytes\":"<<extent.bytes<<",\"differences\":"<<differences<<",\"color_disjoint\":"<<(draw_snapshot_disjoint(extent.guest_address,extent.bytes,completed_color_ranges)?"true":"false")<<",\"compute_disjoint\":"<<(draw_snapshot_disjoint(extent.guest_address,extent.bytes,completed_compute_ranges)?"true":"false")<<"}\n";}
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_sampled_source_admission\",\"word\":"<<pair.word<<",\"sharp\":"<<descriptor.sharp<<",\"vertex\":"<<(stage==pair.vertex_inputs.get()?"true":"false")<<",\"status\":"<<(refreshed?0:88)<<",\"fresh_guest_and_writer_generation_checked\":true}\n";
                         if(!refreshed)source_coherence=88;
                        }
                        for(const auto& buffer:stage->buffers)if(!draw_snapshot_disjoint(buffer.address,buffer.bytes,completed_color_ranges) || !draw_snapshot_disjoint(buffer.address,buffer.bytes,completed_compute_ranges))source_coherence=88;
                       }
                       for(const auto& input:pair.vertex_fetch.buffers)if(!draw_snapshot_disjoint(input.address,input.bytes,completed_color_ranges) || !draw_snapshot_disjoint(input.address,input.bytes,completed_compute_ranges))source_coherence=88;
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_source_coherence\",\"word\":"<<pair.word<<",\"status\":"<<source_coherence<<",\"prior_color_writers\":"<<completed_color_ranges.size()<<",\"sampled_transfer_required\":true,\"GPU_execution\":false}\n";
                       char coherence_flag[2]{};if(GetEnvironmentVariableA("CTR_TILING_TIMING",coherence_flag,sizeof(coherence_flag))==1 && coherence_flag[0]=='1')
                        std::cout<<"{\"scope\":\"original_GnmSubmitFlip_source_coherence_timing\",\"submission\":"<<submission_token<<",\"word\":"<<pair.word<<",\"elapsed_ms\":"<<(GetTickCount64()-source_coherence_started)<<",\"status\":"<<source_coherence<<",\"includes_source_validation_and_refresh\":true}\n";
                       if(source_coherence)continue;
                      if(first_pipeline){const int released=ctr_video_graphics_pipeline_release_v1(api->context,first_pipeline);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_pipeline_release\",\"word\":"<<first_draw_word<<",\"status\":"<<released<<"}\n";if(released)return released;first_pipeline=0;}
                       first_draw_word=pair.word;
                       bool validated=false;const int identity=execute_owned_route(submission_owner,*pending,words.data()+pending->word,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){validated=true;return 88;});
                       if(identity!=88 || !validated)return identity==88?112:(identity?identity:112);
                       CtrGraphicsViewportStateV1 viewports{};const int viewport_status=decode_owned_viewports(ordered_registers.state(),pair.compiler_profile,viewports);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_viewports\",\"word\":"<<pair.word<<",\"status\":"<<viewport_status<<",\"count\":"<<viewports.count<<",\"max_width\":"<<pair.compiler_profile.max_viewport_width<<",\"max_height\":"<<pair.compiler_profile.max_viewport_height<<",\"GPU_execution\":false}\n";
                       if(viewport_status && viewport_status!=88)return viewport_status;
                       if(!viewport_status)for(uint32_t i=0;i<viewports.count;++i){const auto& v=viewports.viewports[i];const auto& sc=viewports.scissors[i];
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_viewport_value\",\"word\":"<<pair.word<<",\"index\":"<<i<<",\"x\":"<<v.x<<",\"y\":"<<v.y<<",\"width\":"<<v.width<<",\"height\":"<<v.height<<",\"min_depth\":"<<v.minDepth<<",\"max_depth\":"<<v.maxDepth<<",\"scissor_x\":"<<sc.offset.x<<",\"scissor_y\":"<<sc.offset.y<<",\"scissor_width\":"<<sc.extent.width<<",\"scissor_height\":"<<sc.extent.height<<",\"GPU_execution\":false}\n";}

                       first_viewports=viewports;first_viewport_status=viewport_status;
                       first_dynamic_status=decode_owned_dynamic_state(ordered_registers.state(),pair.draw_inputs.indexed,first_dynamic);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_dynamic_state\",\"word\":"<<pair.word<<",\"status\":"<<first_dynamic_status<<",\"depth_test\":"<<first_dynamic.depth_test<<",\"stencil_test\":"<<first_dynamic.stencil_test<<",\"primitive_restart\":"<<first_dynamic.primitive_restart<<",\"rasterizer_discard\":"<<first_dynamic.rasterizer_discard<<",\"cull_mode\":"<<first_dynamic.cull_mode<<",\"front_face\":"<<first_dynamic.front_face<<",\"line_width\":"<<first_dynamic.line_width<<",\"GPU_execution\":false}\n";
                       if(first_dynamic_status && first_dynamic_status!=88)return first_dynamic_status;
                       if(!first_dynamic_status && pair.graphics_key.num_color_attachments==2){
                        for(unsigned face=0;face<2;++face){const auto& value=face?first_dynamic.back:first_dynamic.front;
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_MRT_stencil_face\",\"word\":"<<pair.word<<",\"face\":"<<face<<",\"fail_op\":"<<value.fail_op<<",\"pass_op\":"<<value.pass_op<<",\"depth_fail_op\":"<<value.depth_fail_op<<",\"compare_op\":"<<value.compare_op<<",\"reference\":"<<value.reference<<",\"write_mask\":"<<value.write_mask<<",\"compare_mask\":"<<value.compare_mask<<",\"GPU_execution\":false}\n";
                        }
                       }

                       if(!first_viewport_status && !first_dynamic_status)first_dynamic_preflight_status=ctr_video_graphics_dynamic_preflight_v1(api->context,&first_viewports,&first_dynamic);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_dynamic_preflight\",\"word\":"<<pair.word<<",\"status\":"<<first_dynamic_preflight_status<<",\"selected_device\":true,\"GPU_execution\":false}\n";
                       if(first_dynamic_preflight_status && first_dynamic_preflight_status!=88)return first_dynamic_preflight_status;
                       auto& draw_parameters=first_draw_parameters;auto draw_bank=std::make_unique<CtrPm4::Registers>();first_draw_parameter_status=prepare_draw_execution_bank(pair.original_packet.data(),pair.original_packet.size(),ordered_registers.state(),*draw_bank);if(!first_draw_parameter_status)first_draw_parameter_status=decode_owned_draw_parameters(*draw_bank,pair.vertex_pipeline,*pair.vertex,draw_parameters);const int draw_parameter_status=first_draw_parameter_status;
                       if(!first_draw_parameter_status){unsigned differences=0;for(size_t i=0;i<draw_bank->words.size();++i)if(draw_bank->words[i]!=ordered_registers.state().words[i])++differences;
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_draw_execution_bank\",\"word\":"<<pair.word<<",\"count\":"<<draw_bank->words[0xC24C]<<",\"initiator\":"<<draw_bank->words[0xA1FC]<<",\"changed_words\":"<<differences<<",\"compute_unchanged\":"<<(draw_bank->compute==ordered_registers.state().compute?"true":"false")<<",\"original_bank_retained\":true,\"GPU_execution\":false}\n";
                       }
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_draw_parameters\",\"word\":"<<pair.word<<",\"status\":"<<draw_parameter_status<<",\"vertex_offset\":"<<draw_parameters.vertex_offset<<",\"instance_offset\":"<<draw_parameters.instance_offset<<",\"instance_count\":"<<draw_parameters.instance_count<<",\"GPU_execution\":false}\n";
                       if(draw_parameter_status && draw_parameter_status!=88)return draw_parameter_status;
                       std::vector<GraphicsBufferWrite> push_writes;auto& push_resources=first_push_resources;
                       int push_status=prepare_graphics_buffer_writes(pair,live_buffers,push_writes);if(!push_status)push_status=prepare_push_resources(pair.fragment_bindings,pair.vertex_bindings,push_writes,push_resources);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_push_resources\",\"word\":"<<pair.word<<",\"status\":"<<push_status<<",\"userdata_count\":"<<push_resources.userdata_count<<",\"buffer_count\":"<<push_resources.buffer_count<<",\"dedicated_buffer_offsets_zero\":true,\"viewport_transform_decoded_in_next_stage\":true,\"GPU_execution\":false}\n";
                       first_push_status=push_status;if(push_status && push_status!=88)return push_status;
                       if(!push_status){std::array<uint32_t,40> offsets{};for(uint32_t i=0;i<push_resources.buffer_count;++i)offsets[i]=push_resources.offsets[i];
                        first_push_data_status=decode_owned_push_data(ordered_registers.state(),pair.fragment_bindings,pair.vertex_bindings,offsets.data(),push_resources.buffer_count,first_push_data);
                        if(!first_push_data_status && std::memcmp(first_push_data.bytes+16,push_resources.userdata.data(),sizeof(push_resources.userdata)))return 112;
                        if(!first_push_data_status){float transform[4]{};std::memcpy(transform,first_push_data.bytes,sizeof(transform));
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_push_viewport_transform\",\"word\":"<<pair.word<<",\"xoffset\":"<<transform[0]<<",\"xscale\":"<<transform[2]<<",\"yoffset\":"<<transform[1]<<",\"yscale\":"<<transform[3]<<",\"decoded_from_original_register_bank\":true,\"transport_modified\":false,\"GPU_execution\":false}\n";
                        }
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_push_data\",\"word\":"<<pair.word<<",\"status\":"<<first_push_data_status<<",\"payload_bytes\":120,\"transport_bytes\":128,\"same_pass_userdata_match\":"<<(!first_push_data_status?"true":"false")<<",\"GPU_execution\":false}\n";
                        if(first_push_data_status && first_push_data_status!=88)return first_push_data_status;}
                       unsigned written_buffers=0,written_images=0,atomic_resources=0;
                       for(const auto* stage:{pair.fragment.get(),pair.vertex.get()})for(uint32_t i=0;i<stage->resources.binding_count;++i){const auto& resource=stage->resources.bindings[i];
                        if(resource.flags&1u){if(resource.kind==0)++written_buffers;else if(resource.kind==1)++written_images;}if(resource.flags&16u)++atomic_resources;}
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_shader_writeback_requirements\",\"word\":"<<pair.word<<",\"written_buffers\":"<<written_buffers<<",\"written_images\":"<<written_images<<",\"atomic_resources\":"<<atomic_resources<<",\"color_attachment_writer_separate\":true,\"GPU_execution\":false}\n";
                       const auto& fetch=pair.vertex_fetch;
                       const uint32_t sample=(pair.fragment_pipeline.persp_sample || pair.fragment_pipeline.linear_sample)?1u:0u;
                       int pipeline_status=88;
                       const bool single_depth_ms2=pair.graphics_key.num_samples==2 && pair.graphics_key.depth_samples==2;
                       const bool single_depth_sample1=pair.graphics_key.num_samples==1 && pair.graphics_key.depth_samples==1 && ((pair.graphics_key.colors[0].data_format==4 && pair.graphics_key.colors[0].number_format==7) || (pair.graphics_key.colors[0].data_format==10 && pair.graphics_key.colors[0].number_format==0)) && !pair.draw_inputs.indexed;
                       const bool single_retained_depth=pair.graphics_key.num_color_attachments==1 && pair.graphics_key.mrt_mask==1 && (single_depth_ms2 || single_depth_sample1) && draw.depth && draw.colors[0];
                       const bool indexed_depth=pair.draw_inputs.indexed && single_retained_depth && single_depth_ms2;
                       const bool single_inactive_ms2=pair.graphics_key.num_color_attachments==1 && pair.graphics_key.mrt_mask==1 && single_depth_ms2 && !draw.depth && draw.colors[0] && !first_dynamic_status && !first_dynamic.depth_test && !first_dynamic.depth_write && !first_dynamic.stencil_test;
                       const bool single_combined=single_retained_depth || single_inactive_ms2;
                       if(single_inactive_ms2){const bool initialized=std::find(indexed_ms_initialized.begin(),indexed_ms_initialized.end(),draw.colors[0])!=indexed_ms_initialized.end();
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_single_MS2_inactive_depth_admission\",\"word\":"<<pair.word<<",\"retained_MS_generation\":"<<(initialized?"true":"false")<<",\"depth_attachment_absent\":true,\"depth_test\":0,\"stencil_test\":0,\"original_key_preserved\":true,\"GPU_execution\":false}\n";
                        if(!initialized){
                         std::vector<const GraphicsStageOwner::Color*> inputs;const int input_status=graphics_pair_color_inputs(pair,inputs);if(input_status)return input_status;
                         const GraphicsStageOwner::Color* source=nullptr;for(const auto* c:inputs)if(c->slot==0)source=c;if(!source)return 112;const auto& shape=source->requirement;
                         if(shape.data_format!=10 || shape.number_format || shape.samples!=1 || shape.layers!=1 || !shape.bytes || shape.bytes>16*1024*1024 || shape.bytes!=uint64_t(shape.pitch)*shape.height*4)return 88;
                         if(!draw_snapshot_disjoint(shape.guest_address,shape.bytes,completed_color_ranges) || !draw_snapshot_disjoint(shape.guest_address,shape.bytes,completed_compute_ranges))return 88;
                         std::vector<unsigned char> before(static_cast<size_t>(shape.bytes)),after(before.size());
                         int prepared=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],before.data(),shape.bytes,uint64_t(shape.pitch)*4);
                         if(!prepared)prepared=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],0);
                         if(!prepared)prepared=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],1);
                         if(!prepared)prepared=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],after.data(),shape.bytes,uint64_t(shape.pitch)*4);
                         if(prepared)return prepared;if(before!=after)return 112;
                         indexed_ms_initialized.push_back(draw.colors[0]);
                        }
                        pipeline_status=ctr_video_graphics_mrt_pipeline_create_v1(api->context,&pair.graphics_key,&fetch.value,fetch.formats.data(),fetch.value.count,draw.layout,draw.colors.data(),1,pair.vertex->spirv.data(),static_cast<uint32_t>(pair.vertex->spirv.size()),pair.fragment->spirv.data(),static_cast<uint32_t>(pair.fragment->spirv.size()),sample,&first_pipeline);
                       }else if(single_retained_depth){
                        if(single_depth_ms2 && std::find(indexed_ms_initialized.begin(),indexed_ms_initialized.end(),draw.colors[0])==indexed_ms_initialized.end()){
                         const GraphicsStageOwner::Color* source=nullptr;for(const auto* candidate:color_sources)if(candidate->slot==0)source=candidate;
                         if(!source)return 112;const auto& shape=source->requirement;const uint64_t texel=shape.data_format==3?2u:4u;
                         if((shape.data_format!=3 && shape.data_format!=10) || shape.number_format || shape.samples!=1 || !shape.bytes || shape.bytes>16*1024*1024 || shape.bytes!=uint64_t(shape.pitch)*shape.height*texel)return 88;
                         std::vector<unsigned char> before(static_cast<size_t>(shape.bytes)),after(before.size());
                         int copied=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],before.data(),shape.bytes,uint64_t(shape.pitch)*texel);
                         if(!copied)copied=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],0);
                         if(!copied)copied=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],1);
                         if(!copied)copied=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],after.data(),shape.bytes,uint64_t(shape.pitch)*texel);
                         uint64_t differences=0;if(!copied)for(size_t n=0;n<before.size();++n)differences+=before[n]!=after[n];
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_single_depth_MS2_backing\",\"word\":"<<pair.word<<",\"status\":"<<copied<<",\"compared_bytes\":"<<shape.bytes<<",\"differences\":"<<differences<<",\"original_key_unchanged\":true,\"real_transfer_fences\":true}\n";
                         if(copied || differences)return copied?copied:112;
                         indexed_ms_initialized.push_back(draw.colors[0]);
                        }
                        pipeline_status=ctr_video_graphics_mrt_depth_pipeline_create_v1(api->context,&pair.graphics_key,&fetch.value,fetch.formats.data(),fetch.value.count,draw.layout,draw.colors.data(),1,draw.depth,pair.vertex->spirv.data(),static_cast<uint32_t>(pair.vertex->spirv.size()),pair.fragment->spirv.data(),static_cast<uint32_t>(pair.fragment->spirv.size()),sample,&first_pipeline);
                       }else if(pair.graphics_key.num_color_attachments==2 && pair.graphics_key.mrt_mask==3 && draw.depth && draw.colors[0] && draw.colors[1] && draw.colors[0]!=draw.colors[1] && pair.graphics_key.num_samples==2 && pair.graphics_key.depth_samples==2){
                        // The whole public register decoder already derived the
                        // genuine no-AMD fallback key. Never edit guest key samples.
                        for(uint32_t slot=0;slot<2;++slot){
                         const GraphicsStageOwner::Color* source=nullptr;for(const auto* candidate:color_sources)if(candidate->slot==slot)source=candidate;
                         if(!source)return 112;const auto& shape=source->requirement;const uint64_t texel=shape.data_format==3?2u:4u;
                         if((shape.data_format!=3 && shape.data_format!=10) || shape.number_format || shape.samples!=1 || !shape.bytes || shape.bytes>16*1024*1024 || shape.bytes!=uint64_t(shape.pitch)*shape.height*texel)return 88;
                         std::vector<unsigned char> prior(static_cast<size_t>(shape.bytes)),returned(prior.size());
                         const int before=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[slot],prior.data(),shape.bytes,uint64_t(shape.pitch)*texel);if(before)return before;
                         const int copied=ctr_video_color_backing_copy_v1(api->context,draw.colors[slot],0);
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_MS_backing_copy\",\"word\":"<<pair.word<<",\"slot\":"<<slot<<",\"direction\":0,\"status\":"<<copied<<",\"original_key_unchanged\":true,\"guest_writeback\":false,\"GPU_execution\":"<<(!copied?"true":"false")<<"}\n";
                         if(copied)return copied;
                         int reverse=ctr_video_color_backing_copy_v1(api->context,draw.colors[slot],1);
                         if(!reverse)reverse=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[slot],returned.data(),shape.bytes,uint64_t(shape.pitch)*texel);
                         uint64_t differences=0;if(!reverse)for(size_t n=0;n<prior.size();++n)differences+=prior[n]!=returned[n];
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_MS_sample0_roundtrip\",\"word\":"<<pair.word<<",\"slot\":"<<slot<<",\"status\":"<<reverse<<",\"compared_bytes\":"<<shape.bytes<<",\"differences\":"<<differences<<",\"averaged_resolve\":false,\"pixel_payload_persisted\":false,\"GPU_execution\":"<<(!reverse?"true":"false")<<"}\n";
                         if(reverse || differences)return reverse?reverse:112;
                        }
                        pipeline_status=ctr_video_graphics_mrt_depth_pipeline_create_v1(api->context,&pair.graphics_key,&fetch.value,fetch.formats.data(),fetch.value.count,draw.layout,draw.colors.data(),2,draw.depth,pair.vertex->spirv.data(),static_cast<uint32_t>(pair.vertex->spirv.size()),pair.fragment->spirv.data(),static_cast<uint32_t>(pair.fragment->spirv.size()),sample,&first_pipeline);
                       }else if(pair.graphics_key.num_color_attachments==2 && pair.graphics_key.mrt_mask==3 && draw.colors[0] &&
                        ((draw.colors[1] && draw.colors[0]!=draw.colors[1]) ||
                         (!draw.colors[1] && !draw.depth && !pair.draw_inputs.indexed && pair.graphics_key.num_samples==1 && pair.graphics_key.depth_samples==1 &&
                          pair.graphics_key.colors[0].data_format==10 && pair.graphics_key.colors[0].number_format==0 && pair.graphics_key.colors[0].samples==1 &&
                          !pair.graphics_key.colors[1].data_format && !pair.graphics_key.colors[1].write_mask && !pair.graphics_key.colors[1].samples)))
                        pipeline_status=ctr_video_graphics_mrt_pipeline_create_v1(api->context,&pair.graphics_key,&fetch.value,fetch.formats.data(),fetch.value.count,draw.layout,draw.colors.data(),2,pair.vertex->spirv.data(),static_cast<uint32_t>(pair.vertex->spirv.size()),pair.fragment->spirv.data(),static_cast<uint32_t>(pair.fragment->spirv.size()),sample,&first_pipeline);
                       else if(pair.graphics_key.num_color_attachments==1 && draw.colors[0])
                        pipeline_status=ctr_video_graphics_pipeline_create_v1(api->context,&pair.graphics_key,&fetch.value,fetch.formats.data(),fetch.value.count,draw.layout,draw.colors[0],pair.vertex->spirv.data(),static_cast<uint32_t>(pair.vertex->spirv.size()),pair.fragment->spirv.data(),static_cast<uint32_t>(pair.fragment->spirv.size()),sample,&first_pipeline);
                       if(inactive_attachments)
                        pipeline_status=ctr_video_graphics_no_attachment_pipeline_create_v1(api->context,&pair.graphics_key,&fetch.value,fetch.formats.data(),fetch.value.count,draw.layout,pair.vertex->spirv.data(),static_cast<uint32_t>(pair.vertex->spirv.size()),pair.fragment->spirv.data(),static_cast<uint32_t>(pair.fragment->spirv.size()),&first_pipeline);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_pipeline\",\"word\":"<<pair.word<<",\"status\":"<<pipeline_status<<",\"owned_token\":"<<(first_pipeline?"true":"false")<<",\"GPU_execution\":false}\n";
                       if(pipeline_status && pipeline_status!=88)return pipeline_status;
                       if(!pipeline_status && !first_pipeline)return 112;
                       if(!pipeline_status && inactive_attachments){
                        if(first_viewport_status || first_dynamic_status || first_draw_parameter_status || first_push_data_status || first_push_status)return 88;
                        const DrawResourceInputs* resource=nullptr;for(const auto& r:prepared_draw_resources)if(r.word==pair.word)resource=&r;if(!resource)return 112;
                        CtrGraphicsDrawV1 call{};call.version=1;call.size=sizeof(call);call.vertex_count=pair.draw_inputs.count;call.instance_count=first_draw_parameters.instance_count;call.first_vertex=first_draw_parameters.vertex_offset;call.first_instance=first_draw_parameters.instance_offset;
                        call.written_buffers=written_buffers;call.written_images=written_images;call.atomic_resources=atomic_resources;call.vertex_buffer_count=static_cast<uint32_t>(resource->vertices.size());
                        if(call.vertex_buffer_count>32 || pair.draw_inputs.indexed)return 88;for(uint32_t v=0;v<call.vertex_buffer_count;++v)call.vertex_buffers[v]=resource->vertices[v].token;
                        int gpu_status=88;bool invoked=false;
                        const int resumed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                         if(packet.word!=pair.word)return ordered_transport(packet,data);
                         return execute_owned_route(submission_owner,packet,data,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){return ordered_registers.execute_draw(data,packet.words,[&](const CtrPm4::Registers&){
                          invoked=true;gpu_status=ctr_video_graphics_no_attachment_draw_v1(api->context,first_pipeline,&first_viewports,&first_dynamic,&first_push_data,&call);return gpu_status;
                         });});
                        });
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_no_attachment_GPU_draw\",\"word\":"<<pair.word<<",\"status\":"<<gpu_status<<",\"invoked\":"<<(invoked?"true":"false")<<",\"vertices\":"<<call.vertex_count<<",\"vertex_buffers\":"<<call.vertex_buffer_count<<",\"resume_status\":"<<resumed<<",\"completed_packets\":"<<command_cursor.completed()<<",\"original_key_preserved\":true,\"real_fence\":"<<(!gpu_status?"true":"false")<<",\"GPU_execution\":"<<(!gpu_status?"true":"false")<<"}\n";
                        if(invoked && resumed && command_cursor.pending() && command_cursor.pending()->word==pair.word)rejected_graphics_word=pair.word;
                        if(gpu_status && gpu_status!=88)return gpu_status;if(resumed && resumed!=88)return resumed;
                        continue;
                       }
                       if(!pipeline_status && (pair.graphics_key.num_color_attachments!=1 || single_combined)){
                        const uint32_t color_count=single_combined?1u:2u;
                        const bool inactive_second_color=color_count==2 && !draw.colors[1] && !pair.graphics_key.colors[1].data_format && !pair.graphics_key.colors[1].write_mask && !pair.graphics_key.colors[1].samples;
                        const uint32_t active_color_count=inactive_second_color?1u:color_count;
                        if((!single_combined && pair.graphics_key.num_color_attachments!=2) || (!draw.depth && !single_inactive_ms2 && (pair.graphics_key.num_samples!=1 || pair.graphics_key.depth_samples!=1 || pair.graphics_key.z_format || first_dynamic.depth_test || first_dynamic.depth_write || first_dynamic.stencil_test)) || first_viewport_status || first_dynamic_status || first_draw_parameter_status || first_push_data_status || first_push_status)return 88;
                        const DrawResourceInputs* resource=nullptr;for(const auto& r:prepared_draw_resources)if(r.word==pair.word)resource=&r;if(!resource)return 112;
                        CtrGraphicsDrawV1 call{};call.version=1;call.size=sizeof(call);call.vertex_count=pair.draw_inputs.count;call.instance_count=first_draw_parameters.instance_count;call.first_vertex=first_draw_parameters.vertex_offset;call.first_instance=first_draw_parameters.instance_offset;
                        call.written_buffers=written_buffers;call.written_images=written_images;call.atomic_resources=atomic_resources;call.vertex_buffer_count=static_cast<uint32_t>(resource->vertices.size());
                        if(call.vertex_buffer_count>32 || (pair.draw_inputs.indexed && !indexed_depth))return 88;for(uint32_t v=0;v<call.vertex_buffer_count;++v)call.vertex_buffers[v]=resource->vertices[v].token;
                        CtrGraphicsIndexedDrawV1 indexed_call{};
                        if(indexed_depth){const int converted=prepare_indexed_call(command,call,indexed_call);if(converted)return converted;}
                        if(indexed_depth)census_log<<"{\"scope\":\"original_GnmSubmitFlip_indexed_depth_admission\",\"word\":"<<pair.word<<",\"index_count\":"<<indexed_call.index_count<<",\"index_bytes\":"<<indexed_call.index_bytes<<",\"vertex_offset\":"<<indexed_call.vertex_offset<<",\"vertex_buffers\":"<<indexed_call.vertex_buffer_count<<",\"color_count\":"<<color_count<<",\"color_samples\":"<<pair.graphics_key.num_samples<<",\"depth_samples\":"<<pair.graphics_key.depth_samples<<",\"stencil_test\":"<<first_dynamic.stencil_test<<",\"depth_test\":"<<first_dynamic.depth_test<<",\"depth_write\":"<<first_dynamic.depth_write<<",\"original_key_preserved\":true,\"GPU_execution\":false}\n";
                        int gpu_status=88;bool invoked=false;
                        const int resumed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                         if(packet.word!=pair.word)return ordered_transport(packet,data);
                         return execute_owned_route(submission_owner,packet,data,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){return ordered_registers.execute_draw(data,packet.words,[&](const CtrPm4::Registers&){
                          const int writers=color_coherence.begin_many(draw.colors.data(),active_color_count,pair.word);if(writers)return writers;
                          invoked=true;gpu_status=indexed_depth?ctr_video_graphics_indexed_depth_draw_v1(api->context,first_pipeline,&first_viewports,&first_dynamic,&first_push_data,&indexed_call):draw.depth?ctr_video_graphics_mrt_depth_draw_v1(api->context,first_pipeline,&first_viewports,&first_dynamic,&first_push_data,&call):ctr_video_graphics_mrt_draw_v1(api->context,first_pipeline,&first_viewports,&first_dynamic,&first_push_data,&call);if(gpu_status)return gpu_status;
                          if(draw.depth){const int marked=pending_depth_guest_owners.mark(draw.depth,pair.word);if(marked)return marked;}
                          for(uint32_t slot=0;slot<color_count;++slot){if(slot==1 && inactive_second_color)continue;const int copied=pair.graphics_key.num_samples==2?ctr_video_color_backing_copy_v1(api->context,draw.colors[slot],1):0;if(copied)return copied;const GraphicsStageOwner::Color* source=nullptr;for(const auto* candidate:color_sources)if(candidate->slot==slot)source=candidate;if(!source)return 112;const auto& shape=source->requirement;const uint64_t pitch=uint64_t(shape.pitch)*(shape.data_format==3?2:4),bytes=pitch*shape.height;if(!bytes || bytes>32*1024*1024)return 88;std::vector<unsigned char> pixels(static_cast<size_t>(bytes));const int read=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[slot],pixels.data(),bytes,pitch);if(read)return read;
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_MRT_GPU_readback\",\"word\":"<<pair.word<<",\"slot\":"<<slot<<",\"status\":0,\"bytes\":"<<bytes<<",\"sample0_reverse_fenced\":"<<(pair.graphics_key.num_samples==2?"true":"false")<<",\"pixel_payload_persisted\":false,\"GPU_execution\":true}\n";
                          if(shape.data_format==10 || shape.data_format==3 || (shape.data_format==4 && shape.number_format==7)){ComputeGuestWriteback writer;writer.address=shape.guest_address;writer.initial=source->initial.snapshot;for(const auto& generation:refreshed_compute_colors)if(generation.token==draw.colors[slot] && generation.address==shape.guest_address && generation.bytes.size()==shape.bytes)writer.initial=generation.bytes;const int baseline=published_color_generations.baseline(shape.guest_address,shape.bytes,completed_color_ranges,completed_color_ranges.size(),writer.initial);if(baseline && baseline!=62)return baseline;if(writer.initial.size()!=shape.bytes)return 88;ColorGuestDownload download{api->context,draw.colors[slot],shape};const int published=writer.commit(memory,ColorGuestDownload::download,&download);
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_guest_writeback\",\"word\":"<<pair.word<<",\"slot\":"<<slot<<",\"status\":"<<published<<",\"guest_bytes\":"<<writer.initial.size()<<",\"actual_GPU_retiled_generation\":"<<(writer.completed?"true":"false")<<",\"poisoned\":"<<(writer.poisoned?"true":"false")<<",\"pixel_payload_persisted\":false}\n";if(published)return published;if(shape.data_format==10)log_rgba8_occupancy(census_log,pair.word,writer.address,writer.initial,"completed_MRT_writer");const int recorded=published_color_generations.publish(writer,completed_color_ranges.size()+active_color_count);if(recorded)return recorded;}
                          const int completed=color_coherence.complete(draw.colors[slot],pair.word);if(completed)return completed;
                          for(const auto& image:live_images)for(const auto& descriptor:image.stage->descriptors){
                           if(descriptor.kind!=1 || descriptor.sharp!=image.sharp || descriptor.image_status || descriptor.image_extent_status || descriptor.image.guest_address!=shape.guest_address || descriptor.image_extent.bytes!=shape.bytes)continue;
                           if(descriptor.image.data_format!=shape.data_format || descriptor.image.number_format!=shape.number_format || descriptor.image.pitch!=shape.pitch || descriptor.image.tile_mode!=shape.tile_mode || descriptor.image.samples!=1)continue;
                           const int refreshed=color_coherence.synchronize(draw.colors[slot],image.token,[&](uint64_t c,uint64_t t){return transfer_color_sampled_view(api->context,c,t,shape,descriptor.image);});
                           uint64_t compared=0,differences=0,padding=0;const int checked=refreshed?refreshed:compare_color_sampled_view(api->context,draw.colors[slot],image.token,shape,descriptor.image,compared,differences,padding);
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_MRT_writer_to_sampled\",\"word\":"<<pair.word<<",\"slot\":"<<slot<<",\"sharp\":"<<image.sharp<<",\"status\":"<<checked<<",\"compared_pixel_bytes\":"<<compared<<",\"pixel_differences\":"<<differences<<",\"padding_differences\":"<<padding<<",\"real_transfer_fence\":"<<(!checked?"true":"false")<<",\"pixel_payload_persisted\":false}\n";
                           if(checked)return checked;
                           std::vector<unsigned char> current(static_cast<size_t>(shape.bytes));const int guest_read=image_guest_copy(memory,shape.guest_address,current.data(),current.size(),false);if(guest_read)return guest_read;
                           refreshed_color_samples.push_back({image.stage,image.sharp,shape.guest_address,std::move(current)});
                          }
                         }
                          return 0;
                         });});
                        });
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_MRT_GPU_draw\",\"word\":"<<pair.word<<",\"status\":"<<gpu_status<<",\"invoked\":"<<(invoked?"true":"false")<<",\"resume_status\":"<<resumed<<",\"completed_packets\":"<<command_cursor.completed()<<",\"GPU_execution\":"<<(!gpu_status?"true":"false")<<"}\n";
                        if(!gpu_status){for(uint32_t slot=0;slot<color_count;++slot){if(slot==1 && inactive_second_color)continue;const GraphicsStageOwner::Color* source=nullptr;for(const auto* c:color_sources)if(c->slot==slot)source=c;if(!source)return 112;completed_color_ranges.push_back({source->requirement.guest_address,source->requirement.bytes});if(source->requirement.data_format!=10 && source->requirement.data_format!=3 && !(source->requirement.data_format==4 && source->requirement.number_format==7))pending_color_guest_ranges.push_back({source->requirement.guest_address,source->requirement.bytes});}}
                        if(invoked && resumed && command_cursor.pending() && command_cursor.pending()->word==pair.word)rejected_graphics_word=pair.word;
                        if(gpu_status && gpu_status!=88)return gpu_status;if(resumed && resumed!=88)return resumed;
                        continue;
                       }
                       if(!pipeline_status && !first_viewport_status && !first_dynamic_status && !first_dynamic_preflight_status && !first_draw_parameter_status && !first_push_data_status && !first_push_status){
                        const DrawResourceInputs* resource=nullptr;for(const auto& r:prepared_draw_resources)if(r.word==pair.word)resource=&r;
                        if(!resource)return 112;
                        CtrGraphicsDrawV1 call{};call.version=1;call.size=sizeof(call);call.vertex_count=pair.draw_inputs.count;call.instance_count=first_draw_parameters.instance_count;call.first_vertex=first_draw_parameters.vertex_offset;call.first_instance=first_draw_parameters.instance_offset;
                        call.written_buffers=written_buffers;call.written_images=written_images;call.atomic_resources=atomic_resources;call.vertex_buffer_count=static_cast<uint32_t>(resource->vertices.size());
                        if(call.vertex_buffer_count>32)return 88;
                        for(uint32_t v=0;v<call.vertex_buffer_count;++v)call.vertex_buffers[v]=resource->vertices[v].token;
                        CtrGraphicsIndexedDrawV1 indexed_color_call{};
                        if(pair.draw_inputs.indexed){const int converted=prepare_indexed_call(command,call,indexed_color_call);if(converted)return converted;}
                        for(uint32_t v=0;v<fetch.value.count;++v){const auto& input=fetch.value.inputs[v];census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_GPU_vertex_input\",\"word\":"<<pair.word<<",\"binding\":"<<input.binding<<",\"format\":"<<fetch.formats[v]<<",\"offset\":"<<input.offset<<",\"stride\":"<<input.stride<<",\"input_rate\":"<<input.input_rate<<",\"bytes\":"<<input.buffer_size<<",\"GPU_execution\":false}\n";}
                        std::vector<const GraphicsStageOwner::Color*> draw_color_inputs;const int color_inputs_status=graphics_pair_color_inputs(pair,draw_color_inputs);if(color_inputs_status)return color_inputs_status;const GraphicsStageOwner::Color* color_input=nullptr;for(const auto* c:draw_color_inputs)if(c->slot==0)color_input=c;
                        if(!color_input || (color_input->requirement.data_format!=10 && !(color_input->requirement.data_format==4 && color_input->requirement.number_format==7)) || color_input->requirement.samples!=1)return 88;
                        if(pair.graphics_key.num_samples==2){
                         const bool initialized=std::find(indexed_ms_initialized.begin(),indexed_ms_initialized.end(),draw.colors[0])!=indexed_ms_initialized.end();
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_single_MS2_inactive_depth_admission\",\"word\":"<<pair.word<<",\"retained_MS_generation\":"<<(initialized?"true":"false")<<",\"depth_attachment_absent\":"<<(!draw.depth?"true":"false")<<",\"depth_test\":"<<first_dynamic.depth_test<<",\"stencil_test\":"<<first_dynamic.stencil_test<<",\"original_key_preserved\":true,\"GPU_execution\":false}\n";
                         if(!initialized || draw.depth || first_dynamic.depth_test || first_dynamic.depth_write || first_dynamic.stencil_test || pair.graphics_key.depth_samples!=2)return 88;
                        }
                        const auto& color_shape=color_input->requirement;const uint64_t read_pitch=uint64_t(color_shape.pitch)*4,read_bytes=read_pitch*color_shape.height;
                        if(!read_bytes || read_bytes>32*1024*1024)return 88;
                        std::vector<unsigned char> before_pixels(static_cast<size_t>(read_bytes)),after_pixels(static_cast<size_t>(read_bytes));
                        const int before_read=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],before_pixels.data(),read_bytes,read_pitch);if(before_read)return before_read;
                        int gpu_status=88;bool invoked=false;
                        const int resumed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                         if(packet.word!=pair.word)return ordered_transport(packet,data);
                         return execute_owned_route(submission_owner,packet,data,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){
                          return ordered_registers.execute_draw(data,packet.words,[&](const CtrPm4::Registers&){const int writer=color_coherence.begin(draw.colors[0],pair.word);if(writer)return writer;invoked=true;gpu_status=pair.draw_inputs.indexed?ctr_video_graphics_indexed_color_draw_v1(api->context,first_pipeline,draw.colors[0],&first_viewports,&first_dynamic,&first_push_data,&indexed_color_call):ctr_video_graphics_draw_v1(api->context,first_pipeline,draw.colors[0],&first_viewports,&first_dynamic,&first_push_data,&call);if(!gpu_status){const int completed=color_coherence.complete(draw.colors[0],pair.word);if(completed)return completed;}return gpu_status;});
                         });
                        });
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_GPU_draw\",\"word\":"<<pair.word<<",\"status\":"<<gpu_status<<",\"invoked\":"<<(invoked?"true":"false")<<",\"vertices\":"<<call.vertex_count<<",\"instances\":"<<call.instance_count<<",\"vertex_buffers\":"<<call.vertex_buffer_count<<",\"completed_packets\":"<<command_cursor.completed()<<",\"resume_status\":"<<resumed<<",\"real_fence\":"<<(!gpu_status?"true":"false")<<",\"GPU_execution\":"<<(!gpu_status?"true":"false")<<"}\n";
                        if(!gpu_status){
                         if(pair.graphics_key.num_samples==2){const int copied=ctr_video_color_backing_copy_v1(api->context,draw.colors[0],1);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_single_MS2_color_sample0_reverse\",\"word\":"<<pair.word<<",\"status\":"<<copied<<",\"averaged_resolve\":false,\"before_guest_publication\":true,\"GPU_execution\":"<<(!copied?"true":"false")<<"}\n";if(copied)return copied;
                         }
                         completed_color_ranges.push_back({color_shape.guest_address,color_shape.bytes});const int after_read=ctr_video_color_attachment_readback_linear_v1(api->context,draw.colors[0],after_pixels.data(),read_bytes,read_pitch);if(after_read)return after_read;
                         uint64_t changed=0;for(size_t b=0;b<after_pixels.size();++b)if(before_pixels[b]!=after_pixels[b])++changed;
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_GPU_readback\",\"word\":"<<pair.word<<",\"status\":0,\"bytes\":"<<read_bytes<<",\"changed_bytes\":"<<changed<<",\"writer_generation_completed\":true,\"pixel_payload_persisted\":false,\"GPU_execution\":true}\n";
                         pending_color_guest_ranges.push_back({color_shape.guest_address,color_shape.bytes});
                         ComputeGuestWriteback color_writeback;color_writeback.address=color_shape.guest_address;color_writeback.initial=color_input->initial.snapshot;
                         const int prior_publication=published_color_generations.baseline(color_shape.guest_address,color_shape.bytes,completed_color_ranges,completed_color_ranges.size()-1,color_writeback.initial);
                         if(prior_publication && prior_publication!=62)return prior_publication;
                         if(color_writeback.initial.size()!=color_shape.bytes)return 88;
                         ColorGuestDownload color_download{api->context,draw.colors[0],color_shape};
                         const int color_published=color_writeback.commit(memory,ColorGuestDownload::download,&color_download);
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_guest_writeback\",\"word\":"<<pair.word<<",\"status\":"<<color_published<<",\"guest_bytes\":"<<color_writeback.initial.size()<<",\"actual_GPU_retiled_generation\":"<<(color_writeback.completed?"true":"false")<<",\"poisoned\":"<<(color_writeback.poisoned?"true":"false")<<",\"pixel_payload_persisted\":false}\n";
                         if(color_published)return color_published;if(color_shape.data_format==10)log_rgba8_occupancy(census_log,pair.word,color_writeback.address,color_writeback.initial,"completed_color_writer");const int recorded=published_color_generations.publish(color_writeback,completed_color_ranges.size());if(recorded)return recorded;pending_color_guest_ranges.pop_back();
                         if(color_shape.tile_mode==10 && color_shape.data_format==10 && color_shape.number_format==0 && color_shape.samples==1 && color_shape.pitch==1920 && color_shape.height==1088 && color_shape.bytes==8355840){last_display_color=color_writeback;last_display_compute_epoch=completed_compute_ranges.size();}
                         // Refresh existing sampled aliases from the completed original
                         // GPU writer, not their prefetched initial CPU payload. Each
                         // conversion is itself synchronous; unsupported views stay pending.
                         for(const auto& image:live_images)for(const auto& descriptor:image.stage->descriptors){
                          if(descriptor.kind!=1 || descriptor.sharp!=image.sharp || descriptor.image.guest_address!=color_shape.guest_address)continue;
                          const int refreshed=color_coherence.synchronize(draw.colors[0],image.token,[&](uint64_t c,uint64_t t){return transfer_color_sampled_view(api->context,c,t,color_shape,descriptor.image);});
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_completed_writer_to_sampled\",\"word\":"<<pair.word<<",\"image_sharp\":"<<image.sharp<<",\"status\":"<<refreshed<<",\"real_game_writer_completed\":true,\"real_transfer_fence\":"<<(!refreshed?"true":"false")<<",\"guest_writeback\":false,\"GPU_execution\":true}\n";
                          if(!refreshed){uint64_t compared=0,differences=0,padding=0;const int checked=compare_color_sampled_view(api->context,draw.colors[0],image.token,color_shape,descriptor.image,compared,differences,padding);
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_completed_writer_sampled_readback\",\"word\":"<<pair.word<<",\"status\":"<<checked<<",\"compared_pixel_bytes\":"<<compared<<",\"pixel_differences\":"<<differences<<",\"padding_differences\":"<<padding<<",\"real_game_writer_completed\":true,\"pixel_payload_persisted\":false,\"GPU_execution\":true}\n";if(checked)return checked;
                           if(!descriptor.image_extent_status && descriptor.image_extent.bytes==color_shape.bytes && descriptor.image.data_format==color_shape.data_format && descriptor.image.number_format==color_shape.number_format && descriptor.image.tile_mode==color_shape.tile_mode){
                            std::vector<unsigned char> current(static_cast<size_t>(color_shape.bytes));const int guest_read=image_guest_copy(memory,color_shape.guest_address,current.data(),current.size(),false);if(guest_read)return guest_read;
                            if(current!=color_writeback.initial)return 91;
                            refreshed_color_samples.push_back({image.stage,image.sharp,color_shape.guest_address,std::move(current)});
                           }
                          }
                          if(refreshed && refreshed!=88 && refreshed!=91)return refreshed;
                         }
                        }
                        if(invoked && resumed && command_cursor.pending() && command_cursor.pending()->word==pair.word)rejected_graphics_word=pair.word;
                        if(gpu_status && gpu_status!=88)return gpu_status;if(resumed && resumed!=88)return resumed;
                       }

                      }
                      if(command_cursor.completed()==completed_before)break;
                     }
                     if(const auto* pending=command_cursor.pending()){
                      for(const auto& joined:vertex_attachment_joins){const auto& vertex=*joined.vertex;if(vertex.word!=pending->word || vertex.word==rejected_graphics_word)continue;
                       bool validated=false;const int identity=execute_owned_route(submission_owner,*pending,words.data()+pending->word,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){validated=true;return 88;});if(identity!=88 || !validated)return identity==88?112:(identity?identity:112);
                       GraphicsAuxiliaryOwner auxiliary;int auxiliary_status=88;
                       if(vertex.graphics_key.primitive_type==17 && !vertex.graphics_key.mrt_mask)auxiliary_status=compile_owned_vertex_auxiliary(auxiliary);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_auxiliary\",\"word\":"<<vertex.word<<",\"status\":"<<auxiliary_status<<",\"control_words\":"<<auxiliary.control.size()<<",\"evaluation_words\":"<<auxiliary.evaluation.size()<<",\"owned_after_DLL_unload\":true,\"fragment_absent\":true,\"GPU_execution\":false}\n";
                       if(auxiliary_status && auxiliary_status!=88)return auxiliary_status;
                       uint64_t rect_pipeline=0;PipelineCleanup rect_cleanup{api->context,rect_pipeline};
                       const bool rect_no_attachments=!joined.depth && !(ordered_registers.state().words[0xa200]&7u) && !(ordered_registers.state().words[0xa000]&0x21fu) && !vertex.graphics_key.mrt_mask && !vertex.graphics_key.num_color_attachments;
                       const int rect_status=auxiliary_status?auxiliary_status:(rect_no_attachments?ctr_video_graphics_rect_no_attachment_pipeline_create_v1(api->context,&vertex.graphics_key,joined.layout,vertex.vertex->spirv.data(),static_cast<uint32_t>(vertex.vertex->spirv.size()),auxiliary.control.data(),static_cast<uint32_t>(auxiliary.control.size()),auxiliary.evaluation.data(),static_cast<uint32_t>(auxiliary.evaluation.size()),&rect_pipeline):(!joined.depth?88:ctr_video_graphics_rect_depth_pipeline_create_v1(api->context,&vertex.graphics_key,joined.layout,joined.depth,vertex.vertex->spirv.data(),static_cast<uint32_t>(vertex.vertex->spirv.size()),auxiliary.control.data(),static_cast<uint32_t>(auxiliary.control.size()),auxiliary.evaluation.data(),static_cast<uint32_t>(auxiliary.evaluation.size()),&rect_pipeline)));
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_pipeline\",\"word\":"<<vertex.word<<",\"status\":"<<rect_status<<",\"owned_token\":"<<(rect_pipeline?"true":"false")<<",\"fragment_absent\":true,\"GPU_execution\":false}\n";
                       if(rect_status && rect_status!=88)return rect_status;

                       CtrGraphicsViewportStateV1 viewports{};CtrGraphicsDynamicStateV1 dynamic{};CtrGraphicsDrawParametersV1 parameters{};
                       const int viewport=decode_owned_viewports(ordered_registers.state(),vertex.profile,viewports);
                       const int dynamics=decode_owned_dynamic_state(ordered_registers.state(),vertex.draw_inputs.indexed,dynamic);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_dynamic\",\"word\":"<<vertex.word<<",\"decode_status\":"<<dynamics<<",\"depth_test\":"<<dynamic.depth_test<<",\"stencil_test\":"<<dynamic.stencil_test<<",\"depth_write\":"<<dynamic.depth_write<<",\"depth_compare\":"<<dynamic.depth_compare<<",\"depth_bounds_test\":"<<dynamic.depth_bounds_test<<",\"depth_bias\":"<<dynamic.depth_bias<<",\"bias_constant\":"<<dynamic.bias_constant<<",\"bias_slope\":"<<dynamic.bias_slope<<",\"bias_clamp\":"<<dynamic.bias_clamp<<",\"primitive_restart\":"<<dynamic.primitive_restart<<",\"rasterizer_discard\":"<<dynamic.rasterizer_discard<<",\"line_width\":"<<dynamic.line_width<<",\"GPU_execution\":false}\n";
                       for(uint32_t v=0;!viewport && v<viewports.count;++v){const auto& value=viewports.viewports[v];
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_viewport\",\"word\":"<<vertex.word<<",\"index\":"<<v<<",\"x\":"<<value.x<<",\"y\":"<<value.y<<",\"width\":"<<value.width<<",\"height\":"<<value.height<<",\"min_depth\":"<<value.minDepth<<",\"max_depth\":"<<value.maxDepth<<",\"scissor_x\":"<<viewports.scissors[v].offset.x<<",\"scissor_y\":"<<viewports.scissors[v].offset.y<<",\"scissor_width\":"<<viewports.scissors[v].extent.width<<",\"scissor_height\":"<<viewports.scissors[v].extent.height<<",\"GPU_execution\":false}\n";}
                       auto bank=std::make_unique<CtrPm4::Registers>();int offsets=prepare_draw_execution_bank(vertex.packet.data(),vertex.packet.size(),ordered_registers.state(),*bank);
                       if(!offsets)offsets=decode_owned_draw_parameters(*bank,vertex.pipeline,*vertex.vertex,parameters);
                       const int selected=viewport?viewport:(dynamics?dynamics:ctr_video_graphics_dynamic_preflight_v1(api->context,&viewports,&dynamic));
                       // An absent fragment stage contributes no userdata/buffers.
                       // The genuine disabled ClipPlanes binding remains a real
                       // nullDescriptor, never fabricated guest memory.
                       CtrGraphicsBindingContractV1 absent_fragment{};absent_fragment.version=1;absent_fragment.size=sizeof(absent_fragment);
                       std::vector<CtrGraphicsBufferWriteV1> null_writes;int push_status=graphics_vertex_only_null_writes(vertex,null_writes);
                       std::vector<GraphicsBufferWrite> push_writes;
                       if(!push_status)for(const auto& write:null_writes)push_writes.push_back({write.binding,write.buffer_slot,vertex.bindings.descriptors[0].sharp_index,6,true,true,0,UINT64_MAX});
                       GraphicsPushResources resources{};CtrGraphicsPushDataV1 push{};
                       if(!push_status)push_status=prepare_push_resources(absent_fragment,vertex.bindings,push_writes,resources);
                       std::array<uint32_t,40> buffer_offsets{};
                       if(!push_status)push_status=decode_owned_push_data(ordered_registers.state(),absent_fragment,vertex.bindings,buffer_offsets.data(),resources.buffer_count,push);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_pending_push_data\",\"word\":"<<vertex.word<<",\"status\":"<<push_status<<",\"buffer_count\":"<<resources.buffer_count<<",\"userdata_count\":"<<resources.userdata_count<<",\"owned_payload\":"<<(push.version==1?"true":"false")<<",\"fragment_absent\":true,\"null_clip_planes\":true,\"GPU_execution\":false}\n";
                       if(push_status && push_status!=88)return push_status;
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_pending_inputs\",\"word\":"<<vertex.word<<",\"viewport_status\":"<<viewport<<",\"viewports\":"<<viewports.count<<",\"dynamic_status\":"<<dynamics<<",\"selected_dynamic_status\":"<<selected<<",\"draw_parameters_status\":"<<offsets<<",\"vertex_offset\":"<<parameters.vertex_offset<<",\"instance_offset\":"<<parameters.instance_offset<<",\"instances\":"<<parameters.instance_count<<",\"depth_test\":"<<dynamic.depth_test<<",\"stencil_test\":"<<dynamic.stencil_test<<",\"depth_token\":"<<(joined.depth?"true":"false")<<",\"depth_initialized\":"<<(joined.initialized?"true":"false")<<",\"depth_only_preflight_deferred_to_draw\":true,\"GPU_execution\":false}\n";
                       for(int status:{viewport,dynamics,offsets,selected})if(status && status!=88)return status;
                       if(rect_pipeline && !viewport && !dynamics && !offsets && !push_status && !vertex.draw_inputs.indexed){
                        CtrGraphicsDrawV1 call{};call.version=1;call.size=sizeof(call);call.vertex_count=vertex.draw_inputs.count;call.instance_count=parameters.instance_count;call.first_vertex=parameters.vertex_offset;call.first_instance=parameters.instance_offset;
                        int gpu=88;bool invoked=false;
                        const int resumed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                         if(packet.word!=vertex.word)return ordered_transport(packet,data);
                         return execute_owned_route(submission_owner,packet,data,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){return ordered_registers.execute_draw(data,packet.words,[&](const CtrPm4::Registers&){invoked=true;
                          if(joined.depth && std::find(cross_submission_depths.begin(),cross_submission_depths.end(),joined.depth)!=cross_submission_depths.end()){
                           const int restored=ctr_video_depth_attachment_restore_v1(api->context,joined.depth);
                           census_log<<"{\"scope\":\"retained_depth_ordered_writer_restore\",\"word\":"<<vertex.word<<",\"token\":"<<joined.depth<<",\"status\":"<<restored<<",\"before_original_draw\":true,\"CPU_guard_removed\":false}\n";
                           if(restored){gpu=restored;return restored;}
                          }
                          gpu=rect_no_attachments?ctr_video_graphics_no_attachment_draw_v1(api->context,rect_pipeline,&viewports,&dynamic,&push,&call):ctr_video_graphics_rect_depth_draw_v1(api->context,rect_pipeline,joined.depth,&viewports,&dynamic,&push,&call);if(!gpu && joined.depth){const int marked=pending_depth_guest_owners.mark(joined.depth,vertex.word);if(marked)return marked;}return gpu;});});
                        });
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_GPU_draw\",\"word\":"<<vertex.word<<",\"status\":"<<gpu<<",\"invoked\":"<<(invoked?"true":"false")<<",\"vertices\":"<<call.vertex_count<<",\"instances\":"<<call.instance_count<<",\"completed_packets\":"<<command_cursor.completed()<<",\"resume_status\":"<<resumed<<",\"real_fence\":"<<(!gpu?"true":"false")<<",\"guest_depth_writeback\":false,\"GPU_execution\":"<<(!gpu?"true":"false")<<"}\n";
                        if(gpu==88 && invoked)rejected_graphics_word=vertex.word;
                        if(gpu && gpu!=88)return gpu;if(resumed && resumed!=88)return resumed;
                        // Observe genuine D16 GPU contents without granting guest
                        // tiling/publication authority or retiring the dirty owner.
                        if(!gpu && joined.depth && vertex.graphics_key.z_format==1 && vertex.graphics_key.depth_samples==1){
                         const DepthAllocationObservation* allocation=nullptr;for(const auto& candidate:depth_allocations)if(candidate.word==vertex.word){if(allocation)return 112;allocation=&candidate;}
                         if(!allocation || !allocation->extent_valid || allocation->parameters.stencil_format)return 88;
                         const uint64_t pitch=uint64_t(allocation->parameters.width)*2;
                         if(!pitch || pitch>32*1024*1024 || !allocation->parameters.height || allocation->parameters.height>(32*1024*1024)/pitch)return 60;
                         const uint64_t bytes=pitch*allocation->parameters.height;
                         std::vector<unsigned char> linear(static_cast<size_t>(bytes)+32,0xa5);
                         int downloaded=ctr_video_depth_d16_read_linear_v1(api->context,joined.depth,linear.data()+16,bytes,pitch);
                         bool guards=std::all_of(linear.begin(),linear.begin()+16,[](unsigned char x){return x==0xa5;}) && std::all_of(linear.end()-16,linear.end(),[](unsigned char x){return x==0xa5;});
                         uint16_t minimum=UINT16_MAX,maximum=0;uint64_t differing=0;
                         if(!downloaded){uint16_t first=0;std::memcpy(&first,linear.data()+16,2);for(uint64_t offset=0;offset<bytes;offset+=2){uint16_t value=0;std::memcpy(&value,linear.data()+16+offset,2);minimum=std::min(minimum,value);maximum=std::max(maximum,value);differing+=value!=first;}}
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_D16_linear_download\",\"word\":"<<vertex.word<<",\"status\":"<<downloaded<<",\"width\":"<<allocation->parameters.width<<",\"height\":"<<allocation->parameters.height<<",\"linear_pitch\":"<<pitch<<",\"linear_bytes\":"<<bytes<<",\"guards_unchanged\":"<<(guards?"true":"false")<<",\"minimum\":"<<minimum<<",\"maximum\":"<<maximum<<",\"pixels_different_from_first\":"<<differing<<",\"dirty_owners_retained\":"<<pending_depth_guest_owners.size()<<",\"actual_GPU_download\":"<<(!downloaded?"true":"false")<<",\"guest_writeback\":false,\"pixel_payload_persisted\":false}\n";
                         if(!guards)return 112;if(downloaded && downloaded!=88)return downloaded;
                        }
                       }
                       if(rect_pipeline){const int released=ctr_video_graphics_pipeline_release_v1(api->context,rect_pipeline);
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_pipeline_release\",\"word\":"<<vertex.word<<",\"status\":"<<released<<",\"before_layouts\":true}\n";
                        if(released)return released;rect_pipeline=0;}

                      }

                      pending=command_cursor.pending();if(!pending)return 112;
                      if(pending->opcode==0x15){
                       while(pending && pending->opcode==0x15){
                        std::pair<uint32_t,std::shared_ptr<ComputeStageOwner>> compiled{static_cast<uint32_t>(pending->word),{}};
                        for(const auto& owned:compiled_computes)if(owned.first==compiled.first)compiled.second=owned.second;
                        if(!compiled.second){
                         const GraphicsSubmissionOwner::Compute* binary=nullptr;for(const auto& owned:submission_owner.computes)if(owned.word==compiled.first)binary=&owned;
                         if(!binary || !graphics_inputs || profile_status)return 88;std::string reason;
                         const auto inputs=graphics_inputs->api();const auto ranges=graphics_inputs->ranges();
                         const int translated=execute_owned_route(submission_owner,*pending,words.data()+pending->word,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){return compile_owned_compute(inputs,ranges,*binary,device_profile,compiled.second,reason);});
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_compiled_on_demand\",\"word\":"<<compiled.first<<",\"status\":"<<translated<<",\"diagnostic\":"<<graphics_json_text(reason)<<",\"resources\":"<<(compiled.second?compiled.second->resource_metadata:"[]")<<",\"GPU_execution\":false}\n";
                         if(translated==88)break;if(translated)return translated;if(!compiled.second)return 112;
                         compiled_computes.push_back(compiled);
                        }
                        uint64_t compute_layout=0,compute_pipeline=0,compute_buffer=0,compute_image=0,compute_sampled=0;
                        struct SampledCleanup{void* context;uint64_t& token;~SampledCleanup(){if(token)ctr_video_sampled_image_release_v1(context,token);}}sampled_cleanup{api->context,compute_sampled};
                        struct ComputeBufferCleanup{void* context;uint64_t& token;uint64_t& image;~ComputeBufferCleanup(){if(token)ctr_video_buffer_release_v1(context,token);if(image)ctr_video_sampled_image_release_v1(context,image);}}buffer_cleanup{api->context,compute_buffer,compute_image};
                        struct ComputeCleanup{void* context;uint64_t& layout;uint64_t& pipeline;~ComputeCleanup(){if(pipeline)ctr_video_compute_pipeline_release_v1(context,pipeline);if(layout)ctr_video_graphics_layout_release_v1(context,layout);}}cleanup{api->context,compute_layout,compute_pipeline};
                        const int created=execute_owned_route(submission_owner,*pending,words.data()+pending->word,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){
                         int result=ctr_video_compute_layout_create_v1(api->context,&compiled.second->bindings,&compute_layout);
                         if(!result)result=ctr_video_compute_pipeline_create_v1(api->context,compute_layout,compiled.second->stage->spirv.data(),static_cast<uint32_t>(compiled.second->stage->spirv.size()),&compute_pipeline);return result;
                        });
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_pipeline\",\"word\":"<<pending->word<<",\"status\":"<<created<<",\"spirv_words\":"<<compiled.second->stage->spirv.size()<<",\"resources\":"<<compiled.second->resource_metadata<<",\"push_status\":"<<compiled.second->push_status<<",\"dedicated_buffer_offset_zero_only\":true,\"GPU_execution\":false}\n";
                        if(created)return created;
                        // Bounded reached dispatch resources: real owned16B or32B read-only buffer.
                        // This prepares one descriptor, not storage-image or dispatch authority.
                        if(compiled.second->stage->buffers.size()!=1)return 88;
                        const auto& captured=compiled.second->stage->buffers.front();
                        if((captured.bytes!=16 && captured.bytes!=32) || captured.snapshot.size()!=captured.bytes || captured.capture_status || captured.release_status || captured.lease_end_status)return 88;
                        std::vector<unsigned char> current(captured.snapshot.size());int prepared=memory.read(memory.context,captured.address,current.data(),current.size());
                        if(!prepared && !std::equal(current.begin(),current.end(),captured.snapshot.begin()))prepared=88;
                        if(!prepared)prepared=ctr_video_buffer_create_v1(api->context,captured.snapshot.data(),captured.snapshot.size(),1,&compute_buffer);
                        CtrGraphicsBufferWriteV1 buffer_write{};unsigned buffer_matches=0;
                        for(uint32_t k=0;k<compiled.second->bindings.descriptor_count;++k){const auto& binding=compiled.second->bindings.descriptors[k];if(binding.kind==0 && binding.sharp_index==captured.sharp){buffer_write={binding.binding,binding.buffer_slot,binding.storage,0,compute_buffer};++buffer_matches;}}
                        if(!prepared && buffer_matches!=1)prepared=112;
                        if(!prepared)prepared=ctr_video_graphics_layout_buffers_v1(api->context,compute_layout,&buffer_write,1);
                        std::vector<unsigned char> initial(captured.snapshot.size());if(!prepared)prepared=ctr_video_buffer_copy_initial_v1(api->context,compute_buffer,initial.data(),initial.size());
                        if(!prepared && initial!=current)prepared=112;
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_buffer\",\"word\":"<<pending->word<<",\"status\":"<<prepared<<",\"bytes\":"<<captured.bytes<<",\"descriptor_matches\":"<<buffer_matches<<",\"CPU_snapshot_rechecked\":true,\"initial_copy_compared\":"<<(!prepared?"true":"false")<<",\"GPU_execution\":false}\n";
                        if(prepared)return prepared;
                        // Compiler-derived image roles: exactly one written storage image
                        // and at most one read-only sampled source; never substitute roles.
                        const GraphicsStageOwner::Descriptor* storage_owner=nullptr;const GraphicsStageOwner::Descriptor* sampled_owner=nullptr;
                        for(const auto& image:compiled.second->stage->descriptors){unsigned roles=0;bool written=false;
                         for(uint32_t k=0;k<compiled.second->bindings.descriptor_count;++k){const auto& binding=compiled.second->bindings.descriptors[k];if(binding.kind==1 && binding.sharp_index==image.sharp){++roles;written=binding.storage!=0;}}
                         if(roles!=1)return 112;if(written){if(storage_owner)return 88;storage_owner=&image;}else{if(sampled_owner)return 88;sampled_owner=&image;}
                        }
                        for(uint32_t k=0;k<compiled.second->stage->resources.binding_count;++k){const auto& binding=compiled.second->stage->resources.bindings[k];if(binding.kind==0 && (binding.flags&1u))return 88;}
                        if(!storage_owner)return 88;const auto& storage_image=*storage_owner;
                        const int storage_created=storage_image.image_status?storage_image.image_status:(storage_image.image_extent_status?storage_image.image_extent_status:ctr_video_storage_image_create_v1(api->context,&storage_image.image,&compute_image));
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_storage_allocation\",\"status\":"<<storage_created<<",\"word\":"<<pending->word<<",\"owned_token\":"<<(compute_image?"true":"false")<<",\"contents_initialized\":false,\"GPU_execution\":false}\n";
                        if(storage_created==88){
                         int closed=ctr_video_compute_pipeline_release_v1(api->context,compute_pipeline);if(closed)return closed;compute_pipeline=0;
                         closed=ctr_video_graphics_layout_release_v1(api->context,compute_layout);if(closed)return closed;compute_layout=0;
                         closed=ctr_video_buffer_release_v1(api->context,compute_buffer);if(closed)return closed;compute_buffer=0;
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_pipeline_release\",\"word\":"<<pending->word<<",\"status\":0,\"before_resources\":true,\"unsupported_storage_format\":true}\n";break;
                        }
                        if(storage_created)return storage_created;
                        // A CPU snapshot cannot initialize an image alias of an earlier
                        // GPU writer whose guest bytes have not been downloaded.
                        auto overlaps=[](uint64_t a,uint64_t bytes,uint64_t b,uint64_t count){return bytes && count && a<=UINT64_MAX-bytes && b<=UINT64_MAX-count && a<b+count && b<a+bytes;};
                        bool disjoint=true;uint64_t sampled_depth_owner=0;
                        for(const auto& writer:completed_color_ranges)if(overlaps(storage_image.image_extent.guest_address,storage_image.image_extent.bytes,writer.first,writer.second) || overlaps(captured.address,captured.bytes,writer.first,writer.second))disjoint=false;
                        // Whole public DepthWriteAddress/GetDepthSliceSize and ImageInfo
                        // stencil_size policy, e3ce810f3a653f43ac64ebab63023de281a4103a,
                        // regs_depth.h/ImageInfo depth constructor, GPL-2.0-or-later.
                        // Bounded to this reached single-slice tiled Z32Float draw.
                        for(const auto& joined:vertex_attachment_joins)if(joined.vertex->word==680){const auto& r=joined.vertex->registers->words;
                         const uint32_t format=r[0xa010]&3u,tile=(r[0xa010]>>20)&7u;
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_depth_domain\",\"format\":"<<format<<",\"tile\":"<<tile<<",\"view\":"<<r[0xa002]<<"}\n";
                         if(format!=3 || (r[0xa002]&0x00ffe7ffu))return 88;
                         const uint64_t samples=1ull<<((r[0xa010]>>2)&3u);
                         const uint64_t depth_address=uint64_t(r[0xa014])<<8,depth_bytes=(uint64_t(r[0xa017]&0x3fffffu)+1)*64*4*samples;
                         const uint64_t stencil_address=uint64_t(r[0xa015])<<8;uint64_t stencil_bytes=0;
                         for(const auto& allocation:depth_allocations)if(allocation.word==680)stencil_bytes=(uint64_t(r[0xa016]&2047u)+1)*8*allocation.parameters.height;
                         if(overlaps(storage_image.image_extent.guest_address,storage_image.image_extent.bytes,depth_address,depth_bytes) || overlaps(captured.address,captured.bytes,depth_address,depth_bytes))disjoint=false;
                         if(sampled_owner){const auto& source=*sampled_owner;const auto& shape=source.image;
                          const bool depth_overlap=overlaps(source.image_extent.guest_address,source.image_extent.bytes,depth_address,depth_bytes);
                          bool exact=false;
                          for(const auto& allocation:depth_allocations)if(allocation.word==680 && allocation.extent_valid){
                           exact=!source.image_status && !source.image_extent_status && depth_overlap && shape.guest_address==depth_address && source.image_extent.guest_address==depth_address && source.image_extent.bytes==depth_bytes && shape.type==14 && shape.samples==samples && shape.samples==2 && shape.data_format==4 && shape.number_format==7 && shape.tile_mode==1 && shape.width==allocation.parameters.width && shape.height==allocation.parameters.height && shape.pitch==(uint64_t(r[0xa016]&2047u)+1)*8 && shape.layers==1 && shape.levels==1 && !shape.base_level && !shape.base_array && !shape.compression && !shape.alt_tile;
                          }
                          if(exact && joined.initialized && joined.depth){if(sampled_depth_owner && sampled_depth_owner!=joined.depth)return 112;sampled_depth_owner=joined.depth;}
                          else if(depth_overlap)disjoint=false;
                          if((r[0xa011]&1u) && overlaps(source.image_extent.guest_address,source.image_extent.bytes,stencil_address,stencil_bytes))disjoint=false;
                         }
                         if((r[0xa011]&1u) && (overlaps(storage_image.image_extent.guest_address,storage_image.image_extent.bytes,stencil_address,stencil_bytes) || overlaps(captured.address,captured.bytes,stencil_address,stencil_bytes)))disjoint=false;
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_prior_depth_writer\",\"depth_address\":"<<depth_address<<",\"depth_bytes\":"<<depth_bytes<<",\"stencil_address\":"<<stencil_address<<",\"stencil_bytes\":"<<stencil_bytes<<",\"inputs_disjoint\":"<<(disjoint?"true":"false")<<"}\n";
                        }
                        if(!disjoint){census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_coherence_pending\",\"word\":"<<pending->word<<",\"status\":88,\"completed_packets\":"<<command_cursor.completed()<<",\"prior_GPU_writer_requires_transfer\":true,\"pending_color_guest_writers\":"<<pending_color_guest_ranges.size()<<",\"pending_depth_guest_writer\":"<<(!pending_depth_guest_owners.empty()?"true":"false")<<"}\n";census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_route_handoff\",\"word\":"<<pending->word<<",\"status\":88,\"completed_packets\":"<<command_cursor.completed()<<",\"resource_matches\":1,\"missing_whole_GPU_handler\":true,\"GPU_execution\":false}\n";return 88;}
                        // Cross-submit compute reads use the same exact retained generation
                        // authority as graphics. Never lift CPU guards or upload stale bytes.
                        std::shared_ptr<void> compute_depth_lease;
                        uint64_t compute_depth_generation=0;uint32_t compute_depth_writer=0;
                        if(sampled_owner && !sampled_depth_owner){const auto& source=*sampled_owner;
                         int current_writer_status=0;
                         pending_depth_guest_owners.observe([&](uint64_t token,uint32_t writer,bool dirty,uint32_t planes,bool,bool){
                          if(!dirty || current_writer_status)return;
                          for(const auto& retained:retained_depth_identities)if(retained.second==token){const auto& a=*retained.first;const auto& r=a.identity;const auto& p=a.parameters;
                           if((uint64_t(r[6])<<8)!=source.image_extent.guest_address)return;
                           if(planes || p.samples!=2 || p.z_format!=3 || p.stencil_format!=1){current_writer_status=88;return;}
                           CtrDepthMS2LayoutV1 layout{1,sizeof(layout),p.width,p.height,((r[8]&2047)+1)*8,p.samples,p.z_format,p.stencil_format,(r[2]>>20)&7,r[9]&0x3fffff,r[10]&2047,(r[10]>>13)&2047,0,0,uint64_t(r[6])<<8};
                           current_writer_status=runtime_depth_residency.retain(api,graphics_session,token,writer,layout,uint64_t(r[7])<<8);
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_current_depth_writer\",\"word\":"<<pending->word<<",\"token\":"<<token<<",\"writer_word\":"<<writer<<",\"status\":"<<current_writer_status<<",\"no_CPU_publication\":true}\n";return;
                          }
                         });
                         if(current_writer_status)return current_writer_status;

                         runtime_depth_residency.observe(api->context,[&](uint64_t,uint64_t,uint32_t writer,uint64_t generation,const CtrDepthBackingSpansV1& spans,uint64_t,uint64_t){
                          if(!overlaps(source.image_extent.guest_address,source.image_extent.bytes,spans.depth_address,spans.depth_bytes))return;
                          if(compute_depth_generation){disjoint=false;return;}
                          compute_depth_generation=generation;compute_depth_writer=writer;
                          if(overlaps(storage_image.image_extent.guest_address,storage_image.image_extent.bytes,spans.depth_address,spans.depth_bytes) ||
                             overlaps(storage_image.image_extent.guest_address,storage_image.image_extent.bytes,spans.stencil_address,spans.stencil_bytes) ||
                             overlaps(captured.address,captured.bytes,spans.depth_address,spans.depth_bytes) ||
                             overlaps(captured.address,captured.bytes,spans.stencil_address,spans.stencil_bytes)){disjoint=false;return;}
                          const int resolved=runtime_depth_residency.resolve_sampled_view(api->context,source.image,source.image_extent,generation,sampled_depth_owner,compute_depth_lease);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_retained_depth_resolve\",\"word\":"<<pending->word<<",\"status\":"<<resolved<<",\"writer_word\":"<<writer<<",\"generation\":"<<generation<<",\"lease\":"<<sampled_depth_owner<<",\"exact_view_and_guard\":"<<(!resolved?"true":"false")<<"}\n";
                          if(resolved)disjoint=false;
                         });
                         if(!disjoint)return 88;
                        }
                        // The alias retains the actual GPU depth generation; no CPU image
                        // snapshot is uploaded. Restore on every early exit and explicitly
                        // check restoration before allowing another ordered attachment write.
                        struct DepthReadScope{void* context;uint64_t token;std::ostream& log;uint64_t word;
                         int restore(){if(!token)return 0;const int result=ctr_video_depth_attachment_restore_v1(context,token);log<<"{\"scope\":\"original_GnmSubmitFlip_compute_depth_restore\",\"word\":"<<word<<",\"status\":"<<result<<",\"real_fence\":"<<(!result?"true":"false")<<"}\n";if(!result)token=0;return result;}
                         ~DepthReadScope(){if(token)restore();}
                        }depth_read{api->context,0,census_log,pending->word};
                        if(sampled_depth_owner && sampled_owner && !draw_snapshot_disjoint(sampled_owner->image_extent.guest_address,sampled_owner->image_extent.bytes,completed_compute_ranges))return 88;
                        if(sampled_depth_owner){depth_read.token=sampled_depth_owner;const int aliased=ctr_video_depth_sampled_alias_v1(api->context,sampled_depth_owner,&compute_sampled);
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_depth_sampled_alias\",\"word\":"<<pending->word<<",\"status\":"<<aliased<<",\"source_word\":"<<(compute_depth_generation?compute_depth_writer:680)<<",\"retained_generation\":"<<compute_depth_generation<<",\"same_retained_GPU_owner\":true,\"CPU_snapshot_uploaded\":false,\"owned_token\":"<<(compute_sampled?"true":"false")<<"}\n";
                         if(aliased)return aliased;
                        }
                        std::vector<unsigned char> storage_initial=storage_image.image_payload.snapshot;
                        bool composed_depth_initial=false;uint64_t composed_lease=0,composed_generation=0,composed_guard=0;
                        if(storage_image.image_payload.capture_status){
                         const auto& capture=storage_image.image_payload;
                         runtime_depth_residency.observe_overlap(api->context,capture.address,capture.bytes,[&](uint64_t,uint64_t lease,uint32_t,uint64_t generation,const CtrDepthMS2LayoutV1& layout,const CtrDepthBackingSpansV1& spans,uint64_t,uint64_t,int state,uint64_t){
                          if(state || layout.samples!=2)return;
                          std::vector<unsigned char> converted;const int status=snapshot_retained_depth_ms2(api->context,lease,generation,layout,spans,converted);
                          census_log<<"{\"scope\":\"compute_output_depth_GPU_snapshot\",\"word\":"<<pending->word<<",\"lease\":"<<lease<<",\"generation\":"<<generation<<",\"status\":"<<status<<",\"converted_depth_bytes\":"<<converted.size()<<",\"guest_publication\":false,\"guard_retirement\":false}\n";
                          if(!status){std::vector<unsigned char> composed;const int assembled=compose_depth_prefix_snapshot(memory,capture.address,capture.bytes,spans.depth_address,converted,composed);
                           census_log<<"{\"scope\":\"compute_output_composed_snapshot\",\"word\":"<<pending->word<<",\"status\":"<<assembled<<",\"bytes\":"<<composed.size()<<",\"CPU_prefix_bytes\":"<<(spans.depth_address>capture.address?spans.depth_address-capture.address:0)<<",\"guest_publication\":false,\"guard_retirement\":false}\n";
                           if(!assembled){storage_initial.swap(composed);composed_depth_initial=true;composed_lease=lease;composed_generation=generation;composed_guard=spans.depth_address;}
                          }
                         });
                         uint64_t image=0,generation=0,begin=0,guard_bytes=0;
                         const bool guarded=runtime_gpu_resident_memory.identify(storage_image.image_extent.guest_address,image,generation,begin,guard_bytes);
                         uint64_t suffix_image=0,suffix_generation=0,suffix_begin=0,suffix_bytes=0;
                         const bool suffix_guarded=capture.owned_bytes<capture.bytes && runtime_gpu_resident_memory.identify(capture.address+capture.owned_bytes,suffix_image,suffix_generation,suffix_begin,suffix_bytes);
                         runtime_depth_residency.observe_overlap(api->context,capture.address,capture.bytes,[&](uint64_t token,uint64_t lease,uint32_t writer,uint64_t generation,const CtrDepthMS2LayoutV1& layout,const CtrDepthBackingSpansV1& spans,uint64_t begin,uint64_t bytes,int status,uint64_t prior_bytes){
                          census_log<<"{\"scope\":\"compute_output_prior_depth_layout\",\"word\":"<<pending->word<<",\"token\":"<<token<<",\"lease\":"<<lease<<",\"generation\":"<<generation<<",\"writer\":"<<writer<<",\"status\":"<<status<<",\"width\":"<<layout.width<<",\"height\":"<<layout.height<<",\"pitch\":"<<layout.pitch<<",\"samples\":"<<layout.samples<<",\"z_format\":"<<layout.z_format<<",\"stencil_format\":"<<layout.stencil_format<<",\"tile_mode\":"<<layout.tile_mode<<",\"slice_tile_max\":"<<layout.slice_tile_max<<",\"slice_start\":"<<layout.slice_start<<",\"slice_max\":"<<layout.slice_max<<",\"alt_tile\":"<<layout.alt_tile<<",\"bank_swizzle\":"<<layout.bank_swizzle<<",\"depth_address\":"<<spans.depth_address<<",\"depth_bytes\":"<<spans.depth_bytes<<",\"stencil_address\":"<<spans.stencil_address<<",\"stencil_bytes\":"<<spans.stencil_bytes<<",\"guard_begin\":"<<begin<<",\"guard_bytes\":"<<bytes<<",\"prior_backing_bytes\":"<<prior_bytes<<",\"read_only_observation\":true}\n";
                         });
                         MEMORY_BASIC_INFORMATION page{};const bool queried=VirtualQuery(reinterpret_cast<void*>(storage_image.image_extent.guest_address),&page,sizeof(page))!=0;
                         census_log<<"{\"scope\":\"compute_output_capture_authority\",\"word\":"<<pending->word<<",\"range_attempted\":"<<(capture.range_attempted?"true":"false")<<",\"budget_bytes\":"<<capture.budget_bytes<<",\"requirement_status\":"<<capture.requirement_status<<",\"lease_status\":"<<capture.lease_status<<",\"owned_bytes\":"<<capture.owned_bytes<<",\"requested_bytes\":"<<capture.bytes<<",\"capture_status\":"<<capture.capture_status<<",\"release_status\":"<<capture.release_status<<",\"lease_end_status\":"<<capture.lease_end_status<<",\"suffix_guarded\":"<<(suffix_guarded?"true":"false")<<",\"suffix_image\":"<<suffix_image<<",\"suffix_generation\":"<<suffix_generation<<",\"suffix_begin\":"<<suffix_begin<<",\"suffix_bytes\":"<<suffix_bytes<<",\"guarded\":"<<(guarded?"true":"false")<<",\"image\":"<<image<<",\"generation\":"<<generation<<",\"guard_begin\":"<<begin<<",\"guard_bytes\":"<<guard_bytes<<",\"host_query\":"<<(queried?"true":"false")<<",\"host_state\":"<<(queried?page.State:0)<<",\"host_protection\":"<<(queried?page.Protect:0)<<",\"permissions_changed\":false}\n";
                        }
                        if(storage_image.image_extent_status || (!composed_depth_initial && storage_image.image_payload.capture_status) || storage_initial.size()!=storage_image.image_extent.bytes || compiled.second->push_status){
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_output_transfer_pending\",\"word\":"<<pending->word<<",\"status\":88,\"tile\":"<<storage_image.image.tile_mode<<",\"extent_status\":"<<storage_image.image_extent_status<<",\"bytes\":"<<storage_image.image_extent.bytes<<",\"depth_alias_prepared\":"<<(compute_sampled?"true":"false")<<",\"GPU_execution\":false}\n";census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_route_handoff\",\"word\":"<<pending->word<<",\"status\":88,\"completed_packets\":"<<command_cursor.completed()<<",\"resource_matches\":1,\"missing_whole_GPU_handler\":true,\"GPU_execution\":false}\n";return 88;}

                        ComputeGuestWriteback writeback;writeback.address=storage_image.image_extent.guest_address;writeback.initial=std::move(storage_initial);
                        std::vector<unsigned char> current_image(writeback.initial.size());int initialized=composed_depth_initial?0:image_guest_copy(memory,writeback.address,current_image.data(),current_image.size(),false);
                        if(!initialized && !composed_depth_initial && current_image!=writeback.initial)initialized=88;
                        if(!initialized)initialized=ctr_video_sampled_image_upload_tiled_v1(api->context,compute_image,writeback.initial.data(),writeback.initial.size(),&storage_image.image_extent);
                        uint32_t image_binding=0;unsigned image_matches=0;
                        for(uint32_t k=0;k<compiled.second->bindings.descriptor_count;++k){const auto& binding=compiled.second->bindings.descriptors[k];if(binding.kind==1 && binding.sharp_index==storage_image.sharp){image_binding=binding.binding;++image_matches;}}
                        if(!initialized && image_matches!=1)initialized=112;
                        std::vector<uint32_t> compute_image_bindings{image_binding};std::vector<uint64_t> compute_image_tokens{compute_image};
                        if(!initialized && sampled_owner){const auto& source=*sampled_owner;
                         if(source.image_status || source.image_extent_status || (!sampled_depth_owner && (source.image_payload.capture_status || source.image_payload.snapshot.size()!=source.image_extent.bytes)))initialized=88;
                         if(!initialized && overlaps(source.image_extent.guest_address,source.image_extent.bytes,storage_image.image_extent.guest_address,storage_image.image_extent.bytes))initialized=88;
                         for(const auto& writer:completed_color_ranges)if(overlaps(source.image_extent.guest_address,source.image_extent.bytes,writer.first,writer.second)){
                          if(!published_color_generations.matches(source.image_extent.guest_address,source.image_extent.bytes,source.image_payload.snapshot,completed_color_ranges))initialized=88;
                         }
                         std::vector<unsigned char> source_current(source.image_payload.snapshot.size());
                         if(!sampled_depth_owner){
                          if(!initialized)initialized=image_guest_copy(memory,source.image_extent.guest_address,source_current.data(),source_current.size(),false);
                          if(!initialized && source_current!=source.image_payload.snapshot)initialized=88;
                          if(!initialized && source.image.data_format==10)log_rgba8_occupancy(census_log,pending->word,source.image_extent.guest_address,source_current,"checked_compute_sampled_input");
                          if(!initialized)initialized=ctr_video_sampled_image_create_v1(api->context,&source.image,&compute_sampled);
                          if(!initialized)initialized=ctr_video_sampled_image_upload_tiled_v1(api->context,compute_sampled,source_current.data(),source_current.size(),&source.image_extent);
                         }else source_current.clear();
                         uint32_t source_binding=0;unsigned source_matches=0;for(uint32_t k=0;k<compiled.second->bindings.descriptor_count;++k){const auto& binding=compiled.second->bindings.descriptors[k];if(binding.kind==1 && !binding.storage && binding.sharp_index==source.sharp){source_binding=binding.binding;++source_matches;}}
                         if(!initialized && source_matches!=1)initialized=112;
                         if(!initialized){compute_image_bindings.push_back(source_binding);compute_image_tokens.push_back(compute_sampled);}
                         census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_sampled_initialized\",\"word\":"<<pending->word<<",\"status\":"<<initialized<<",\"bytes\":"<<source_current.size()<<",\"CPU_snapshot_rechecked\":"<<(!sampled_depth_owner?"true":"false")<<",\"retained_depth_alias\":"<<(sampled_depth_owner?"true":"false")<<",\"real_transfer_fence\":"<<(!initialized?"true":"false")<<"}\n";
                        }
                        // The provider consumes images in compiler contract order,
                        // not storage-first order. Original1087 lists sampled first.
                        if(!initialized){
                         std::vector<uint32_t> ordered_bindings;std::vector<uint64_t> ordered_tokens;
                         for(uint32_t k=0;k<compiled.second->bindings.descriptor_count;++k){const auto& binding=compiled.second->bindings.descriptors[k];if(binding.kind!=1)continue;
                          unsigned matches=0;for(size_t n=0;n<compute_image_bindings.size();++n)if(compute_image_bindings[n]==binding.binding){ordered_bindings.push_back(binding.binding);ordered_tokens.push_back(compute_image_tokens[n]);++matches;}
                          if(matches!=1){initialized=112;break;}
                         }
                         if(!initialized)initialized=ctr_video_graphics_layout_images_v1(api->context,compute_layout,ordered_bindings.data(),ordered_tokens.data(),static_cast<uint32_t>(ordered_tokens.size()));
                        }
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_storage_initialized\",\"word\":"<<pending->word<<",\"status\":"<<initialized<<",\"tiled_bytes\":"<<writeback.initial.size()<<",\"inputs_disjoint\":"<<(disjoint?"true":"false")<<",\"real_transfer_fence\":"<<(!initialized?"true":"false")<<"}\n";
                        if(initialized){census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_route_handoff\",\"word\":"<<pending->word<<",\"status\":"<<initialized<<",\"completed_packets\":"<<command_cursor.completed()<<",\"resource_matches\":1,\"missing_whole_GPU_handler\":true,\"GPU_execution\":false}\n";return initialized;}
                        struct Download{void* context;uint64_t token;const CtrGraphicsImageExtentV1* extent;};Download download{api->context,compute_image,&storage_image.image_extent};
                        const auto finish=+[](void* raw,const unsigned char* initial,uint64_t length,unsigned char* output,uint64_t capacity)->int{if(length!=capacity)return 60;const auto& d=*static_cast<Download*>(raw);return ctr_video_image_readback_tiled_v1(d.context,d.token,initial,length,d.extent,output);};
                        int dispatched=88,written_back=88;const uint32_t dispatch_word=static_cast<uint32_t>(pending->word);
                        const int compute_resume=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                         if(packet.word==dispatch_word)return execute_owned_route(submission_owner,packet,data,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){return ordered_registers.execute_dispatch(data,packet.words,[&](const CtrPm4::Registers& bank){
                          const uint32_t groups[]={bank.compute[1],bank.compute[2],bank.compute[3]};dispatched=ctr_video_compute_dispatch_v1(api->context,compute_pipeline,&compiled.second->push,groups);if(dispatched)return dispatched;
                          if(composed_depth_initial){
                           // Validate the real original compute and tiled download into
                           // owned host storage. The command remains pending: its guest
                           // destination overlaps a live depth exclusion lease.
                           std::vector<unsigned char> result(writeback.initial.size());
                           const int downloaded=finish(&download,writeback.initial.data(),writeback.initial.size(),result.data(),result.size());
                           census_log<<"{\"scope\":\"compute_output_GPU_result_before_publication\",\"word\":"<<pending->word<<",\"dispatch_status\":"<<dispatched<<",\"download_status\":"<<downloaded<<",\"bytes\":"<<result.size()<<",\"guest_publication\":false,\"guard_retirement\":false,\"command_completed\":false}\n";
                           if(downloaded)return downloaded;
                           std::shared_ptr<const std::vector<unsigned char>> prior;
                           const int current=runtime_depth_residency.prior_backing(api->context,composed_lease,composed_generation,prior);if(current)return current;
                           writeback.poisoned=true;
                           written_back=runtime_gpu_resident_memory.publish_overlap_prefix(writeback.address,writeback.initial,result,composed_guard,composed_lease,composed_generation);
                           census_log<<"{\"scope\":\"compute_output_depth_prefix_publication\",\"word\":"<<pending->word<<",\"status\":"<<written_back<<",\"bytes\":"<<result.size()<<",\"depth_overlap_bytes\":"<<(writeback.address+result.size()-composed_guard)<<",\"lease\":"<<composed_lease<<",\"prior_generation\":"<<composed_generation<<",\"old_full_alias_invalidated\":"<<(!written_back?"true":"false")<<",\"remaining_suffix_retained\":true,\"atomic_against_unsynchronized_guest\":false}\n";
                           if(!written_back){writeback.initial.swap(result);writeback.completed=true;writeback.poisoned=false;}
                           return written_back;
                          }
                          written_back=writeback.commit(memory,finish,&download);return written_back;
                         });});
                         if(packet.opcode==0x48){const auto complete=+[](void* raw)->int{const auto& owner=*static_cast<ComputeGuestWriteback*>(raw);return owner.completed && !owner.poisoned?0:88;};const int signaled=complete_eos_writeback(data,packet.words,memory,complete,&writeback);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_EOS_completed\",\"word\":"<<packet.word<<",\"status\":"<<signaled<<",\"after_GPU_and_guest_writeback\":"<<(writeback.completed?"true":"false")<<"}\n";return signaled;}
                         if(packet.opcode==0x58)return acquire_packet(packet,data);
                      if(packet.opcode==0x46){
                       const auto visibility=+[](void* context)->int{return ctr_video_queue_memory_visibility_v1(context);};
                       const int status=complete_event_metadata_flush(data,packet.words,visibility,api->context);
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_event_metadata_flush\",\"word\":"<<packet.word<<",\"status\":"<<status<<",\"control\":"<<(packet.words>1?data[1]:0)<<",\"real_queue_barrier_fence\":"<<(!status?"true":"false")<<",\"guest_writeback\":false,\"depth_dirty_owners\":"<<pending_depth_guest_owners.size()<<",\"GPU_execution\":"<<(!status?"true":"false")<<"}\n";return status;
                      }
                         if(packet.opcode==0x3c){const int waited=poll_wait_reg_mem(data,packet.words,ordered_registers.state(),memory);census_log<<"{\"scope\":\"original_GnmSubmitFlip_post_compute_wait\",\"word\":"<<packet.word<<",\"status\":"<<waited<<"}\n";return waited;}
                         return ordered_transport(packet,data);
                        });
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_GPU_dispatch\",\"word\":"<<dispatch_word<<",\"status\":"<<dispatched<<",\"guest_writeback_status\":"<<written_back<<",\"guest_bytes\":"<<writeback.initial.size()<<",\"writeback_completed\":"<<(writeback.completed?"true":"false")<<",\"resume_status\":"<<compute_resume<<",\"completed_packets\":"<<command_cursor.completed()<<",\"GPU_execution\":"<<(!dispatched?"true":"false")<<"}\n";
                        const int depth_restored=depth_read.restore();if(depth_restored)return depth_restored;
                        if(dispatched || written_back)return dispatched?dispatched:written_back;if(compute_resume && compute_resume!=88)return compute_resume;
                        if(storage_image.image.data_format==10)log_rgba8_occupancy(census_log,dispatch_word,writeback.address,writeback.initial,"completed_compute_writer");
                        completed_compute_ranges.push_back({writeback.address,writeback.initial.size()});
                        // Refresh every exact future color alias before releasing the
                        // completed compute generation. Draw admission rechecks both
                        // guest bytes and intervening GPU writers below.
                        for(const auto& attachment:attachment_joins){if(attachment.pair->word<=dispatch_word)continue;
                         std::vector<const GraphicsStageOwner::Color*> aliases;if(graphics_pair_color_inputs(*attachment.pair,aliases))continue;
                         for(const auto* alias:aliases){const auto& demand=alias->requirement;const uint64_t token=attachment.colors[alias->slot];
                          if(!token || demand.guest_address!=writeback.address || demand.bytes!=writeback.initial.size())continue;
                          bool done=false;for(const auto& prior:refreshed_compute_colors)if(prior.token==token && prior.writer==dispatch_word)done=true;if(done)continue;
                          int refreshed=color_coherence.begin(token,dispatch_word);uint64_t compared=0;
                          if(!refreshed)refreshed=refresh_compute_color(api->context,token,demand,writeback,memory,compared);
                          if(!refreshed)refreshed=color_coherence.complete(token,dispatch_word);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_to_color\",\"writer_word\":"<<dispatch_word<<",\"consumer_word\":"<<attachment.pair->word<<",\"slot\":"<<alias->slot<<",\"status\":"<<refreshed<<",\"compared_backing_bytes\":"<<compared<<",\"real_transfer_fences\":"<<(!refreshed?"true":"false")<<",\"pixel_payload_persisted\":false}\n";
                          if(refreshed)return refreshed;
                          refreshed_compute_colors.push_back({token,writeback.address,dispatch_word,completed_compute_ranges.size(),writeback.initial});
                         }
                        }

                        int released=ctr_video_compute_pipeline_release_v1(api->context,compute_pipeline);if(released)return released;compute_pipeline=0;
                        released=ctr_video_graphics_layout_release_v1(api->context,compute_layout);if(released)return released;compute_layout=0;
                        released=ctr_video_buffer_release_v1(api->context,compute_buffer);if(released)return released;compute_buffer=0;
                        released=ctr_video_sampled_image_release_v1(api->context,compute_image);if(released)return released;compute_image=0;
                        census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_pipeline_release\",\"word\":"<<dispatch_word<<",\"status\":0,\"before_resources\":true}\n";
                        pending=command_cursor.pending();
                        // Only the final plain marker may consume this exact fenced
                        // publication. Other variants retain their explicit boundary.
                        if(pending && pending->opcode==0x10 && pending->word+pending->words==words.size() && writeback.completed && !writeback.poisoned){
                         PrepareFlipRequest request{};
                         int flipped=flip_submission?prepare_plain_flip(words.data()+pending->word,pending->words,flip_handle,flip_stack_owned,flip_stack,request):88;
                         if(!flipped){
                          std::vector<unsigned char> checked(writeback.initial.size());
                          flipped=image_guest_copy(memory,writeback.address,checked.data(),checked.size(),false);
                          if(!flipped && checked!=writeback.initial)flipped=88;
                          if(!flipped){
                           // Whole display32 snapshot: inspect values, never persist pixels.
                           // Constant DWORD data is invariant under the tile permutation;
                           // this diagnostic does not pretend to detile nonconstant data.
                           uint32_t first_pixel=0;std::memcpy(&first_pixel,checked.data(),4);uint64_t different_words=0,nonzero_rgb_words=0;
                           for(size_t pixel=0;pixel+4<=checked.size();pixel+=4){uint32_t value=0;std::memcpy(&value,checked.data()+pixel,4);different_words+=value!=first_pixel;nonzero_rgb_words+=(value&0x00ffffffu)!=0;}
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_publication_pixel_values\",\"word\":"<<pending->word<<",\"guest_bytes\":"<<checked.size()<<",\"dword_count\":"<<checked.size()/4<<",\"first_BGRA_DWORD\":"<<first_pixel<<",\"different_DWORDs\":"<<different_words<<",\"nonzero_RGB_DWORDs\":"<<nonzero_rgb_words<<",\"full_checked_GPU_snapshot\":true,\"pixels_persisted\":false,\"nonconstant_detile_claim\":false}\n";
                          }
                          FlipSurfaceLock surface_lock;uint64_t label_base=0;
                          if(!flipped)flipped=ctr_video_tiled_label_address_v1(api->context,&label_base);
                          if(!flipped)flipped=surface_lock.acquire(memory,label_base,request.slot,request.argument);
                          // The retained session clock was opened by the first completed frame.
                          // StartPacing is deliberately non-idempotent (second call64).
                          if(!flipped && original_gnm_frame_end.frames()==0)flipped=api->start_pacing(api->context);
                          if(!flipped)flipped=ctr_video_submit_tiled_snapshot_v1(api->context,request.slot,writeback.address,checked.data(),checked.size(),request.argument);
                          CtrVideoCompletionV1 completion{};
                          if(!flipped)flipped=api->present_next(api->context,5000,&completion);
                          if(!flipped)flipped=surface_lock.complete(flipped,completion.count,completion.token,completion.argument);
                          if(!flipped)flipped=ctr_video_complete_tiled_flip_v1(api->context,request.slot,&completion);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_surface_lock\",\"word\":"<<pending->word<<",\"status\":"<<flipped<<",\"label_address\":"<<label_base+request.slot*8<<",\"lock_before_enqueue\":true,\"current_lock_retained_after_real_completion\":"<<(!flipped?"true":"false")<<",\"provider_completion_retirement\":true}\n";
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_actual_present\",\"word\":"<<pending->word<<",\"status\":"<<flipped<<",\"guest_address\":"<<writeback.address<<",\"guest_bytes\":"<<checked.size()<<",\"slot\":"<<request.slot<<",\"mode\":"<<request.mode<<",\"argument\":"<<request.argument<<",\"completion_count\":"<<completion.count<<",\"completion_token\":"<<completion.token<<",\"checked_GPU_publication\":true,\"real_present\":"<<(!flipped?"true":"false")<<"}\n";
                          if(flipped)return flipped;
                          completed_present_count=completion.count;completed_present_token=completion.token;completed_present_argument=completion.argument;
                          const uint32_t flip_word=static_cast<uint32_t>(pending->word);
                          const int marker_completed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                           if(packet.word!=flip_word || packet.opcode!=0x10)return 88;
                           PrepareFlipRequest validated{};return prepare_plain_flip(data,packet.words,flip_handle,flip_stack_owned,flip_stack,validated);
                          });
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_submission_complete\",\"word\":"<<flip_word<<",\"status\":"<<marker_completed<<",\"completed_packets\":"<<command_cursor.completed()<<",\"packet_count\":"<<command_cursor.count()<<",\"pending\":"<<(command_cursor.pending()?"true":"false")<<",\"real_present\":true,\"label_retirement_verified\":true}\n";
                          if(marker_completed || command_cursor.pending() || command_cursor.completed()!=command_cursor.count())return marker_completed?marker_completed:112;
                          // Diagnostic observation only: shorten host pause on explicit opt-in.
                          char short_observation[2]{}, no_observation[2]{};
                          const unsigned observation_ticks=(GetEnvironmentVariableA("CTR_NO_FRAME_OBSERVATION",no_observation,sizeof(no_observation))==1 && no_observation[0]=='1')?0u:((GetEnvironmentVariableA("CTR_SHORT_FRAME_OBSERVATION",short_observation,sizeof(short_observation))==1 && short_observation[0]=='1')?200u:3000u);
                          std::cout<<"{\"scope\":\"frame_observation_hold\",\"real_present\":true,\"hold_ms\":"<<observation_ticks*10<<"}\n";std::cout.flush();
                          if(!observation_ticks){const int pumped=api->pump_messages(api->context);if(pumped)return pumped;}
                          for(unsigned hold=0;hold<observation_ticks;++hold){if(api->pump_messages(api->context))break;Sleep(10);}
                         }
                        }
                        // Refresh exact future sampled aliases from the verified writer.
                        // Admission rejects any intervening overlapping compute writer.
                        if(pending)for(const auto& image:live_images){uint32_t consumer=0;
                         for(const auto& entry:submission_owner.entries)if(entry.word>dispatch_word && entry.stage.get()==image.stage){if(consumer && consumer!=entry.word)return 60;consumer=static_cast<uint32_t>(entry.word);}
                         if(!consumer)continue;
                         for(const auto& descriptor:image.stage->descriptors){
                          if(descriptor.kind!=1 || descriptor.sharp!=image.sharp || descriptor.image.guest_address!=writeback.address)continue;
                          if(descriptor.image_extent_status || descriptor.image_extent.bytes!=writeback.initial.size() || descriptor.image.data_format!=storage_image.image.data_format || descriptor.image.number_format!=storage_image.image.number_format || descriptor.image.pitch!=storage_image.image.pitch || descriptor.image.tile_mode!=storage_image.image.tile_mode)return 88;
                          Download refresh{api->context,image.token,&descriptor.image_extent};
                          const auto upload=+[](void* raw,const unsigned char* source,uint64_t bytes)->int{const auto& target=*static_cast<Download*>(raw);return ctr_video_sampled_image_upload_tiled_v1(target.context,target.token,source,bytes,target.extent);};
                          uint64_t compared=0;const int refreshed=refresh_compute_sampled(writeback,memory,upload,finish,&refresh,compared);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_compute_to_sampled\",\"writer_word\":"<<dispatch_word<<",\"consumer_word\":"<<consumer<<",\"sharp\":"<<image.sharp<<",\"status\":"<<refreshed<<",\"compared_guest_bytes\":"<<compared<<",\"real_transfer_fences\":"<<(!refreshed?"true":"false")<<",\"original_writer_guest_verified\":true,\"pixel_payload_persisted\":false}\n";
                          if(refreshed)return refreshed;
                          refreshed_compute_samples.push_back({image.stage,image.sharp,consumer,image.token,writeback.address,completed_compute_ranges.size(),writeback.initial});
                         }
                        }
                       }
                      }

                      pending=command_cursor.pending();if(!pending)break;
                        if(pending && pending->opcode==0x10 && pending->word+pending->words==words.size() && last_display_color.completed && !last_display_color.poisoned && completed_compute_ranges.size()==last_display_compute_epoch && published_color_generations.matches(last_display_color.address,last_display_color.initial.size(),last_display_color.initial,completed_color_ranges)){
                         PrepareFlipRequest request{};
                         int flipped=flip_submission?prepare_plain_flip(words.data()+pending->word,pending->words,flip_handle,flip_stack_owned,flip_stack,request):88;
                         if(!flipped){
                          std::vector<unsigned char> checked(last_display_color.initial.size());
                          flipped=image_guest_copy(memory,last_display_color.address,checked.data(),checked.size(),false);
                          if(!flipped && checked!=last_display_color.initial)flipped=88;
                          if(!flipped){
                           // Whole display32 snapshot: inspect values, never persist pixels.
                           // Constant DWORD data is invariant under the tile permutation;
                           // this diagnostic does not pretend to detile nonconstant data.
                           uint32_t first_pixel=0;std::memcpy(&first_pixel,checked.data(),4);uint64_t different_words=0,nonzero_rgb_words=0;
                           for(size_t pixel=0;pixel+4<=checked.size();pixel+=4){uint32_t value=0;std::memcpy(&value,checked.data()+pixel,4);different_words+=value!=first_pixel;nonzero_rgb_words+=(value&0x00ffffffu)!=0;}
                           census_log<<"{\"scope\":\"original_GnmSubmitFlip_publication_pixel_values\",\"word\":"<<pending->word<<",\"guest_bytes\":"<<checked.size()<<",\"dword_count\":"<<checked.size()/4<<",\"first_BGRA_DWORD\":"<<first_pixel<<",\"different_DWORDs\":"<<different_words<<",\"nonzero_RGB_DWORDs\":"<<nonzero_rgb_words<<",\"full_checked_GPU_snapshot\":true,\"pixels_persisted\":false,\"nonconstant_detile_claim\":false}\n";
                          }
                          FlipSurfaceLock surface_lock;uint64_t label_base=0;
                          if(!flipped)flipped=ctr_video_tiled_label_address_v1(api->context,&label_base);
                          if(!flipped)flipped=surface_lock.acquire(memory,label_base,request.slot,request.argument);
                          // The retained session clock was opened by the first completed frame.
                          // StartPacing is deliberately non-idempotent (second call64).
                          if(!flipped && original_gnm_frame_end.frames()==0)flipped=api->start_pacing(api->context);
                          if(!flipped)flipped=ctr_video_submit_tiled_snapshot_v1(api->context,request.slot,last_display_color.address,checked.data(),checked.size(),request.argument);
                          CtrVideoCompletionV1 completion{};
                          if(!flipped)flipped=api->present_next(api->context,5000,&completion);
                          if(!flipped)flipped=surface_lock.complete(flipped,completion.count,completion.token,completion.argument);
                          if(!flipped)flipped=ctr_video_complete_tiled_flip_v1(api->context,request.slot,&completion);
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_surface_lock\",\"word\":"<<pending->word<<",\"status\":"<<flipped<<",\"label_address\":"<<label_base+request.slot*8<<",\"lock_before_enqueue\":true,\"current_lock_retained_after_real_completion\":"<<(!flipped?"true":"false")<<",\"provider_completion_retirement\":true}\n";
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_actual_present\",\"word\":"<<pending->word<<",\"status\":"<<flipped<<",\"guest_address\":"<<last_display_color.address<<",\"guest_bytes\":"<<checked.size()<<",\"slot\":"<<request.slot<<",\"mode\":"<<request.mode<<",\"argument\":"<<request.argument<<",\"completion_count\":"<<completion.count<<",\"completion_token\":"<<completion.token<<",\"checked_GPU_publication\":true,\"real_present\":"<<(!flipped?"true":"false")<<"}\n";
                          if(flipped)return flipped;
                          completed_present_count=completion.count;completed_present_token=completion.token;completed_present_argument=completion.argument;
                          const uint32_t flip_word=static_cast<uint32_t>(pending->word);
                          const int marker_completed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){
                           if(packet.word!=flip_word || packet.opcode!=0x10)return 88;
                           PrepareFlipRequest validated{};return prepare_plain_flip(data,packet.words,flip_handle,flip_stack_owned,flip_stack,validated);
                          });
                          census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_submission_complete\",\"word\":"<<flip_word<<",\"status\":"<<marker_completed<<",\"completed_packets\":"<<command_cursor.completed()<<",\"packet_count\":"<<command_cursor.count()<<",\"pending\":"<<(command_cursor.pending()?"true":"false")<<",\"real_present\":true,\"label_retirement_verified\":true}\n";
                          if(marker_completed || command_cursor.pending() || command_cursor.completed()!=command_cursor.count())return marker_completed?marker_completed:112;
                          // Diagnostic observation only: shorten host pause on explicit opt-in.
                          char short_observation[2]{}, no_observation[2]{};
                          const unsigned observation_ticks=(GetEnvironmentVariableA("CTR_NO_FRAME_OBSERVATION",no_observation,sizeof(no_observation))==1 && no_observation[0]=='1')?0u:((GetEnvironmentVariableA("CTR_SHORT_FRAME_OBSERVATION",short_observation,sizeof(short_observation))==1 && short_observation[0]=='1')?200u:3000u);
                          std::cout<<"{\"scope\":\"frame_observation_hold\",\"real_present\":true,\"hold_ms\":"<<observation_ticks*10<<"}\n";std::cout.flush();
                          if(!observation_ticks){const int pumped=api->pump_messages(api->context);if(pumped)return pumped;}
                          for(unsigned hold=0;hold<observation_ticks;++hold){if(api->pump_messages(api->context))break;Sleep(10);}
                         }
                        }

                      pending=command_cursor.pending();if(!pending)break;
                      if(command_cursor.completed()!=execution_before && (pending->opcode==0x2d || pending->opcode==0x27))continue;
                      unsigned matches=0;for(const auto& resource:prepared_draw_resources)if(resource.word==pending->word)++matches;
                      if(matches>1)return 60;
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_draw_handoff_inputs\",\"word\":"<<first_draw_word<<",\"pipeline_live\":"<<(first_pipeline?"true":"false")<<",\"viewport_status\":"<<first_viewport_status<<",\"viewport_count\":"<<first_viewports.count<<",\"dynamic_status\":"<<first_dynamic_status<<",\"push_resource_status\":"<<first_push_status<<",\"push_buffer_count\":"<<first_push_resources.buffer_count<<",\"push_data_status\":"<<first_push_data_status<<",\"push_data_owned\":"<<(first_push_data.version==1?"true":"false")<<",\"GPU_execution\":false}\n";

                      // Visibility completion must advance the same captured cursor.
                      // Calling the provider alone leaves word0 pending and falsely
                      // converts its genuine success into112 on the second submit.
                      if(pending->opcode==0x58){
                       const auto before=command_cursor.completed();const auto word=pending->word;
                       const int resumed=command_cursor.resume([&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& packet,const uint32_t* data){return ordered_transport(packet,data);});
                       census_log<<"{\"scope\":\"original_GnmSubmitFlip_acquire_cursor_resume\",\"word\":"<<word<<",\"status\":"<<resumed<<",\"completed_before\":"<<before<<",\"completed_after\":"<<command_cursor.completed()<<"}\n";
                       if(resumed && resumed!=88)return resumed;
                       if(command_cursor.completed()>before)continue;
                       return resumed?resumed:112;
                      }
                      const int handoff=pending->opcode==0x58?acquire_packet(*pending,words.data()+pending->word):execute_owned_route(submission_owner,*pending,words.data()+pending->word,ordered_registers.state(),[&](const GraphicsSubmissionOwner::Route&){return 88;});
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered_route_handoff\",\"word\":"<<pending->word<<",\"opcode\":"<<pending->opcode<<",\"packet_words\":"<<pending->words<<",\"first_payload\":"<<(pending->words>1?words[pending->word+1]:0)<<",\"status\":"<<handoff<<",\"resource_matches\":"<<matches<<",\"missing_whole_GPU_handler\":true,\"completed_packets\":"<<command_cursor.completed()<<",\"GPU_execution\":false}\n";
                      if(handoff!=88)return handoff?handoff:112;
                     }
                     break;
                     }
                     if(!command_cursor.pending() && last_display_color.completed && !last_display_color.poisoned && completed_compute_ranges.size()==last_display_compute_epoch && published_color_generations.matches(last_display_color.address,last_display_color.initial.size(),last_display_color.initial,completed_color_ranges)){
                      if(retained_display_generation==UINT64_MAX)return 88;
                      const auto generation=retained_display_generation+1;
                      const int retained_status=retained_display_authority.publish(reinterpret_cast<uint64_t>(api->context),last_display_color.address,generation,last_display_color.initial,true,false);
                      if(retained_status)return retained_status;
                      retained_display_candidate=last_display_color;retained_display_generation=generation;
                      census_log<<"{\"scope\":\"completed_display_authority_retained\",\"generation\":"<<generation<<",\"bytes\":"<<last_display_color.initial.size()<<",\"fully_completed_cursor\":true,\"pixels_persisted\":false}\n";
                     }
                     for(const auto& vertex:live_vertices){uint64_t layout=0;unsigned matches=0;
                      for(const auto& draw:attachment_joins)if(draw.pair->word==vertex.word){layout=draw.layout;++matches;}
                      for(const auto& draw:vertex_attachment_joins)if(draw.vertex->word==vertex.word){layout=draw.layout;++matches;}
                      if(matches!=1 || !layout)return 112;
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_layout_join\",\"word\":"<<vertex.word<<",\"binding\":"<<vertex.binding<<",\"bytes\":"<<vertex.bytes<<",\"unique_layout\":true,\"GPU_execution\":false}\n";
                     }
                     if(first_pipeline){const int released=ctr_video_graphics_pipeline_release_v1(api->context,first_pipeline);
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_first_pipeline_release\",\"status\":"<<released<<",\"before_layouts\":true}\n";if(released)return released;first_pipeline=0;}
                     if(graphics_depth_restore_status)return graphics_depth_restore_status;
                     const auto vertex_count=vertex_layouts.size();unsigned vertex_released=0;const int vertex_closed=vertex_layouts.close(&vertex_released);
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_only_layout_lifecycle\",\"created\":"<<vertex_count<<",\"released\":"<<vertex_released<<",\"status\":"<<vertex_closed<<",\"released_before_resources\":true,\"GPU_execution\":false}\n";
                     if(vertex_closed)return vertex_closed;
                     layout_status=layouts.close(&layout_released);if(layout_status)return layout_status;
                     census_log<<layout_rows.str();
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_layout_batch\",\"peak_live_layouts\":"<<layouts.size()<<",\"live_buffers_at_layout_close\":"<<batch.size()<<",\"live_images_at_layout_close\":"<<images.size()<<",\"live_samplers_at_layout_close\":"<<samplers.size()<<",\"layouts_released_before_buffers\":true,\"status\":"<<layout_status<<",\"storage_descriptor_updates\":true,\"descriptor_binding\":false,\"gpu_execution\":false}\n";
                     const int depth_alias_closed=depth_aliases.close();if(depth_alias_closed)return depth_alias_closed;
                     const auto depth_peak=depths.size();depth_status=depths.close();if(depth_status)return depth_status;depth_released=static_cast<unsigned>(depth_peak);
                     int resident_after_cleanup=0;
                     runtime_depth_residency.observe(api->context,[&](uint64_t token,uint64_t lease,uint32_t writer,uint64_t generation,const CtrDepthBackingSpansV1& spans,uint64_t,uint64_t){
                      int checked=runtime_depth_residency.covers(api->context,token,writer);uint64_t alias=0;
                      uint64_t resolved=0;std::shared_ptr<void> resolved_owner;
                      if(!checked)checked=runtime_depth_residency.resolve(api->context,spans,generation,resolved,resolved_owner);
                      if(!checked && (resolved!=lease || !resolved_owner))checked=112;
                      auto partial=spans;--partial.stencil_bytes;uint64_t rejected=UINT64_MAX;std::shared_ptr<void> rejected_owner;
                      if(!checked && (runtime_depth_residency.resolve(api->context,partial,generation,rejected,rejected_owner)!=88 || rejected!=UINT64_MAX || rejected_owner || runtime_depth_residency.resolve(api->context,spans,generation+1,rejected,rejected_owner)!=88 || rejected!=UINT64_MAX || rejected_owner))checked=112;
                      if(!checked)checked=ctr_video_depth_sampled_alias_v1(api->context,resolved,&alias);
                      if(!checked && !alias)checked=112;
                      if(alias){const int released=ctr_video_sampled_image_release_v1(api->context,alias);if(!checked)checked=released;}
                      census_log<<"{\"scope\":\"original_GnmSubmitFlip_resident_after_original_release\",\"status\":"<<checked<<",\"token\":"<<token<<",\"lease\":"<<lease<<",\"generation\":"<<generation<<",\"exact_logical_spans_resolved\":"<<(!checked?"true":"false")<<",\"partial_and_stale_lookup_rejected\":"<<(!checked?"true":"false")<<",\"real_sampled_alias\":"<<(!checked?"true":"false")<<",\"CPU_guard_retained\":"<<(!checked?"true":"false")<<",\"guest_publication\":false}\n";
                      if(checked)resident_after_cleanup=checked;
                     });
                     if(resident_after_cleanup)return resident_after_cleanup;
                     census_log<<depth_rows.str();
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_depth_batch\",\"peak_live_depths\":"<<depth_peak<<",\"released_after_layouts\":true,\"status\":"<<depth_status<<",\"GPU_execution\":false}\n";
                     const auto color_count=colors.size();const int color_closed=colors.close();
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_color_lifecycle\",\"created\":"<<color_count<<",\"released\":"<<(color_closed?0:color_count)<<",\"status\":"<<color_closed<<",\"released_after_layouts\":true,\"GPU_execution\":false}\n";
                     if(color_closed)return color_closed;
                     sampler_status=samplers.close(&sampler_released);if(sampler_status)return sampler_status;
                     unsigned vertex_buffer_released=0;const int vertex_buffer_closed=vertex_buffers.close(&vertex_buffer_released);if(vertex_buffer_closed)return vertex_buffer_closed;
                     census_log<<"{\"scope\":\"original_GnmSubmitFlip_vertex_buffer_lifecycle\",\"created\":"<<vertex_buffers.size()<<",\"released\":"<<vertex_buffer_released<<",\"compared_bytes\":"<<vertex_compared<<",\"live_joins\":"<<live_vertices.size()<<",\"released_after_layouts\":true,\"status\":"<<vertex_buffer_closed<<",\"GPU_execution\":false}\n";
                     const int released=batch.close(&buffer_released);if(released)return released;
                     image_status=images.close(&image_released);if(image_status)return image_status;
                     completed_single_submission=is_first_draw_buffer && !command_cursor.pending() && command_cursor.completed()==command_cursor.count();
                     census_log<<buffer_rows.str();return 0;
                    });
                    std::cout<<"{\"scope\":\"original_GnmSubmitFlip_batch_timing\",\"submission\":"<<submission_token<<",\"buffer\":"<<i<<",\"ccb\":"<<(kind?"true":"false")<<",\"preparation_ms\":"<<(execution_started-preparation_started)<<",\"execution_including_observation_hold_ms\":"<<(GetTickCount64()-execution_started)<<",\"status\":"<<buffer_status<<"}\n";
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_image_allocation_lifecycle\",\"attempted\":"<<image_attempts<<",\"created\":"<<image_created<<",\"released\":"<<image_released<<",\"unsupported\":"<<image_missing<<",\"peak_live_images\":"<<image_peak<<",\"released_after_layouts\":true,\"status\":"<<image_status<<",\"owner_thread_dispatch\":true,\"upload\":"<<((image_uploaded || image_tiled)?"true":"false")<<",\"linear_uploads_fence_completed\":"<<image_uploaded<<",\"tiled_uploads_fence_completed\":"<<image_tiled<<",\"descriptor_binding\":false,\"inflight_GPU_work\":false,\"gpu_execution\":false}\n";
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_buffer_lifecycle\",\"attempted\":"<<buffer_attempts<<",\"created\":"<<buffer_created<<",\"released\":"<<buffer_released<<",\"host_compared_bytes\":"<<buffer_compared<<",\"peak_live_buffers\":"<<buffer_peak<<",\"status\":"<<buffer_status<<",\"owner_thread_dispatch\":true,\"descriptor_binding\":false,\"gpu_readback\":false,\"gpu_execution\":false}\n";
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_paired_layout_lifecycle\",\"attempted\":"<<paired_stages.size()<<",\"created\":"<<layout_created<<",\"released\":"<<layout_released<<",\"unsupported\":"<<layout_missing<<",\"status\":"<<layout_status<<",\"owner_thread_dispatch\":true,\"fully_written_layouts\":"<<layout_fully_written<<",\"all_descriptor_sets_written\":false,\"pipeline_created\":false,\"gpu_execution\":false}\n";
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_depth_allocation_lifecycle\",\"attempted\":"<<depth_attempts<<",\"created\":"<<depth_created<<",\"released\":"<<depth_released<<",\"unsupported\":"<<depth_missing<<",\"status\":"<<depth_status<<",\"owner_thread_dispatch\":true,\"standalone_full_aspect_clears\":"<<depth_cleared<<",\"htile_initialization\":false,\"gpu_execution\":false}\n";

                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_sampler_lifecycle\",\"attempted\":"<<sampler_attempts<<",\"created\":"<<sampler_created<<",\"released\":"<<sampler_released<<",\"peak_live_samplers\":"<<sampler_peak<<",\"released_after_layouts\":true,\"status\":"<<sampler_status<<",\"owner_thread_dispatch\":true,\"descriptor_binding\":false,\"inflight_GPU_work\":false,\"gpu_execution\":false}\n";
                    census_log<<"{\"scope\":\"original_GnmSubmitFlip_owned_batch\",\"retained_graphics_stages\":"<<submission_owner.entries.size()<<",\"retained_host_bytes\":"<<submission_owner.bytes<<",\"retained_routes\":"<<submission_owner.routes.size()<<",\"retention_status\":"<<submission_owner.status<<",\"compute_binaries_owned\":"<<submission_owner.computes.size()<<",\"compute_resources_owned\":false,\"source_writer_coherence\":false,\"gpu_execution\":false}\n";
                    if(!kind && i==0){const auto line=census_log.str();HANDLE log=CreateFileW(L"runtime/user/logs/submit_flip_census.jsonl",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD sent=0;
                     if(log==INVALID_HANDLE_VALUE || !WriteFile(log,line.data(),static_cast<DWORD>(line.size()),&sent,nullptr) || sent!=line.size()){if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);ExitProcess(4);}CloseHandle(log);}
                    ordered_log<<"{\"scope\":\"original_GnmSubmitFlip_ordered\",\"buffer\":"<<i<<",\"ccb\":"<<(kind?"true":"false")<<",\"decode_status\":"<<decode<<",\"packets\":[";
                    for(size_t k=0;k<ordered.size();++k){if(k)ordered_log<<",";const auto& p=ordered[k];ordered_log<<"["<<p.word<<","<<p.words<<","<<p.opcode<<"]";}
                    ordered_log<<"],\"render_target_writes_full_submission\":[";
                    for(size_t k=0;k<full_target_writes.size();++k){if(k)ordered_log<<",";const auto& w=full_target_writes[k];ordered_log<<"["<<w[0]<<","<<w[1]<<","<<w[2]<<"]";}
                    ordered_log<<"],\"dma_before_first_draw\":["<<prefix_dma.str()<<"],\"execution_routes\":["<<execution_routes.str()<<"],\"register_apply_failures_full_submission\":"<<register_failures<<",\"snapshot_consistent_across_guest_writers\":false,\"effects\":false}\n";
                    if(!kind && i==0){const auto line=ordered_log.str();HANDLE log=CreateFileW(L"runtime/user/logs/submit_flip_ordered.json",GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD sent=0;
                     if(log==INVALID_HANDLE_VALUE || !WriteFile(log,line.data(),static_cast<DWORD>(line.size()),&sent,nullptr) || sent!=line.size()){if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);ExitProcess(4);}CloseHandle(log);}
                    if(!kind && words.size()>=64){const auto* tail=words.data()+words.size()-64;
                     const bool known=tail[0]==0xc03e1000 && (tail[1]==0x68750777 || tail[1]==0x68750778 || tail[1]==0x68750780 || tail[1]==0x68750781);
                     std::cout<<"{\"scope\":\"original_GnmSubmitFlip_prepare\",\"buffer\":"<<i<<",\"known_header_selector\":"<<(known?"true":"false")<<",\"header\":"<<tail[0]<<",\"selector\":"<<tail[1];
                     if(known)std::cout<<",\"label_address_low\":"<<tail[2]<<",\"label_address_high\":"<<tail[3]<<",\"label_value\":"<<tail[4]<<",\"event\":"<<tail[5]<<",\"index\":"<<tail[6];
                     std::cout<<",\"effects\":false}\n";
                    }
                }
                const bool readable_commands=true;
                std::vector<CtrPm4::InitOperation> plan;
                const int plan_status=CtrPm4::PrepareInit(words.data(),words.size(),plan);
                size_t rejected_word=SIZE_MAX;unsigned rejected_opcode=UINT32_MAX;
                std::vector<CtrPm4::Packet> decoded;
                if(plan_status && !CtrPm4::DecodePackets(words.data(),words.size(),decoded))for(const auto& packet:decoded){
                    std::vector<CtrPm4::InitOperation> prefix;
                    if(CtrPm4::PrepareInit(words.data(),packet.word+packet.words,prefix)){
                        rejected_word=packet.word;rejected_opcode=packet.opcode;break;
                    }
                }
                std::cout<<"{\"scope\":\"original_GnmSubmit_plan_validation\",\"status\":"<<plan_status<<",\"prepared_operations\":"<<plan.size()<<",\"first_rejected_word\":"<<rejected_word<<",\"first_rejected_opcode\":"<<rejected_opcode<<",\"gpu_execution\":false}\n";
                std::map<unsigned,size_t> histogram;size_t cursor=0,packets=0;
                while(readable_commands && cursor<words.size()){
                    const auto header=words[cursor];const auto type=header>>30;
                    if(type==1)break;
                    const auto payload=type==2?0u:type==3?(((header>>16&0x3fff)+1)&0x3fff):((header>>16&0x3fff)+1);
                    if(payload>words.size()-cursor-1)break;
                    const auto opcode=header>>8&255;
                    if(type==3 && opcode==0x10 && payload){
                        const auto marker=words[cursor+1];
                        const char* category=marker==0x68750776u?"patched_flip":
                            (marker==0x68750777u || marker==0x68750778u || marker==0x68750780u || marker==0x68750781u)?"prepare_flip":
                            (marker==0x68750001u || marker==0x68750002u || marker==0x68750003u || marker==0x6875000eu)?"debug":
                            (marker>=0x68750004u && marker<=0x68750006u)?"resource_hint":
                            (marker>>16==0x6875u)?"reserved_hint":"ordinary";
                        std::cout<<"{\"scope\":\"original_GnmSubmit_nop_metadata\",\"word\":"<<cursor<<",\"payload_dwords\":"<<payload<<",\"payload_class\":\""<<category<<"\",\"reserved_hint_selector\":"<<((marker>>16==0x6875u)?(marker&65535):0)<<"}\n";
                    }
                    if(type==3 && (opcode==0x69 || opcode==0x76 || opcode==0x79) && payload){
                        const auto control=words[cursor+1];
                        std::cout<<"{\"scope\":\"original_GnmSubmit_set_metadata\",\"word\":"<<cursor<<",\"opcode\":"<<opcode<<",\"register_offset\":"<<(control&65535)<<",\"index\":"<<(control>>28)<<",\"reserved_control\":"<<(control&0x0fff0000u)<<",\"shader_compute\":"<<((header&2)?"true":"false")<<",\"data_dwords\":"<<(payload-1)<<"}\n";
                    }
                    if(type==3 && opcode==0x47 && payload==5){
                        const auto* packet=words.data()+cursor;
                        const uint64_t target=packet[2]|(uint64_t(packet[3]&65535)<<32);
                        const auto selection=packet[3]>>29;
                        const unsigned width=selection==1?4u:selection==2?8u:0u;
                        uint64_t previous=0;
                        const bool owned_target=width && !memory.read(memory.context,target,&previous,width);
                        const uint64_t value=selection==1?packet[4]:packet[4]|(uint64_t(packet[5])<<32);
                        const auto backing=direct_memory_backing;
                        const bool direct_target=width && backing && backing->contains_mapped_range(target,width);
                        std::cout<<"{\"scope\":\"original_GnmSubmit_eop_mapping\",\"direct_mapping_owned\":"<<(direct_target?"true":"false")<<",\"inside_command_buffer\":"<<((target>=address && target-address<=bytes && width<=bytes-(target-address))?"true":"false")<<"}\n";

                        std::cout<<"{\"scope\":\"original_GnmSubmit_eop_metadata\",\"event_type\":"<<(packet[1]&63)<<",\"event_index\":"<<(packet[1]>>8&15)<<",\"data_select\":"<<selection<<",\"interrupt_select\":"<<(packet[3]>>24&3)<<",\"address\":"<<target<<",\"alignment4\":"<<((target&3)?"false":"true")<<",\"alignment8\":"<<((target&7)?"false":"true")<<",\"write_bytes\":"<<width<<",\"owned_rw_read\":"<<(owned_target?"true":"false")<<",\"value_nonzero\":"<<(value?"true":"false")<<"}\n";
                    }
                    if(type==3 && opcode==0x58 && payload==6){
                        const auto* packet=words.data()+cursor;
                        std::cout<<"{\"scope\":\"original_GnmSubmit_acquire_metadata\",\"coher_control\":"<<packet[1]<<",\"size_lo\":"<<packet[2]<<",\"size_hi\":"<<packet[3]<<",\"base_lo\":"<<packet[4]<<",\"base_hi\":"<<packet[5]<<",\"poll_interval\":"<<packet[6]<<"}\n";
                    }
                    if(type==3 && opcode==0x28 && payload==2)
                        std::cout<<"{\"scope\":\"original_GnmSubmit_context_metadata\",\"load_control\":"<<words[cursor+1]<<",\"shadow_control\":"<<words[cursor+2]<<"}\n";
                    ++histogram[type==3?opcode:256+type];++packets;cursor+=1+payload;
                }
                std::cout<<"{\"scope\":\"original_GnmSubmit_command_metadata\",\"buffer\":"<<i<<",\"ccb\":"<<(kind?"true":"false")<<",\"bytes\":"<<bytes<<",\"address\":"<<address<<",\"owned_read\":"<<(readable_commands?"true":"false")<<",\"parsed_dwords\":"<<cursor<<",\"packets\":"<<packets<<",\"opcodes\":[";
                bool comma=false;for(const auto& item:histogram){if(comma)std::cout<<",";comma=true;std::cout<<"["<<item.first<<","<<item.second<<"]";}
                std::cout<<"]}\n";
            }
            std::cout.flush();
            char resume_completed[4]{};
            if(completed_single_submission && arrays && command_count==1 && submission.draw.size()==1 && submission.constant.size()==1 && submission.constant[0].words.empty() &&
               (native_bridge || (GetEnvironmentVariableA("CTR_RETURN_COMPLETED_GNM",resume_completed,sizeof(resume_completed))==1 && resume_completed[0]=='1'))){
             if(native_bridge && !flip_submission){std::printf("{\"scope\":\"original_GnmSubmit_ordered_complete\",\"result\":0,\"all_commands_completed\":true,\"all_resources_cleaned\":true,\"real_present\":false,\"caller\":%llu}\n",(unsigned long long)caller);std::fflush(stdout);return EXCEPTION_CONTINUE_EXECUTION;}
             uint64_t return_address=0;const auto rsp=exception->ContextRecord->Rsp;
             if(rsp>UINT64_MAX-8 || memory.read(memory.context,rsp,&return_address,8) || return_address!=caller)ExitProcess(4);
             if(original_gnm_frame_end.publish(completed_present_count,completed_present_token,GetCurrentThreadId(),true,true,true,completed_present_argument))ExitProcess(4);
             std::cout<<"{\"scope\":\"original_GnmSubmitFlip_completed_service_return\",\"nid\":\"xbxNatawohc\",\"result\":0,\"all_owned_commands_completed\":true,\"all_resources_cleaned\":true,\"real_present\":true,\"caller\":"<<caller<<"}\n"<<std::flush;
             exception->ContextRecord->Rax=0;exception->ContextRecord->Rsp=rsp+8;exception->ContextRecord->Rip=return_address;return EXCEPTION_CONTINUE_EXECUTION;
            }

 return EXCEPTION_CONTINUE_SEARCH;
}
inline int execute_native_ordered_submission(uint32_t count,uint64_t da,uint64_t ds,uint64_t ca,uint64_t cs,uint64_t caller){
 if(count!=1)throw std::runtime_error("Ordered GnmSubmit supports observed count1 only");
 CONTEXT context{};context.Rdi=count;context.Rsi=da;context.Rdx=ds;context.Rcx=ca;context.R8=cs;
 EXCEPTION_POINTERS exception{};exception.ContextRecord=&context;
 const auto result=execute_ordered_submission(&exception,false,caller,true);
 if(result!=EXCEPTION_CONTINUE_EXECUTION)throw std::runtime_error("Ordered GnmSubmit has incomplete command/resource cursor");
 return 0;
}

inline int gnm_submit_ordered_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;
 auto previous=direct_memory_backing;auto backing=std::make_shared<DirectMemoryBacking>(16384);direct_memory_backing=backing;
 int64_t physical=-1;void* target=nullptr;
 if(backing->allocate(0,16384,16384,0,0,&physical) || backing->map(&target,16384,51,0,physical,0))return 2;
 const auto address=reinterpret_cast<uint64_t>(target);GuestStack storage(32768,4096);const auto base=reinterpret_cast<uint64_t>(storage.bottom());
 *reinterpret_cast<uint64_t*>(base)=base+256;*reinterpret_cast<uint32_t*>(base+128)=32;
 const uint32_t commands[]={0xc0002a00,0,0xc0044700,0x504,static_cast<uint32_t>(address),0x40000000U|static_cast<uint32_t>(address>>32),0x76543210,0xfedcba98};
 std::memcpy(reinterpret_cast<void*>(base+256),commands,sizeof(commands));
 const auto before=original_gnm_frame_end.snapshot();auto prior=ordered_gnm_submit;ordered_gnm_submit=&execute_native_ordered_submission;
 const uint64_t arguments[]={1,base,base+128,0,0};
 if(ctr_gnm_submit_bridge_fixture(arguments) || *static_cast<const uint64_t*>(target)!=0xfedcba9876543210ULL)return 3;
 const auto after=original_gnm_frame_end.snapshot();if(after.count!=before.count || after.pending!=before.pending || after.frames!=before.frames)return 4;
 // Unknown packet must remain pending and must not write the authored sentinel.
 *static_cast<uint64_t*>(target)=0;*reinterpret_cast<uint32_t*>(base+256)=0xc000ff00;
 bool rejected=false;try{runtime_gnm_submit(1,base,base+128,0,0);}catch(...){rejected=true;}
 if(!rejected || *static_cast<uint64_t*>(target))return 5;
 ordered_gnm_submit=prior;direct_memory_backing=previous;if(runtime_video_close(1))return 6;
 std::cout<<"{\"scope\":\"gnm_submit_ordered_control\",\"passed\":true,\"SysV5\":true,\"INDEX_TYPE_and_real_EOP64\":true,\"unknown_packet_rejected_before_EOP\":true,\"no_presentation_receipt\":true,\"synthetic\":true}\n";return 0;
}
inline LONG CALLBACK entry_dependency_exception(EXCEPTION_POINTERS* exception) {
    if(gpu_fault_hook && exception->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && gpu_fault_hook(exception))return EXCEPTION_CONTINUE_EXECUTION;
    if(runtime_gpu_resident_memory.diagnose(exception))return EXCEPTION_CONTINUE_SEARCH;
    BankCall call;call.functions=entry_function_bank;call.data=entry_data_bank;
    if(capture_bank_call(exception,&call)!=EXCEPTION_EXECUTE_HANDLER) {
        if(runtime_exception_dispatch.dispatch(exception)==EXCEPTION_CONTINUE_EXECUTION)return EXCEPTION_CONTINUE_EXECUTION;
        if(exception->ExceptionRecord->ExceptionCode!=0xe06d7363 && exception->ExceptionRecord->ExceptionCode!=EXCEPTION_SINGLE_STEP && exception->ExceptionRecord->ExceptionCode!=EXCEPTION_BREAKPOINT) {
            char diagnostic[256];const auto length=snprintf(diagnostic,sizeof(diagnostic),"startup exception code=0x%lx rip=0x%llx rsp=0x%llx thread=%lu\n",exception->ExceptionRecord->ExceptionCode,static_cast<unsigned long long>(exception->ContextRecord->Rip),static_cast<unsigned long long>(exception->ContextRecord->Rsp),GetCurrentThreadId());
            DWORD written=0;if(length>0)WriteFile(GetStdHandle(STD_ERROR_HANDLE),diagnostic,static_cast<DWORD>(length),&written,nullptr);
        }

        if(exception->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION || !entry_images || !entry_metadata) return EXCEPTION_CONTINUE_SEARCH;
        for(std::size_t i=0;i<entry_images->size();++i) {
            const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[i].get());
            for(const auto& span:(*entry_metadata)[i].executable_spans) if(exception->ContextRecord->Rip>=base+span.first && exception->ContextRecord->Rip<base+span.second) {
                // Adapt the exact guest FS:[0] self-pointer load. Host FS stays untouched.
                const unsigned char fs_self_load[]={0x64,0x48,0x8b,0x04,0x25,0,0,0,0};
                // CTR_TLS_PATCH=1: after emulating, rewrite this executed instruction to
                // read the TEB slot (tls_storage.hpp). Displacement first, prefix last, so a
                // concurrent thread sees either form; the intermediate fs:[disp] form is
                // emulated here too.
                const std::uint32_t slot_disp=guest_tls_teb_slot==TLS_OUT_OF_INDEXES?0u:0x1480u+8u*guest_tls_teb_slot;
                unsigned char fs_slot_load[sizeof(fs_self_load)];std::memcpy(fs_slot_load,fs_self_load,sizeof(fs_self_load));std::memcpy(fs_slot_load+5,&slot_disp,4);
                const auto* at=reinterpret_cast<const unsigned char*>(exception->ContextRecord->Rip);
                const bool plain=std::memcmp(at,fs_self_load,sizeof(fs_self_load))==0;
                // A thread can fault on the original bytes while another thread completes the
                // rewrite before this handler reads them: the final gs form is accepted too.
                unsigned char gs_slot_load[sizeof(fs_self_load)];std::memcpy(gs_slot_load,fs_slot_load,sizeof(fs_slot_load));gs_slot_load[0]=0x65;
                const bool intermediate=slot_disp && (std::memcmp(at,fs_slot_load,sizeof(fs_slot_load))==0 || std::memcmp(at,gs_slot_load,sizeof(gs_slot_load))==0);
                if(exception->ContextRecord->Rip+sizeof(fs_self_load)<=base+span.second &&
                   exception->ExceptionRecord->NumberParameters>=2 &&
                   exception->ExceptionRecord->ExceptionInformation[0]==0 &&
                   (exception->ExceptionRecord->ExceptionInformation[1]==0 || intermediate) && current_tls_storage && (plain || intermediate)) {
                    try {
                        exception->ContextRecord->Rax=reinterpret_cast<std::uint64_t>(current_tls_storage->static_thread_pointer());
                        exception->ContextRecord->Rip+=sizeof(fs_self_load);
                        static const bool patch=[]{char v[2]{};return GetEnvironmentVariableA("CTR_TLS_PATCH",v,sizeof(v))==1 && v[0]=='1';}();
                        if(patch && plain && slot_disp && TlsGetValue(guest_tls_teb_slot)==reinterpret_cast<void*>(exception->ContextRecord->Rax)){
                            static std::mutex patch_mutex;static std::atomic<unsigned> patched{0};
                            std::lock_guard<std::mutex> guard(patch_mutex);
                            auto* code=reinterpret_cast<unsigned char*>(exception->ContextRecord->Rip-sizeof(fs_self_load));
                            DWORD previous=0;
                            if(code[0]==0x64 && VirtualProtect(code,sizeof(fs_self_load),PAGE_EXECUTE_READWRITE,&previous)){
                                std::memcpy(code+5,&slot_disp,4);FlushInstructionCache(GetCurrentProcess(),code,sizeof(fs_self_load));
                                code[0]=0x65;FlushInstructionCache(GetCurrentProcess(),code,sizeof(fs_self_load));
                                VirtualProtect(code,sizeof(fs_self_load),previous,&previous);
                                const auto count=++patched;
                                if(count<=4 || !(count&(count-1))){char line[160];const int n=snprintf(line,sizeof(line),"{\"scope\":\"guest_tls_load_patched\",\"sites\":%u,\"teb_slot\":%lu}\n",count,guest_tls_teb_slot);DWORD w=0;if(n>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(n),&w,nullptr);}
                            }
                        }
                        return EXCEPTION_CONTINUE_EXECUTION;
                    } catch(const std::exception&) { /* Explicit unresolved access below. */ }
                }
                char line[1024];
                const auto count=snprintf(line,sizeof(line),"{\"scope\":\"original_startup_unresolved_access\",\"image_sha256\":\"%s\",\"caller_offset\":%llu,\"access_kind\":%llu,\"fault_address\":%llu,\"rax\":%llu,\"rdi\":%llu,\"rsi\":%llu,\"rbp\":%llu,\"rsp\":%llu,\"main_reached\":false}\n",(*entry_metadata)[i].sha256.c_str(),static_cast<unsigned long long>(exception->ContextRecord->Rip-base),static_cast<unsigned long long>(exception->ExceptionRecord->ExceptionInformation[0]),static_cast<unsigned long long>(exception->ExceptionRecord->ExceptionInformation[1]),static_cast<unsigned long long>(exception->ContextRecord->Rax),static_cast<unsigned long long>(exception->ContextRecord->Rdi),static_cast<unsigned long long>(exception->ContextRecord->Rsi),static_cast<unsigned long long>(exception->ContextRecord->Rbp),static_cast<unsigned long long>(exception->ContextRecord->Rsp));
                DWORD written=0;if(count>0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(count),&written,nullptr);
                const auto* context=exception->ContextRecord;
                const auto register_count=snprintf(line,sizeof(line),"{\"scope\":\"original_fault_registers\",\"rbx\":%llu,\"rcx\":%llu,\"rdx\":%llu,\"r8\":%llu,\"r9\":%llu,\"r10\":%llu,\"r11\":%llu,\"r12\":%llu,\"r13\":%llu,\"r14\":%llu,\"r15\":%llu}\n",context->Rbx,context->Rcx,context->Rdx,context->R8,context->R9,context->R10,context->R11,context->R12,context->R13,context->R14,context->R15);
                if(register_count>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(register_count),&written,nullptr);
                auto frame=exception->ContextRecord->Rbp;
                for(unsigned depth=0;depth<8 && frame;++depth) {
                    std::uint64_t pair[2]{};SIZE_T read=0;
                    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(frame),pair,sizeof(pair),&read) || read!=sizeof(pair)) break;
                    for(std::size_t image=0;image<entry_images->size();++image) {
                        const auto image_base=reinterpret_cast<std::uint64_t>((*entry_images)[image].get());
                        for(const auto& executable:(*entry_metadata)[image].executable_spans) if(pair[1]>=image_base+executable.first && pair[1]<image_base+executable.second) {
                            const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_fault_frame_candidate\",\"depth\":%u,\"image_sha256\":\"%s\",\"return_offset\":%llu}\n",depth,(*entry_metadata)[image].sha256.c_str(),static_cast<unsigned long long>(pair[1]-image_base));
                            if(length>0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);
                        }
                    }
                    if(pair[0]<=frame || pair[0]-frame>1048576) break;
                    frame=pair[0];
                }
                ExitProcess(4);
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto& bank=call.data_access?*entry_data_bank:*entry_function_bank;
    const auto& identity=bank.records[call.index].identity;
    if(!call.data_access) {
        const auto argument=exception->ContextRecord->Rdi;
        std::uint64_t cell=0;SIZE_T count=0;
        const bool readable=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(argument),&cell,8,&count) && count==8;
        const char* argument_sha="outside_mapped_images";std::uint64_t offset=0;
        if(entry_images && entry_metadata) for(std::size_t i=0;i<entry_images->size();++i) {
            const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[i].get());
            for(const auto& span:(*entry_metadata)[i].spans) if(argument>=base+span.first && argument<base+span.second) {argument_sha=(*entry_metadata)[i].sha256.c_str();offset=argument-base;}
        }
        char argument_line[768];const auto length=snprintf(argument_line,sizeof(argument_line),"{\"scope\":\"original_missing_first_argument\",\"nid\":\"%s\",\"argument_image_sha256\":\"%s\",\"argument_offset\":%llu,\"cell_readable\":%s,\"cell_u64\":%llu,\"rdi\":%llu,\"rsi\":%llu,\"rbx\":%llu,\"rdx\":%llu,\"rcx\":%llu,\"r8\":%llu,\"r9\":%llu}\n",identity.nid.c_str(),argument_sha,static_cast<unsigned long long>(offset),readable?"true":"false",static_cast<unsigned long long>(cell),static_cast<unsigned long long>(argument),static_cast<unsigned long long>(exception->ContextRecord->Rsi),static_cast<unsigned long long>(exception->ContextRecord->Rbx),static_cast<unsigned long long>(exception->ContextRecord->Rdx),static_cast<unsigned long long>(exception->ContextRecord->Rcx),static_cast<unsigned long long>(exception->ContextRecord->R8),static_cast<unsigned long long>(exception->ContextRecord->R9));
        DWORD written=0;if(length>0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),argument_line,static_cast<DWORD>(length),&written,nullptr);
        if(identity.nid=="5uFKckiJYRM" && identity.library=="libSceGnmDriver") {
            const auto memory=make_owned_guest_copy_api();std::array<uint32_t,12> source{};std::array<uint32_t,40> destination{};
            const auto* c=exception->ContextRecord;
            const bool source_owned=c->Rdx && !memory.read(memory.context,c->Rdx,source.data(),sizeof(source));
            const bool destination_owned=!memory.read(memory.context,c->Rdi,destination.data(),sizeof(destination));
            std::cout<<"{\"scope\":\"original_GnmSetPsShader350_metadata\",\"size_dwords\":"<<c->Rsi<<",\"source_null\":"<<(c->Rdx?"false":"true")<<",\"source_12dwords_owned\":"<<(source_owned?"true":"false")<<",\"destination_40dwords_owned\":"<<(destination_owned?"true":"false")<<",\"source_high_address_zero\":"<<((source_owned && !source[1])?"true":"false")<<",\"gpu_execution\":false}\n"<<std::flush;
        }
        if((identity.nid=="gAhCn6UiU4Y" || identity.nid=="V31V01UiScY") && identity.library=="libSceGnmDriver") {
            // Public sceGnmSetVsShader consumes seven register DWORDs and emits
            // exactly29DWORD. Only selector/ownership metadata is persisted.
            const auto memory=make_owned_guest_copy_api();std::array<uint32_t,7> source{};
            std::array<uint32_t,29> destination{};
            const auto* c=exception->ContextRecord;
            const bool source_owned=!memory.read(memory.context,c->Rdx,source.data(),sizeof(source));
            const bool destination_owned=!memory.read(memory.context,c->Rdi,destination.data(),sizeof(destination));
            std::cout<<"{\"scope\":\"original_GnmVsShader_metadata\",\"nid\":\""<<identity.nid<<"\",\"size_dwords\":"<<c->Rsi<<",\"modifier\":"<<c->Rcx<<",\"source_7dwords_owned\":"<<(source_owned?"true":"false")<<",\"destination_29dwords_owned\":"<<(destination_owned?"true":"false")<<",\"modifier_mask_valid\":"<<((c->Rcx&0xfcfffc3fU)?"false":"true")<<",\"source_high_address_zero\":"<<((source_owned && !source[1])?"true":"false")<<",\"shader_low_nonzero\":"<<((source_owned && source[0])?"true":"false")<<",\"gpu_execution\":false}\n"<<std::flush;
        }
        if(identity.nid=="2SKEx6bSq-4" && identity.library=="libkernel") {
            struct BatchEntry {uint64_t start,offset,length;uint8_t protection,type;int16_t reserved;int32_t operation;};
            static_assert(sizeof(BatchEntry)==32);
            const auto memory=make_owned_guest_copy_api();const auto requested=static_cast<int32_t>(exception->ContextRecord->Rsi);
            std::array<BatchEntry,16> entries{};
            const bool read=requested>0 && requested<=16 && !memory.read(memory.context,exception->ContextRecord->Rdi,entries.data(),static_cast<size_t>(requested)*sizeof(BatchEntry));
            std::cout<<"{\"scope\":\"original_BatchMap_metadata\",\"count\":"<<requested<<",\"owned_read\":"<<(read?"true":"false")<<",\"out_null\":"<<(exception->ContextRecord->Rdx?"false":"true")<<"}\n";
            if(read)for(int i=0;i<requested;++i){const auto& e=entries[i];
                std::cout<<"{\"scope\":\"original_BatchMap_entry\",\"start\":"<<e.start<<",\"offset\":"<<e.offset<<",\"length\":"<<e.length<<",\"protection\":"<<unsigned(e.protection)<<",\"type\":"<<unsigned(e.type)<<",\"reserved\":"<<e.reserved<<",\"operation\":"<<e.operation<<"}\n";
            }
            std::cout.flush();
        }
        if(identity.nid=="1qXLHIpROPE" && identity.library=="libSceGnmDriver") {
         auto* c=exception->ContextRecord;const auto memory=make_owned_guest_copy_api();uint64_t ret=0;
         if(c->Rsp>UINT64_MAX-8 || memory.read(memory.context,c->Rsp,&ret,8) || ret!=call.caller)ExitProcess(4);
         int result=-1;uint64_t labels=0;
         if(static_cast<uint32_t>(c->Rsi)==7){
          std::shared_ptr<RuntimeVideoSession> session;{std::lock_guard<std::mutex> lock(runtime_video_mutex);session=runtime_video_session;}
          if(!session || static_cast<int32_t>(c->Rdx)!=1){std::fprintf(stderr,"NID=1qXLHIpROPE library=libSceGnmDriver caller=0x%llx unsupported: live VideoOut handle unavailable\n",static_cast<unsigned long long>(call.caller));std::fflush(stderr);ExitProcess(4);}
          const int status=session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_tiled_label_address_v1(api->context,&labels);});
          if(status){std::fprintf(stderr,"NID=1qXLHIpROPE library=libSceGnmDriver caller=0x%llx label provider status=%d\n",static_cast<unsigned long long>(call.caller),status);std::fflush(stderr);ExitProcess(4);}result=insert_wait_flip(memory,c->Rdi,static_cast<uint32_t>(c->Rsi),labels,static_cast<uint32_t>(c->Rcx));
          if(result){std::fprintf(stderr,"NID=1qXLHIpROPE library=libSceGnmDriver caller=0x%llx packet write status=%d\n",static_cast<unsigned long long>(call.caller),result);std::fflush(stderr);ExitProcess(4);}
         }
         std::printf("{\"scope\":\"original_GnmInsertWaitFlipDone_return\",\"result\":%d,\"size\":%llu,\"slot\":%llu,\"real_label_address\":%llu,\"caller\":%llu,\"thread\":%lu,\"GPU_wait_executed\":false}\n",result,static_cast<unsigned long long>(c->Rsi),static_cast<unsigned long long>(c->Rcx),static_cast<unsigned long long>(labels),static_cast<unsigned long long>(call.caller),GetCurrentThreadId());std::fflush(stdout);
         c->Rax=static_cast<uint64_t>(static_cast<int64_t>(result));c->Rsp+=8;c->Rip=ret;return EXCEPTION_CONTINUE_EXECUTION;
        }
        if(identity.nid=="yvZ73uQUqrk" && identity.library=="libSceGnmDriver") {
            const auto state=original_gnm_frame_end.snapshot();
            char enabled[4]{};
            if(GetEnvironmentVariableA("CTR_RETURN_COMPLETED_GNM",enabled,sizeof(enabled))==1 && enabled[0]=='1' && state.pending){
             const auto memory=make_owned_guest_copy_api();uint64_t return_address=0;const auto rsp=exception->ContextRecord->Rsp;
             if(rsp>UINT64_MAX-8 || memory.read(memory.context,rsp,&return_address,8) || return_address!=call.caller)ExitProcess(4);
             std::shared_ptr<RuntimeVideoSession> session;{std::lock_guard<std::mutex> guard(runtime_video_mutex);session=runtime_video_session;}
             if(!session)ExitProcess(4);uint64_t frame=0;
             const int status=original_gnm_frame_end.finish(GetCurrentThreadId(),[&](uint64_t count,uint64_t token){
              return session->invoke([&](const CtrVideoSessionApiV1* api){const CtrVideoCompletionV1 prior{count,token,state.argument};return ctr_video_end_tiled_frame_v1(api->context,&prior,&frame);});
             });
             std::cout<<"{\"scope\":\"original_GnmSubmitDone_frame_end\",\"status\":"<<status<<",\"frame\":"<<frame<<",\"completion_count\":"<<state.count<<",\"completion_token\":"<<state.token<<",\"real_GPU_idle\":"<<(!status?"true":"false")<<",\"commands_and_resources_completed\":true,\"retained_DCB_CCB_offsets\":0,\"next_submission_fresh_local_state\":true,\"synthetic_IRQ\":false,\"caller\":"<<call.caller<<"}\n"<<std::flush;
             if(status || frame!=original_gnm_frame_end.frames())ExitProcess(4);
             exception->ContextRecord->Rax=0;exception->ContextRecord->Rsp=rsp+8;exception->ContextRecord->Rip=return_address;return EXCEPTION_CONTINUE_EXECUTION;
            }

            std::cout<<"{\"scope\":\"original_GnmSubmitDone_pending\",\"completion_count\":"<<state.count<<",\"completion_token\":"<<state.token<<",\"owner_thread\":"<<state.thread<<",\"calling_thread\":"<<GetCurrentThreadId()<<",\"pending\":"<<(state.pending?"true":"false")<<",\"frames_ended\":"<<state.frames<<",\"real_end_provider_bound\":false,\"guest_success\":false}\n"<<std::flush;
        }
        if((identity.nid=="zwY0YV91TTI" || identity.nid=="xbxNatawohc") && identity.library=="libSceGnmDriver") {
            const auto handled=execute_ordered_submission(exception,identity.nid=="xbxNatawohc",call.caller);
            if(handled==EXCEPTION_CONTINUE_EXECUTION)return handled;
        }

        if(identity.nid=="N1bEoJ4SRw4" && identity.library=="libSceVideoOut") {
            std::array<unsigned char,32> mode{};const auto memory=make_owned_guest_copy_api();
            const bool readable_mode=!memory.read(memory.context,exception->ContextRecord->Rdx,mode.data(),mode.size());
            uint32_t mode_size=0;uint64_t refresh=0,resolution=0;
            std::memcpy(&mode_size,mode.data(),4);std::memcpy(&refresh,mode.data()+8,8);std::memcpy(&resolution,mode.data()+16,8);
            std::cout<<"{\"scope\":\"original_VideoOutConfigureOutputMode_metadata\",\"owned_read\":"<<(readable_mode?"true":"false")<<",\"size\":"<<mode_size<<",\"encoding\":"<<unsigned(mode[4])<<",\"range\":"<<unsigned(mode[5])<<",\"colorimetry\":"<<unsigned(mode[6])<<",\"depth\":"<<unsigned(mode[7])<<",\"refresh\":"<<refresh<<",\"resolution\":"<<resolution<<",\"mode_bytes\":"<<exception->ContextRecord->R8<<",\"options_bytes\":"<<exception->ContextRecord->R9<<",\"options_null\":"<<(exception->ContextRecord->Rcx?"false":"true")<<"}\n";std::cout.flush();
        }
        if(identity.nid=="w3BY+tAEiQY" && identity.library=="libSceVideoOut") {
            // Metadata observation only: no pixels captured, registration or GPU readiness.
            const auto memory=make_owned_guest_copy_api();
            CtrVideoBufferAttributesV1 attributes{};std::array<uint64_t,16> addresses{};
            const auto buffers=exception->ContextRecord->Rcx;
            const bool valid_count=buffers>0 && buffers<=addresses.size();
            const bool attributes_read=!memory.read(memory.context,exception->ContextRecord->R8,&attributes,sizeof(attributes));
            const bool addresses_read=valid_count && !memory.read(memory.context,exception->ContextRecord->Rdx,addresses.data(),buffers*sizeof(uint64_t));
            char detail[640];const auto size=snprintf(detail,sizeof(detail),"{\"scope\":\"original_VideoOutRegisterBuffers_metadata\",\"handle\":%llu,\"first\":%llu,\"count\":%llu,\"attributes_owned_read\":%s,\"addresses_owned_read\":%s,\"format\":%u,\"tiling\":%d,\"width\":%u,\"height\":%u,\"pitch\":%u,\"GPU_coherence\":false,\"registration_performed\":false}\n",static_cast<unsigned long long>(argument),static_cast<unsigned long long>(exception->ContextRecord->Rsi),static_cast<unsigned long long>(buffers),attributes_read?"true":"false",addresses_read?"true":"false",attributes.format,attributes.tiling,attributes.width,attributes.height,attributes.pitch_pixels);
            DWORD sent=0;if(size>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),detail,static_cast<DWORD>(size),&sent,nullptr);
            if(addresses_read)for(size_t index=0;index<buffers;++index) {
                uint64_t remaining=0;{
                    std::lock_guard<std::mutex> lock(owned_output_lock);
                    for(const auto& owner:owned_output_extents)for(const auto& extent:owner.second)
                        if(addresses[index]>=extent.first && addresses[index]<extent.second)remaining=(std::max)(remaining,extent.second-addresses[index]);
                }
                const auto backing=direct_memory_backing?reinterpret_cast<uint64_t>(direct_memory_backing->address()):0;
                const bool direct=backing && addresses[index]>=backing && addresses[index]-backing<direct_memory_backing->size();
                const auto address_length=snprintf(detail,sizeof(detail),"{\"scope\":\"original_VideoOutRegisterBuffers_address\",\"index\":%llu,\"address\":%llu,\"owned_extent_remaining\":%llu,\"direct_backing\":%s,\"direct_offset\":%llu,\"GPU_coherence\":false}\n",static_cast<unsigned long long>(index),static_cast<unsigned long long>(addresses[index]),static_cast<unsigned long long>(remaining),direct?"true":"false",static_cast<unsigned long long>(direct?addresses[index]-backing:0));
                if(address_length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),detail,static_cast<DWORD>(address_length),&sent,nullptr);
            }
        }
        if(identity.nid=="yb2cRhagD1I" && identity.library=="libSceGnmDriver") {
            // Public gnmdriver_init.h HwInitPacketSize=0x100 DWORDs, not capacity bytes.
            const auto address=exception->ContextRecord->Rdi;
            const auto capacity=static_cast<std::uint32_t>(exception->ContextRecord->Rsi);
            const bool range=address<=UINT64_MAX-1024;
            MEMORY_BASIC_INFORMATION info{};const auto query=VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info));
            const bool owned=range && is_owned_output(address,address+1024);
            const auto backing=direct_memory_backing?reinterpret_cast<std::uint64_t>(direct_memory_backing->address()):0;
            const bool direct=backing && address>=backing && address-backing<direct_memory_backing->size();
            char memory_line[512];const auto memory_length=snprintf(memory_line,sizeof(memory_line),"{\"scope\":\"original_gnm_init_destination_authority\",\"capacity_dwords\":%u,\"hardware_init_bytes\":1024,\"owned_output_extent\":%s,\"direct_backing\":%s,\"direct_offset\":%llu,\"committed\":%s,\"page_protection\":%lu}\n",capacity,owned?"true":"false",direct?"true":"false",static_cast<unsigned long long>(direct?address-backing:0),query&&info.State==MEM_COMMIT?"true":"false",query?info.Protect:0);
            DWORD memory_written=0;if(memory_length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),memory_line,static_cast<DWORD>(memory_length),&memory_written,nullptr);
        }
        if(identity.nid=="6UgtwV+0zb4" && identity.library=="libkernel") {
            for(std::size_t image=0;image<entry_images->size();++image) {
                const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[image].get());
                const auto& cur_sha=(*entry_metadata)[image].sha256;
                const auto target_off=(cur_sha=="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")?0x14cfb10ULL:0x1409af0ULL;
                if((cur_sha!="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1" && cur_sha!="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5") || exception->ContextRecord->Rdx!=base+target_off)continue;
                std::uint32_t key=0;SIZE_T got=0;
                const auto tsd_offset=(cur_sha=="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")?0x2a0cb50ULL:0x260cb50ULL;
                const bool valid=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+tsd_offset),&key,4,&got) && got==4;
                char line[256];const auto key_length=snprintf(line,sizeof(line),"{\"scope\":\"original_worker_tsd_key\",\"global_offset\":%llu,\"readable\":%s,\"key\":%u,\"runtime_owned\":%s}\n",static_cast<unsigned long long>(tsd_offset),valid?"true":"false",key,valid&&thread_keys.owns(key)?"true":"false");
                DWORD sent=0;if(key_length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(key_length),&sent,nullptr);
            }
            const auto object=exception->ContextRecord->Rcx;
            for(const auto field: {std::uint64_t{0x20},std::uint64_t{0x30},std::uint64_t{0x70}}) {
                std::uint64_t target=0;SIZE_T obtained=0;
                const bool valid=object<=UINT64_MAX-field && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(object+field),&target,8,&obtained) && obtained==8;
                const char* target_sha="outside_mapped_images";std::uint64_t target_offset=0;
                if(valid)for(std::size_t image=0;image<entry_images->size();++image) {
                    const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[image].get());
                    for(const auto& span:(*entry_metadata)[image].spans)if(target>=base+span.first && target<base+span.second) {target_sha=(*entry_metadata)[image].sha256.c_str();target_offset=target-base;}
                }
                char detail[512];const auto size=snprintf(detail,sizeof(detail),"{\"scope\":\"original_thread_object_field\",\"field_offset\":%llu,\"readable\":%s,\"target_image_sha256\":\"%s\",\"target_offset\":%llu}\n",static_cast<unsigned long long>(field),valid?"true":"false",target_sha,static_cast<unsigned long long>(target_offset));
                DWORD emitted=0;if(size>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),detail,static_cast<DWORD>(size),&emitted,nullptr);
            }
            for(unsigned chain=0;chain<2;++chain) {
            std::uint64_t pointer=object;
            const std::uint64_t callback_offsets[]={0x20,0,0,0xc8};
            const std::uint64_t completion_offsets[]={0x70,0,0xc8};
            const auto* offsets=chain?completion_offsets:callback_offsets;
            for(unsigned depth=0;depth<(chain?3U:4U);++depth) {
                std::uint64_t target=0;SIZE_T obtained=0;const auto field=offsets[depth];
                const bool valid=pointer<=UINT64_MAX-field && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pointer+field),&target,8,&obtained) && obtained==8;
                const char* sha="outside_mapped_images";std::uint64_t chain_offset=0;
                if(valid)for(std::size_t image=0;image<entry_images->size();++image) {
                    const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[image].get());
                    for(const auto& span:(*entry_metadata)[image].spans)if(target>=base+span.first && target<base+span.second) {sha=(*entry_metadata)[image].sha256.c_str();chain_offset=target-base;}
                }
                char detail[512];const auto size=snprintf(detail,sizeof(detail),"{\"scope\":\"%s\",\"depth\":%u,\"field_offset\":%llu,\"readable\":%s,\"target_image_sha256\":\"%s\",\"target_offset\":%llu}\n",chain?"original_completion_pointer_chain":"original_callback_pointer_chain",depth,static_cast<unsigned long long>(field),valid?"true":"false",sha,static_cast<unsigned long long>(chain_offset));
                DWORD emitted=0;if(size>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),detail,static_cast<DWORD>(size),&emitted,nullptr);
                if(chain && depth==0 && valid && target) {
                    std::uint64_t mutex_value=0,condition_value=0;SIZE_T got_mutex=0,got_condition=0;
                    const bool cells=target<=UINT64_MAX-0x30 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(target+0x20),&mutex_value,8,&got_mutex) && got_mutex==8 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(target+0x28),&condition_value,8,&got_condition) && got_condition==8;
                    bool mutex_owned=false,condition_owned=false;
                    if(cells){std::lock_guard<std::mutex> mx(mutex_registry_lock);mutex_owned=mutex_registry.count(mutex_value)!=0;}
                    if(cells){std::lock_guard<std::mutex> cv(condition_registry_lock);condition_owned=condition_registry.count(condition_value)!=0;}
                    char state[256];const auto state_size=snprintf(state,sizeof(state),"{\"scope\":\"original_completion_sync_ownership\",\"cells_readable\":%s,\"mutex_owned\":%s,\"mutex_zero\":%s,\"condition_owned\":%s,\"condition_zero\":%s}\n",cells?"true":"false",mutex_owned?"true":"false",mutex_value==0?"true":"false",condition_owned?"true":"false",condition_value==0?"true":"false");
                    DWORD sent=0;if(state_size>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),state,static_cast<DWORD>(state_size),&sent,nullptr);
                }
                if(!valid || !target)break;pointer=target;
            }
            }

        }

    }
    const char* caller_sha="unknown";std::uint64_t caller_offset=0;
    for(std::size_t i=0;i<entry_images->size();++i) {
        const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[i].get());
        for(const auto& span:(*entry_metadata)[i].executable_spans)
            if(call.caller>=base+span.first && call.caller<base+span.second) {
                caller_sha=(*entry_metadata)[i].sha256.c_str();caller_offset=call.caller-base;
            }
    }
    char location[256];
    const auto location_length=snprintf(location,sizeof(location),"{\"scope\":\"original_caller_location\",\"image_sha256\":\"%s\",\"offset\":%llu}\n",caller_sha,static_cast<unsigned long long>(caller_offset));
    DWORD location_written=0;
    if(location_length>0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),location,static_cast<DWORD>(location_length),&location_written,nullptr);
    char line[768];
    const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_startup_libc_dependency_boundary\",\"nid\":\"%s\",\"library\":\"%s\",\"source_image_sha256\":\"%s\",\"caller_address\":%llu,\"data_access\":%s,\"original_startup_executed\":true,\"original_libc_page_enabled\":true,\"main_reached\":false,\"runtime_replacements\":0}\n",identity.nid.c_str(),identity.library.c_str(),identity.image_sha256.c_str(),static_cast<unsigned long long>(call.caller),call.data_access?"true":"false");
    DWORD written=0;if(length>0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);
    //: actual VirtualQuery boundary needs owner/flags/output-size evidence.
    // Observe registers only; preserve the existing missing-import failure below.
    if(!call.data_access && identity.nid=="rVjRvHJ0X6c" && identity.library=="libkernel"){
        char query_line[512];const auto n=snprintf(query_line,sizeof(query_line),"{\"scope\":\"missing_virtual_query_arguments\",\"caller_image_sha256\":\"%s\",\"caller_offset\":%llu,\"address\":%llu,\"flags\":%llu,\"output\":%llu,\"info_size\":%llu,\"guest_modified\":false}\n",caller_sha,static_cast<unsigned long long>(caller_offset),static_cast<unsigned long long>(exception->ContextRecord->Rdi),static_cast<unsigned long long>(exception->ContextRecord->Rsi),static_cast<unsigned long long>(exception->ContextRecord->Rdx),static_cast<unsigned long long>(exception->ContextRecord->Rcx));
        DWORD emitted=0;if(n>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),query_line,static_cast<DWORD>(n),&emitted,nullptr);
    }
    char pending_gnm[4]{};
    if(!call.data_access && identity.nid=="xbxNatawohc" && identity.library=="libSceGnmDriver" &&
       GetEnvironmentVariableA("CTR_WAIT_GNM_SERVICE",pending_gnm,sizeof(pending_gnm))==1 && pending_gnm[0]=='1') {
        const auto& trapped=bank.records[call.index];
        try {
            if(!entry_metadata || trapped.image>=entry_metadata->size() || entry_metadata->empty())throw std::runtime_error("Gnm pending image missing");
            const uint64_t pending_args[5]={exception->ContextRecord->Rdi,exception->ContextRecord->Rsi,exception->ContextRecord->Rdx,exception->ContextRecord->Rcx,exception->ContextRecord->R8};
            const auto target=prepare_gnm_pending((*entry_metadata)[trapped.image],trapped.slot,entry_metadata->front().sha256,pending_args,call.caller);
            std::fprintf(stderr,"PENDING service9 NID=xbxNatawohc library=libSceGnmDriver caller=0x%llx thread=%lu; awaiting real implementation, no return\n",static_cast<unsigned long long>(call.caller),GetCurrentThreadId());std::fflush(stderr);
            exception->ContextRecord->Rip=target;return EXCEPTION_CONTINUE_EXECUTION;
        }catch(const std::exception& error){std::fprintf(stderr,"Gnm pending admission failed: %s\n",error.what());std::fflush(stderr);ExitProcess(4);}
    }
    // Opt-in diagnostic: keep a missing call pending without returning a value.
    // Other original workers may progress; never a service implementation.
    char parked_nid[128]{};
    if(!call.data_access && GetEnvironmentVariableA("CTR_PARK_MISSING_NID",parked_nid,sizeof(parked_nid)) && identity.nid==parked_nid){
        // Retail thread entry0x1409af0 calls argument+0x30, then completion
        // object's vtable+0xc8. Read identity only; never execute the callback.
        if(entry_images && !entry_images->empty() && observed_worker_target==entry_images->front().get()+0x1409af0){
            const auto arg=reinterpret_cast<std::uint64_t>(observed_worker_argument);
            std::uint64_t completion=0,vtable=0,signal=0,wait=0;SIZE_T read=0;
            const auto read64=[&](std::uint64_t address,std::uint64_t& value){return address && address<=UINT64_MAX-8 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,8,&read) && read==8;};
            const bool valid=arg && arg<=UINT64_MAX-0x78 && read64(arg+0x70,completion) && read64(completion,vtable) && vtable<=UINT64_MAX-0xe8 && read64(vtable+0xc8,signal) && read64(vtable+0xe0,wait);
            std::fprintf(stderr,"{\"scope\":\"parked_worker_completion_identity\",\"thread\":%lu,\"argument\":%llu,\"completion_object\":%llu,\"signal_target\":%llu,\"wait_target\":%llu,\"read_valid\":%s,\"guest_modified\":false}\n",GetCurrentThreadId(),static_cast<unsigned long long>(arg),static_cast<unsigned long long>(completion),static_cast<unsigned long long>(signal),static_cast<unsigned long long>(wait),valid?"true":"false");
        }
        observe_condition_frames(exception->ContextRecord->Rbp,true,"missing_call_original_frame");
        HANDLE pending=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!pending)ExitProcess(4);
        std::fprintf(stderr,"PENDING missing NID=%s library=%s caller=0x%llx thread=%lu; no return, diagnostic only\n",identity.nid.c_str(),identity.library.c_str(),static_cast<unsigned long long>(call.caller),GetCurrentThreadId());std::fflush(stderr);
        const DWORD waited=WaitForSingleObject(pending,INFINITE);CloseHandle(pending);
        std::fprintf(stderr,"Unexpected missing-call wait completion=%lu\n",waited);std::fflush(stderr);
    }
    ExitProcess(4);
}
inline LONG CALLBACK entry_boundary_exception(EXCEPTION_POINTERS* exception) {
    if(gpu_fault_hook && exception->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && gpu_fault_hook(exception))return EXCEPTION_CONTINUE_EXECUTION;
    const auto* record=exception->ExceptionRecord;
    if(record->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION || record->NumberParameters<2 ||
       record->ExceptionInformation[0]!=8 || record->ExceptionInformation[1]!=boundary_target)
        return EXCEPTION_CONTINUE_SEARCH;
    std::uint64_t caller=0;SIZE_T read=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(exception->ContextRecord->Rsp),&caller,8,&read) || read!=8)
        return EXCEPTION_CONTINUE_SEARCH;
    char line[512];
    const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_entry_first_libc_execute_boundary\",\"nid\":\"bzQExy189ZI\",\"library\":\"libc\",\"caller_return_offset\":%llu,\"retail_entry_executed\":true,\"libc_executed\":false,\"main_reached\":false,\"reason\":\"original_libc_execution_disabled_pending_TLS_bootstrap\"}\n",static_cast<unsigned long long>(caller-boundary_source));
    DWORD written=0;
    if(length>0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);
    ExitProcess(caller==boundary_source+0x99?4:5);
}
extern "C" void ctr_jump_startup(void*,const void*,void*);
inline int execute_original_init_env(const std::vector<MappedImage>& images,const std::vector<ImageMetadata>& metadata) {
    const auto libc_sha="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816";
    for(std::size_t i=0;i<metadata.size();++i) {
        if(metadata[i].sha256!=libc_sha) continue;
        for(const auto& symbol:metadata[i].exports) {
            if(symbol.nid!="bzQExy189ZI" || symbol.library!="libc" || symbol.module!="libc" || symbol.type!=2) continue;
            if(symbol.address!=0xad7b0 || images[i].get()[symbol.address]!=0xc3)
                throw std::runtime_error("Pinned original init_env RET differs");
            bool executable=false;
            for(const auto& span:metadata[i].executable_spans)
                if(symbol.address>=span.first && symbol.address<span.second) executable=true;
            if(!executable) throw std::runtime_error("Original init_env outside code");
            const auto page=symbol.address&~std::uint64_t(4095);DWORD previous=0;
            if(!VirtualProtect(images[i].get()+page,4096,PAGE_EXECUTE_READ,&previous) ||
               !FlushInstructionCache(GetCurrentProcess(),images[i].get()+symbol.address,1))
                throw std::runtime_error("Original init_env permission failed");
            auto* target=resolve_original_call(images,metadata,metadata.front().sha256,libc_sha,symbol.address);
            (void)ctr_call_original_integer2(target,0,0);
            DWORD ignored=0;
            if(!VirtualProtect(images[i].get()+page,4096,previous,&ignored))
                throw std::runtime_error("Original init_env permission restoration failed");
            std::cout << "{\"scope\":\"whole_original_libc_init_env_export\",\"image_sha256\":" << std::quoted(metadata[i].sha256)
                      << ",\"address\":710576,\"nid\":\"bzQExy189ZI\",\"library\":\"libc\",\"original_instruction\":\"RET\",\"original_returned\":true,\"reconstructed_function_executed\":false,\"main_reached\":false}\n";
            return 0;
        }
    }
    throw std::runtime_error("Pinned original init_env export missing");
}
inline int run_entry_boundary(std::vector<MappedImage>& input_images,const std::vector<ImageMetadata>& metadata,bool enable_init_page=false,bool bind_guard=false,bool bind_time=false,bool bind_mutex=false,bool bind_condition=false,bool bind_attributes=false,bool bind_settype=false,bool bind_recursive=false,bool bind_attribute_destroy=false,bool bind_random=false,bool bind_sysmodule=false,bool bind_memory=false,bool bind_query=false,bool initialize_libc_only=false,bool continue_after_libc_init=false) {
    // A startup worker must retain mappings after its creator returns. Move only
    // the non-returning startup path; initializer-only controls keep caller ownership.
    std::shared_ptr<std::vector<MappedImage>> startup_images;
    if(continue_after_libc_init) startup_images=std::make_shared<std::vector<MappedImage>>(std::move(input_images));
    auto& images=startup_images?*startup_images:input_images;
    if(metadata.empty() || metadata.front().executable_spans.empty()) throw std::runtime_error("Entry image missing");
    const auto pin=metadata.front().sha256;
    if(pin!="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1" &&
       pin!="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5") throw std::runtime_error("Entry SHA unsupported");
    const auto first=std::find_if(metadata.front().imports.begin(),metadata.front().imports.end(),[](const auto& symbol){return symbol.nid=="bzQExy189ZI" && symbol.library=="libc" && symbol.type==2;});
    if(first==metadata.front().imports.end()) throw std::runtime_error("Entry first libc import missing");
    std::memcpy(&boundary_target,images.front().get()+first->slot,8);boundary_source=reinterpret_cast<std::uint64_t>(images.front().get());condition_observation_base=boundary_source;
    bool target_verified=false;
    for(std::size_t i=1;i<metadata.size();++i) for(const auto& symbol:metadata[i].exports)
        if(symbol.nid==first->nid && symbol.library==first->library && symbol.module==first->module &&
           boundary_target==reinterpret_cast<std::uint64_t>(images[i].get())+symbol.address) target_verified=true;
    if(!target_verified) throw std::runtime_error("Entry first libc pointer differs");
    std::shared_ptr<TlsStorage> tls_storage;
    std::unique_ptr<TlsContextScope> tls_scope;
    if(initialize_libc_only) {
        if(images.size()!=metadata.size()) throw std::runtime_error("TLS image metadata differs");
        std::vector<TlsTemplate> templates;std::size_t relocated=0;
        for(std::size_t i=0;i<metadata.size();++i) if(metadata[i].has_tls) {
            templates.push_back({metadata[i].tls_initialized,static_cast<std::size_t>(metadata[i].tls_size),static_cast<std::size_t>(metadata[i].tls_alignment)});
            relocated+=apply_tls_module_id(images[i],metadata[i],templates.size());
        }
        tls_storage=std::make_shared<TlsStorage>(std::move(templates));
        tls_scope=std::make_unique<TlsContextScope>(*tls_storage);
        std::cout<<"{\"scope\":\"startup_tls_context\",\"module_relocations\":"<<relocated<<",\"guest_thread_creation\":false}\n"<<std::flush;
    }
    std::vector<ObjectBinding> services;
    if(bind_time) services.push_back({"ejekcaNQNq0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_orbis_gettimeofday)});
    if(bind_time) services.push_back({"n88vx3C5nW8","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_gettimeofday)});
    if(bind_mutex) {
        services.push_back({"7H0iTOciTLo","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_lock)});
        services.push_back({"2Z+PpY6CaJg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_unlock)});
    }
    if(bind_condition) services.push_back({"mkx2fVhNMsg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_broadcast)});
    if(initialize_libc_only) {services.push_back({"wtkt-teR1so","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_init)});services.push_back({"wtkt-teR1so","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_init)});}
    if(initialize_libc_only) {services.push_back({"E+tyo3lp5Lw","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_detach)});services.push_back({"E+tyo3lp5Lw","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_detach)});}
    if(initialize_libc_only) {services.push_back({"2Q0z6rnBrTE","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_stack_size)});services.push_back({"2Q0z6rnBrTE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_stack_size)});}
    if(initialize_libc_only) {services.push_back({"Jmi+9w9u0E4","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_create)});services.push_back({"Jmi+9w9u0E4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_create)});}
    if(initialize_libc_only) {services.push_back({"zHchY8ft5pk","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_destroy)});services.push_back({"zHchY8ft5pk","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_attr_destroy)});}
    if(initialize_libc_only) services.push_back({"1tKyG7RlMJo","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_get_priority)});
    if(initialize_libc_only) services.push_back({"4J2sUJmuHZQ","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_process_time)});
    if(initialize_libc_only) services.push_back({"7H0iTOciTLo","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_lock)});
    if(initialize_libc_only) services.push_back({"2Z+PpY6CaJg","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_unlock)});
    if(initialize_libc_only) {services.push_back({"K-jXhbt2gn4","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutex_trylock)});services.push_back({"K-jXhbt2gn4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutex_trylock)});}
    if(initialize_libc_only) {services.push_back({"HF7lK46xzjY","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutexattr_destroy)});services.push_back({"HF7lK46xzjY","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutexattr_destroy)});}
    if(initialize_libc_only) {services.push_back({"ttHNfU+qDBU","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutex_init)});services.push_back({"ttHNfU+qDBU","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutex_init)});}
    if(initialize_libc_only) {services.push_back({"mDmgMOGVUqg","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutexattr_settype)});services.push_back({"mDmgMOGVUqg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutexattr_settype)});}
    if(initialize_libc_only) {services.push_back({"dQHWEsJtoE4","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutexattr_init)});services.push_back({"dQHWEsJtoE4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutexattr_init)});}
    if(bind_attributes) services.push_back({"F8bUHwAG284","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_attributes_init)});
    if(bind_settype) services.push_back({"iMp8QpE+XO4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_attributes_settype)});
    if(bind_recursive) services.push_back({"cmo1RIYva9o","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_init)});
    if(bind_attribute_destroy) services.push_back({"smWEktiyyG0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_attributes_destroy)});
    if(bind_random) services.push_back({"PI7jIZj4pcE","libSceRandom","libSceRandom",reinterpret_cast<const void*>(&ctr_sysv_random_bytes)});
    if(bind_sysmodule) {
        if(!bind_random) throw std::runtime_error("Sysmodule requires concrete Random service");
        services.push_back({"g8cM39EUZ6o","libSceSysmodule","libSceSysmodule",reinterpret_cast<const void*>(&ctr_sysv_sysmodule_load)});
    }
    if(bind_memory) {
        configure_direct_memory(images.front(),metadata.front());
        if(initialize_libc_only) services.push_back({"6kPnj51T62Y","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_resolution)});
        if(initialize_libc_only) services.push_back({"uquVH4-Du78","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_close)});
        if(initialize_libc_only) services.push_back({"kGVLc3htQE8","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_capability)});
        if(initialize_libc_only) services.push_back({"HXzjK9yI30k","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_add_flip)});
        if(initialize_libc_only) services.push_back({"Xru92wHJRmg","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_add_vblank)});
        if(initialize_libc_only) services.push_back({"SbU3dwp80lQ","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_flip_status)});
        if(initialize_libc_only) services.push_back({"1FZBKy8HeNU","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_vblank_status)});
        if(initialize_libc_only) services.push_back({"CBiu4mCE1DA","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_set_rate)});
        if(initialize_libc_only) services.push_back({"i6-sR91Wt-4","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_set_attributes)});
        if(initialize_libc_only) services.push_back({"w3BY+tAEiQY","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_register)});
        if(initialize_libc_only) services.push_back({"OcQybQejHEY","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_label_address)});
        if(initialize_libc_only) services.push_back({"pjkDsgxli6c","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_mode_any)});
        if(initialize_libc_only) services.push_back({"N5KDtkIjjJ4","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_unregister)});
        if(initialize_libc_only) services.push_back({"N1bEoJ4SRw4","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_configure)});
        if(initialize_libc_only) services.push_back({"Up36PTk687E","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_open)});
        if(initialize_libc_only) services.push_back({"1jfXLRVzisc","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_usleep)});
        if(initialize_libc_only) services.push_back({"2SKEx6bSq-4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_batch_map)});
        if(initialize_libc_only) services.push_back({"hv1luiJrqQM","libScePad","libScePad",reinterpret_cast<const void*>(&ctr_sysv_pad_init)});
        if(initialize_libc_only) services.push_back({"0BzLGljcwBo","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_dispatch_direct)});
        if(initialize_libc_only) services.push_back({"Kx-h-nWQJ8A","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_set_cs)});
        if(initialize_libc_only) services.push_back({"xk0AcarP3V4","libScePad","libScePad",reinterpret_cast<const void*>(&ctr_sysv_pad_open)});
        if(initialize_libc_only) services.push_back({"kvYEw2lBndk","libSceGameLiveStreaming","libSceGameLiveStreaming",reinterpret_cast<const void*>(&ctr_sysv_live_stream_init_compat)});
        if(initialize_libc_only) services.push_back({"7qZVNgEu+SY","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_pop_marker)});
        if(initialize_libc_only) services.push_back({"W1Etj-jlW7Y","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_push_marker)});
        if(initialize_libc_only) services.push_back({"mLVL7N7BVBg","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_update_ps)});
        if(initialize_libc_only) services.push_back({"QOQtbeDqsT4","libSceAudioOut","libSceAudioOut",reinterpret_cast<const void*>(&ctr_sysv_audio_out_output)});
        if(initialize_libc_only) services.push_back({"s1--uE9mBFw","libSceAudioOut","libSceAudioOut",reinterpret_cast<const void*>(&ctr_sysv_audio_out_close)});
        if(initialize_libc_only) services.push_back({"W0Hpm2X0uPE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_set_priority)});
        if(initialize_libc_only) services.push_back({"b+uAV89IlxE","libSceAudioOut","libSceAudioOut",reinterpret_cast<const void*>(&ctr_sysv_audio_out_volume)});
        if(initialize_libc_only) services.push_back({"ekNvsT22rsY","libSceAudioOut","libSceAudioOut",reinterpret_cast<const void*>(&ctr_sysv_audio_out_open)});
        if(initialize_libc_only) services.push_back({"JfEPXVxhFqA","libSceAudioOut","libSceAudioOut",reinterpret_cast<const void*>(&ctr_sysv_audio_out_init)});
        if(initialize_libc_only) services.push_back({"Q3dyFuwGn64","libSceAjm","libSceAjm",reinterpret_cast<const void*>(&ctr_sysv_ajm_register)});
        if(initialize_libc_only) services.push_back({"dl+4eHSzUu4","libSceAjm","libSceAjm",reinterpret_cast<const void*>(&ctr_sysv_ajm_initialize)});
        if(initialize_libc_only) services.push_back({"Vo5V8KAwCmk","libSceSystemService","libSceSystemService",reinterpret_cast<const void*>(&ctr_sysv_hide_splash)});
        if(initialize_libc_only) services.push_back({"V31V01UiScY","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_update_vs)});
        if(initialize_libc_only) services.push_back({"HlTPoZ-oY7Y","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_draw_index)});
        if(initialize_libc_only) services.push_back({"GGsn7jMTxw4","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_draw_auto)});
        if(initialize_libc_only) services.push_back({"5uFKckiJYRM","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_set_ps)});
        if(initialize_libc_only) services.push_back({"gAhCn6UiU4Y","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_set_vs)});
        if(initialize_libc_only) ordered_gnm_submit=&execute_native_ordered_submission;
        if(initialize_libc_only) services.push_back({"zwY0YV91TTI","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_submit)});
        if(initialize_libc_only) {install_real_gnm_init350(direct_memory_backing,process_neo_mode());services.push_back({"yb2cRhagD1I","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_init350)});}
        services.push_back({"pO96TwzOm5E","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_direct_memory_size)});
    }
    if(bind_query) {
        if(!bind_memory) throw std::runtime_error("Direct query requires memory owner");
        services.push_back({"rVjRvHJ0X6c","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_virtual_query)});
        services.push_back({"BHouLQzh0X0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_direct_memory_query)});
    }
    if(initialize_libc_only) services.push_back({"JGgj7Uvrl+A","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_orbis_condition_broadcast)});
    if(initialize_libc_only) services.push_back({"kDh-NfxgMtE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_signal)});
    if(initialize_libc_only) services.push_back({"2Tb92quprl0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_init)});
    if(initialize_libc_only) services.push_back({"waPcxYiR3WA","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_attributes_destroy)});
    if(initialize_libc_only) services.push_back({"DzES9hQF4f4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_priority)});
    if(initialize_libc_only) services.push_back({"eXbUSpEaTsA","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_inherit)});
    if(initialize_libc_only) services.push_back({"4+h9EzwKF4I","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_policy)});
    services.push_back({"FxVZqBAA7ks","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_write)});
    services.push_back({"4wSze92BhLI","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_console_write)});
    services.push_back({"9JYNqN6jAKI","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_console_debug)});
    if(initialize_libc_only) services.push_back({"UTXzJbWhhTE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_stack_size)});
    if(initialize_libc_only) services.push_back({"m5-2bsNfv7s","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_attributes_init)});
    if(initialize_libc_only) services.push_back({"s9e3+YpRnzw","libSceSaveDataDialog","libSceSaveDataDialog",reinterpret_cast<const void*>(&ctr_sysv_save_dialog_initialize)});
    if(initialize_libc_only) services.push_back({"KK3Bdg1RWK0","libSceSaveDataDialog","libSceSaveDataDialog",reinterpret_cast<const void*>(&ctr_sysv_save_dialog_status)});
    if(initialize_libc_only) services.push_back({"YuH2FA7azqQ","libSceSaveDataDialog","libSceSaveDataDialog",reinterpret_cast<const void*>(&ctr_sysv_save_dialog_terminate)});
    if(initialize_libc_only) services.push_back({"uoUpLGNkygk","libSceCommonDialog","libSceCommonDialog",reinterpret_cast<const void*>(&ctr_sysv_common_dialog_initialize)});
    if(initialize_libc_only) services.push_back({"4czppHBiriw","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_semaphore_signal)});
    if(initialize_libc_only) services.push_back({"-o5uEDpN+oY","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_convert_local)});
    if(initialize_libc_only) services.push_back({"QBi7HCK03hw","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_clock)});
    if(initialize_libc_only) {services.push_back({"lLMT9vJAck0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_clock)});services.push_back({"lLMT9vJAck0","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_clock)});}
    if(initialize_libc_only) services.push_back({"u5oqtlIP+Fw","libSceNetCtlForNpToolkit","libSceNetCtl",reinterpret_cast<const void*>(&ctr_sysv_net_toolkit_check)});
    if(initialize_libc_only) services.push_back({"wIsKy+TfeLs","libSceNetCtlForNpToolkit","libSceNetCtl",reinterpret_cast<const void*>(&ctr_sysv_net_toolkit_register)});
    if(initialize_libc_only) services.push_back({"uBPlr0lbuiI","libSceNetCtl","libSceNetCtl",reinterpret_cast<const void*>(&ctr_sysv_net_connection_state)});
    if(initialize_libc_only) services.push_back({"obuxdTiwkF8","libSceNetCtl","libSceNetCtl",reinterpret_cast<const void*>(&ctr_sysv_netctl_info)});
    if(initialize_libc_only) services.push_back({"rPo6tV8D9bM","libSceSystemService","libSceSystemService",reinterpret_cast<const void*>(&ctr_sysv_system_status)});
    if(initialize_libc_only) services.push_back({"fZo48un7LK4","libSceSystemService","libSceSystemService",reinterpret_cast<const void*>(&ctr_sysv_system_parameter)});
    if(initialize_libc_only) services.push_back({"12wOHk8ywb0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_semaphore_poll)});
    if(initialize_libc_only) services.push_back({"Zxa0VhQVTsk","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_semaphore_wait)});
    if(initialize_libc_only) services.push_back({"188x57JYp0g","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_semaphore_create)});
    if(initialize_libc_only) services.push_back({"mL8NDH86iQI","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_map_flexible)});
    if(initialize_libc_only) services.push_back({"G3AnLNdRBjE","libSceNpWebApi","libSceNpWebApi",reinterpret_cast<const void*>(&ctr_sysv_np_web_init)});
    if(initialize_libc_only) services.push_back({"asz3TtIqGF8","libSceNpWebApi","libSceNpWebApi",reinterpret_cast<const void*>(&ctr_sysv_np_web_term)});
    if(initialize_libc_only) services.push_back({"A9cVMUtEp4Y","libSceHttp","libSceHttp",reinterpret_cast<const void*>(&ctr_sysv_http_init)});
    if(initialize_libc_only) services.push_back({"Ik-KpLTlf7Q","libSceHttp","libSceHttp",reinterpret_cast<const void*>(&ctr_sysv_http_term)});
    if(initialize_libc_only) services.push_back({"hdpVEUDFW3s","libSceSsl","libSceSsl",reinterpret_cast<const void*>(&ctr_sysv_ssl_init)});
    if(initialize_libc_only) services.push_back({"0K1yQ6Lv-Yc","libSceSsl","libSceSsl",reinterpret_cast<const void*>(&ctr_sysv_ssl_term)});
    if(initialize_libc_only) services.push_back({"dgJBaeJnGpo","libSceNet","libSceNet",reinterpret_cast<const void*>(&ctr_sysv_net_pool_create)});
    if(initialize_libc_only) services.push_back({"K7RlrTkI-mw","libSceNet","libSceNet",reinterpret_cast<const void*>(&ctr_sysv_net_pool_destroy)});
    if(initialize_libc_only) services.push_back({"tsvEmnenz48","libSceLibcInternal","libSceLibcInternal",reinterpret_cast<const void*>(&ctr_sysv_cxa_atexit)});
    if(initialize_libc_only) services.push_back({"H2e8t5ScQGc","libSceLibcInternal","libSceLibcInternal",reinterpret_cast<const void*>(&ctr_sysv_cxa_finalize)});
    if(initialize_libc_only) services.push_back({"3GPpjQdAMTw","libSceLibcInternal","libSceLibcInternal",reinterpret_cast<const void*>(&ctr_sysv_cxa_guard_acquire)});
    if(initialize_libc_only) services.push_back({"9rAeANT2tyE","libSceLibcInternal","libSceLibcInternal",reinterpret_cast<const void*>(&ctr_sysv_cxa_guard_release)});
    if(initialize_libc_only) services.push_back({"2emaaluWzUw","libSceLibcInternal","libSceLibcInternal",reinterpret_cast<const void*>(&ctr_sysv_cxa_guard_abort)});
    if(initialize_libc_only) services.push_back({"CdWp0oHWGr0","libSceUserService","libSceUserService",reinterpret_cast<const void*>(&ctr_sysv_user_initial)});
    if(initialize_libc_only) services.push_back({"-quPa4SEJUw","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_attr_get_stack)});
    if(initialize_libc_only) services.push_back({"noQgleu+KLE","libSceNpWebApi","libSceNpWebApi",reinterpret_cast<const void*>(&ctr_sysv_np_web_delete_request)});
    if(initialize_libc_only) services.push_back({"KjNeZ-29ysQ","libSceNpWebApi","libSceNpWebApi",reinterpret_cast<const void*>(&ctr_sysv_np_web_send_offline)});
    if(initialize_libc_only) services.push_back({"rdgs5Z1MyFw","libSceNpWebApi","libSceNpWebApi",reinterpret_cast<const void*>(&ctr_sysv_np_web_create_request)});
    if(initialize_libc_only) services.push_back({"zk6c65xoyO0","libSceNpWebApi","libSceNpWebApi",reinterpret_cast<const void*>(&ctr_sysv_np_web_create_user)});
    if(initialize_libc_only) services.push_back({"IADmD4tScBY","libSceImeDialog","libSceImeDialog",reinterpret_cast<const void*>(&ctr_sysv_ime_dialog_status)});
    if(initialize_libc_only) services.push_back({"3Zl8BePTh9Y","libSceNpManager","libSceNpManager",reinterpret_cast<const void*>(&ctr_sysv_np_check)});
    if(initialize_libc_only) services.push_back({"JELHf4xPufo","libSceNpManagerForToolkit","libSceNpManager",reinterpret_cast<const void*>(&ctr_sysv_np_toolkit_check)});
    if(initialize_libc_only) services.push_back({"0c7HbXRKUt4","libSceNpManagerForToolkit","libSceNpManager",reinterpret_cast<const void*>(&ctr_sysv_np_toolkit_register)});
    if(initialize_libc_only) services.push_back({"YIvqqvJyjEc","libSceNpManagerForToolkit","libSceNpManager",reinterpret_cast<const void*>(&ctr_sysv_np_toolkit_unregister)});
    if(initialize_libc_only) services.push_back({"wuI7c7UNk0A","libSceUserServiceForNpToolkit","libSceUserService",reinterpret_cast<const void*>(&ctr_sysv_user_callback_register)});
    if(initialize_libc_only) services.push_back({"yH17Q6NWtVg","libSceUserService","libSceUserService",reinterpret_cast<const void*>(&ctr_sysv_user_event)});
    if(initialize_libc_only) services.push_back({"p-o74CnoNzY","libSceNpManager","libSceNpManager",reinterpret_cast<const void*>(&ctr_sysv_np_id)});
    if(initialize_libc_only) services.push_back({"gjP9-KQzoUk","libScePad","libScePad",reinterpret_cast<const void*>(&ctr_sysv_pad_information)});
    if(initialize_libc_only) services.push_back({"YndgXqQVV7c","libScePad","libScePad",reinterpret_cast<const void*>(&ctr_sysv_pad_read_state)});
    if(initialize_libc_only) services.push_back({"g+PZd2hiacg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_destroy)});
    if(initialize_libc_only) services.push_back({"2Of0f+3mhhE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_mutex_destroy)});
    if(initialize_libc_only) services.push_back({"DYhhWbJSeRg","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_gamma)});
    if(initialize_libc_only) services.push_back({"pv9CI5VC+R0","libSceVideoOut","libSceVideoOut",reinterpret_cast<const void*>(&ctr_sysv_video_adjust_color)});
    if(initialize_libc_only) services.push_back({"1xxcMiGu2fo","libSceUserService","libSceUserService",reinterpret_cast<const void*>(&ctr_sysv_user_name)});
    if(initialize_libc_only) services.push_back({"fPhymKNvK-A","libSceUserService","libSceUserService",reinterpret_cast<const void*>(&ctr_sysv_user_login_list)});
    if(initialize_libc_only) services.push_back({"6xVpy0Fdq+I","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_signal_mask)});
    if(initialize_libc_only) services.push_back({"j3YMu1MVNNo","libSceUserService","libSceUserService",reinterpret_cast<const void*>(&ctr_sysv_user_service_initialize)});
    if(initialize_libc_only) services.push_back({"-2IRUCO--PM","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_read_tsc)});
    if(initialize_libc_only) services.push_back({"1j3S3n-tTW4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_tsc_frequency)});
    if(initialize_libc_only) services.push_back({"bt3CTBKmGyI","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_set_affinity)});
    if(initialize_libc_only) services.push_back({"VOx8NGmHXTs","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_cpu_mode)});
    if(initialize_libc_only) services.push_back({"eoht7mQOCmo","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_key_get)});
    if(initialize_libc_only) services.push_back({"+BzXYkqYeLE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_key_set)});
    if(initialize_libc_only) services.push_back({"PrdHuuDekhY","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_key_delete)});
    if(initialize_libc_only) services.push_back({"geDaqgH9lTg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_key_create)});
    if(initialize_libc_only) services.push_back({"rNhWz+lvOMU","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_destructor_set)});
    if(initialize_libc_only) services.push_back({"pB-yGZ2nQ9o","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_atexit_set)});
    if(initialize_libc_only) services.push_back({"WhCc1w3EhSI","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_atexit_report_set)});
    if(initialize_libc_only) {
        if(!bind_memory) throw std::runtime_error("Process parameter requires validated retail profile");
        process_parameter=images.front().get()+(pin=="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1"?0x2600000ULL:0x2a00000ULL);
        services.push_back({"959qrazPIrg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_get_proc_param)});
    }
    if(initialize_libc_only) services.push_back({"upoVrzMHFeE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_orbis_mutex_trylock)});
    if(initialize_libc_only) services.push_back({"9UK1vLZQft4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_orbis_mutex_lock)});
    if(initialize_libc_only) services.push_back({"nsYoNRywwNg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_attributes_init)});
    if(initialize_libc_only) services.push_back({"62KCwEMmzcM","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_attributes_destroy)});
    if(initialize_libc_only) services.push_back({"-Wreprtu0Qs","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_attr_set_detach)});
    if(initialize_libc_only) services.push_back({"3qxgM4ezETA","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_attr_set_affinity)});
    if(initialize_libc_only) services.push_back({"IOnSvHzqu6A","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_event_flag_set)});
    if(initialize_libc_only) services.push_back({"JTvBflhYazQ","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_event_flag_wait)});
    if(initialize_libc_only) services.push_back({"BpFoboUJoZU","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_event_flag_create)});
    if(initialize_libc_only){
      services.push_back({"b0xyllnVY-I","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_add_eq_event)});
      services.push_back({"PVT+fuoS9gU","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_delete_eq_event)});
      services.push_back({"UoYY0DWMC0U","libSceGnmDriver","libSceGnmDriver",reinterpret_cast<const void*>(&ctr_sysv_gnm_get_eq_event_type)});
    }
    if(initialize_libc_only) services.push_back({"D0OdFMjp46I","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_equeue_create)});
    if(initialize_libc_only) services.push_back({"jpFjmgAC5AE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_equeue_delete)});
    if(initialize_libc_only) services.push_back({"fzyMKs9kim0","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_equeue_wait)});
    if(initialize_libc_only) services.push_back({"UK2Tl2DWUns","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_close)});
    if(initialize_libc_only) services.push_back({"Cg4srZ6TKbU","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_read)});
    if(initialize_libc_only) services.push_back({"+r3rMFwItV4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_pread)});
    if(initialize_libc_only) services.push_back({"nKWi-N2HBV4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_pwrite)});
    if(initialize_libc_only) services.push_back({"oib76F-12fk","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_lseek)});
    if(initialize_libc_only) services.push_back({"yFVnOdGxvZY","libScePad","libScePad",reinterpret_cast<const void*>(&ctr_sysv_pad_set_vibration)});
    if(initialize_libc_only) {services.push_back({"ltCfaGr2JGE","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutex_destroy)});services.push_back({"ltCfaGr2JGE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_mutex_destroy)});}
    if(initialize_libc_only) services.push_back({"Nn7zKwnA5q0","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_get_todo)});
    if(initialize_libc_only) services.push_back({"gUPGiOQ1tmQ","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_todo)});
    if(initialize_libc_only) services.push_back({"uWIYLFkkwqk","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_locus)});
    if(initialize_libc_only) services.push_back({"rvBSfTimejE","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_get_speed)});
    if(initialize_libc_only) services.push_back({"4AAcTU9R3XM","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_set_speed)});
    if(initialize_libc_only) services.push_back({"ts6GlZOKRrE","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_init)});
    if(initialize_libc_only) services.push_back({"M1Gma1ocrGE","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_open)});
    if(initialize_libc_only) services.push_back({"73fF1MFU8hA","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_chunks)});
    if(initialize_libc_only) services.push_back({"Uco1I0dlDi8","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_close)});
    if(initialize_libc_only) services.push_back({"MPe0EeBGM-E","libScePlayGo","libScePlayGo",reinterpret_cast<const void*>(&ctr_sysv_playgo_term)});
    if(initialize_libc_only) services.push_back({"xnd8BJzAxmk","libSceAppContent","libSceAppContentUtil",reinterpret_cast<const void*>(&ctr_sysv_app_content_list)});
    if(initialize_libc_only) services.push_back({"R9lA82OraNs","libSceAppContent","libSceAppContentUtil",reinterpret_cast<const void*>(&ctr_sysv_app_content_initialize)});
    if(initialize_libc_only) services.push_back({"99b82IKXpH4","libSceAppContent","libSceAppContentUtil",reinterpret_cast<const void*>(&ctr_sysv_app_content_get_int)});
    if(initialize_libc_only) services.push_back({"eV9wAD2riIA","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_stat)});
    if(initialize_libc_only) services.push_back({"kBwCPsYX-m4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_fstat)});
    if(initialize_libc_only) services.push_back({"1G3lF1Gg1k8","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_kernel_open)});
    if(initialize_libc_only) services.push_back({"BmMjYxmew1w","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_timedwait)});
    if(initialize_libc_only) services.push_back({"WKAXJ4XBPQ4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_condition_wait_owned)});
    if(initialize_libc_only) services.push_back({"onNY9Byn-W8","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_join_owned)});
    if(initialize_libc_only) services.push_back({"3kg7rT0NQIs","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_pthread_exit)});
    if(initialize_libc_only) services.push_back({"T72hz6ffq08","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_yield)});
    if(initialize_libc_only) services.push_back({"EI-5-jlq2dE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_numeric_id)});
    if(initialize_libc_only) services.push_back({"rcrVFJsQWRY","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_get_affinity)});
    if(initialize_libc_only) services.push_back({"Xs9hdiD7sAA","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_set_sched)});
    if(initialize_libc_only) services.push_back({"EotR8a3ASf4","libScePosix","libkernel",reinterpret_cast<const void*>(&ctr_sysv_posix_thread_self)});
    if(initialize_libc_only) services.push_back({"aI+OeCz8xrQ","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_self)});
    if(initialize_libc_only) services.push_back({"x1X76arYMxU","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_attributes_current)});
    if(initialize_libc_only) services.push_back({"8+s5BzZjxSg","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_attributes_affinity)});
    if(initialize_libc_only) services.push_back({"tn3VlD0hG60","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_orbis_mutex_unlock)});
    if(initialize_libc_only) services.push_back({"jh+8XiK4LeE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_asan_enabled)});
    if(initialize_libc_only) services.push_back({"L-Q3LEjIbgA","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_map_direct)});
    if(initialize_libc_only) services.push_back({"rTXw65xmLIA","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_allocate_direct)});
    if(initialize_libc_only) services.push_back({"WslcK1FQcGI","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_is_neo_mode)});
    if(initialize_libc_only) services.push_back({"NWtTN10cJzE","libSceLibcInternalExt","libSceLibcInternal",reinterpret_cast<const void*>(&ctr_sysv_heap_trace_query)});
    if(initialize_libc_only) services.push_back({"p5EcQeEeJAE","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_application_heap_set)});
    if(initialize_libc_only) services.push_back({"9BcDykPmo1I","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_guest_errno)});
    if(initialize_libc_only) services.push_back({"vNe1w4diLCs","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_tls_address)});
    if(continue_after_libc_init) {
        if(pin!="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1" &&
           pin!="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")throw std::runtime_error("Startup worker retail pin unsupported");
        const uint64_t worker_target=(pin=="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")?0x14cfb10:0x1409af0;
        validated_worker_target(images.front().get(),metadata.front(),pin,worker_target,117);
        startup_worker_images=startup_images;startup_worker_tls=tls_storage;startup_worker_metadata=metadata;
        services.push_back({"TywrFKCoLGY","libSceSaveData","libSceSaveData",reinterpret_cast<const void*>(&ctr_sysv_save_data_initialize3)});
        services.push_back({"4qGrR6eoP9Y","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_detach_owned)});
        services.push_back({"6UgtwV+0zb4","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_thread_create)});
    }
    if(initialize_libc_only) {
        services.push_back({"Cxwy7wHq4J0","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_0)});
        services.push_back({"PR5k1penBLM","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_1)});
        services.push_back({"cK6bYHf-Q5E","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_2)});
        services.push_back({"RujUxbr3haM","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_3)});
        services.push_back({"i-XwZjw0OOY","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_4)});
        services.push_back({"-hJRce8wn1U","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_5)});
        services.push_back({"OcAgPxcq5Vk","libSceJson2","libSceJson",reinterpret_cast<const void*>(&ctr_sysv_json2_6)});
        services.push_back({"gky0+oaNM4k","libSceNetCtl","libSceNetCtl",reinterpret_cast<const void*>(&ctr_sysv_netctl_init_compat)});
        services.push_back({"+MCXJlWdi+s","libSceSharePlay","libSceSharePlay",reinterpret_cast<const void*>(&ctr_sysv_shareplay_info_compat)});
        services.push_back({"isruqthpYcw","libSceSharePlay","libSceSharePlay",reinterpret_cast<const void*>(&ctr_sysv_shareplay_init_compat)});
        services.push_back({"co2NCj--pnc","libSceSharePlay","libSceSharePlay",reinterpret_cast<const void*>(&ctr_sysv_shareplay_prohibition_compat)});
        services.push_back({"Nlev7Lg8k3A","libSceNet","libSceNet",reinterpret_cast<const void*>(&ctr_sysv_net_init_compat)});
        std::cout<<"{\"scope\":\"compatibility_stubs_active\",\"count\":"<<ctr_declared_compat_count<<",\"tag\":\"R8\"}\n"<<std::flush;
    }
    if(initialize_libc_only){
        services.push_back({"oM+XCzVG3oM","libSceFontFt","libSceFontFt",reinterpret_cast<const void*>(&ctr_sysv_font_select_library)});
        services.push_back({"Xx974EW-QFY","libSceFontFt","libSceFontFt",reinterpret_cast<const void*>(&ctr_sysv_font_select_renderer)});
        services.push_back({"whrS4oksXc4","libSceFont","libSceFont",reinterpret_cast<const void*>(&ctr_sysv_font_memory_init)});
        services.push_back({"h6hIgxXEiEc","libSceFont","libSceFont",reinterpret_cast<const void*>(&ctr_sysv_font_memory_term)});
    }
    if(initialize_libc_only){
        struct MspaceAliasPin{const char* nid;uint64_t offset,size;};
        for(const auto& pin_alias:{MspaceAliasPin{"-hn1tcVHq5Q",404224,14},MspaceAliasPin{"OJjm-QOIHlI",404256,14},MspaceAliasPin{"Vla-Z+eXlxo",404272,593},MspaceAliasPin{"8zTFvBIAIN8",324176,379},MspaceAliasPin{"6sJWiWSRuqk",325040,458},MspaceAliasPin{"eLdDw6l0-bU",272784,224},MspaceAliasPin{"j4ViWNHEgww",324736,96},MspaceAliasPin{"hcuQgD53UxM",258176,238}}){
            unsigned matches=0;
            for(std::size_t i=0;i<metadata.size();++i)for(const auto& symbol:metadata[i].exports){
                if(symbol.nid!=pin_alias.nid || symbol.library!="libc" || symbol.module!="libc" || symbol.type!=2)continue;
                if(metadata[i].sha256!="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816" || symbol.address!=pin_alias.offset || symbol.size!=pin_alias.size)throw std::runtime_error("Original mspace export pin differs");
                bool executable_extent=false;for(const auto& span:metadata[i].executable_spans)if(symbol.address>=span.first && symbol.address<span.second && symbol.size<=span.second-symbol.address)executable_extent=true;
                if(!executable_extent)throw std::runtime_error("Original mspace outside executable span");
                // Binding precedes the loader's final RX publication. Do not execute here.
                auto* target=images[i].get()+symbol.address;
                services.push_back({symbol.nid,"libSceLibcInternal","libSceLibcInternal",target});++matches;
                std::cout<<"{\"scope\":\"original_mspace_alias_binding\",\"nid\":"<<std::quoted(symbol.nid)<<",\"target_offset\":"<<symbol.address<<",\"original_code\":true,\"engine_replacements\":0}\n"<<std::flush;
            }
            if(matches!=1)throw std::runtime_error("Original mspace export missing or ambiguous");
        }
    }
    if(initialize_libc_only && gpu_library_requested()){
        // Optional shadPS4 GPU library (gpu_library.hpp): SDK version from the retail
        // process parameters (OrbisProcParam.sdk_version at offset 0x10).
        std::uint32_t sdk_version=0;if(process_parameter)std::memcpy(&sdk_version,static_cast<const unsigned char*>(process_parameter)+0x10,4);
        load_gpu_library(sdk_version);route_gpu_imports(metadata,services);gpu_library_present=true;
        route_hle_imports(metadata,services);
        direct_memory_map_hook=[](std::uint64_t address,std::uint64_t bytes){gpu_map_guest(address,bytes);};
        guest_cpu_write_hook=[](const void* address,std::uint64_t bytes){gpu_invalidate_guest(address,bytes);};
        if(direct_memory_backing)direct_memory_backing->replay_mappings();
    }
    auto functions=install_missing_traps(images,metadata,services);
    if(continue_after_libc_init){install_startup_write_reload(images,metadata,pin);install_startup_mutex_reload(images,metadata,pin);}
    if(bind_sysmodule) {
        std::size_t verified=0;
        for(std::size_t i=0;i<metadata.size();++i) for(const auto& symbol:metadata[i].imports)
            if(symbol.library=="libSceRandom") {
                if(symbol.nid!="PI7jIZj4pcE" || symbol.module!="libSceRandom" || symbol.type!=2 || symbol.addend) throw std::runtime_error("Random module export unsupported");
                std::uint64_t value=0;std::memcpy(&value,images[i].get()+symbol.slot,8);
                if(value!=reinterpret_cast<std::uint64_t>(&ctr_sysv_random_bytes)) throw std::runtime_error("Random module slot not bound");
                ++verified;
            }
        if(!verified) throw std::runtime_error("Random module has no verified import slots");
        random_module_state.configure_verified_exports(true);
        std::cout<<"{\"scope\":\"random_module_export_readback\",\"verified_slots\":"<<verified<<"}\n"<<std::flush;
    }

    if(initialize_libc_only) {
        unsigned verified=0;
        for(std::size_t i=0;i<metadata.size();++i) for(const auto& symbol:metadata[i].imports) {
            if(symbol.library!="libSceSaveDataDialog") continue;
            const void* expected=nullptr;
            if(symbol.nid=="s9e3+YpRnzw") expected=reinterpret_cast<const void*>(&ctr_sysv_save_dialog_initialize);
            if(symbol.nid=="KK3Bdg1RWK0") expected=reinterpret_cast<const void*>(&ctr_sysv_save_dialog_status);
            if(symbol.nid=="YuH2FA7azqQ") expected=reinterpret_cast<const void*>(&ctr_sysv_save_dialog_terminate);
            if(!expected) continue; // Open retains the explicit missing-import trap.
            std::uint64_t value=0;std::memcpy(&value,images[i].get()+symbol.slot,8);
            if(symbol.type!=2 || symbol.addend || value!=reinterpret_cast<std::uint64_t>(expected)) throw std::runtime_error("SaveDialog lifecycle slot differs");
            ++verified;
        }
        if(verified!=3) throw std::runtime_error("SaveDialog lifecycle export set incomplete");
        save_module_exports_ready=true;
        std::cout<<"{\"scope\":\"save_dialog_export_readback\",\"verified_slots\":"<<verified<<",\"open_missing\":true}\n"<<std::flush;
    }
    std::vector<ObjectBinding> objects;
    if(bind_guard) objects.push_back({"f7uOxY9mM1U","libkernel","libkernel",process_guard()});
    if(initialize_libc_only) {
        unsigned matches=0;
        for(std::size_t i=0;i<metadata.size();++i)for(const auto& symbol:metadata[i].exports)
            if(symbol.nid=="2sWzhYqFH4E" && symbol.library=="libc" && symbol.module=="libc" && symbol.type==1){
                if(metadata[i].sha256!="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816" || symbol.address!=0x120c60 || symbol.size!=456)throw std::runtime_error("Original stdout object pin differs");
                bool backed=false;for(const auto& span:metadata[i].spans)if(symbol.address>=span.first && symbol.address<=span.second && symbol.size<=span.second-symbol.address)backed=true;
                if(!backed)throw std::runtime_error("Original stdout object extent invalid");
                objects.push_back({symbol.nid,symbol.library,symbol.module,images[i].get()+symbol.address});++matches;
            }
        if(matches!=1)throw std::runtime_error("Original stdout export missing or ambiguous");
        std::cout<<"{\"scope\":\"original_stdout_data_binding\",\"offset\":1182816,\"size\":456,\"original_object\":true}\n"<<std::flush;
    }
    auto data=install_missing_data_guards(images,metadata,objects);
    if(enable_init_page) {
        entry_images=&images;entry_metadata=&metadata;
        for(std::size_t i=0;i<images.size();++i){
            std::cout<<"{\"scope\":\"owned_original_mapping\",\"image_sha256\":\""<<metadata[i].sha256<<"\",\"base\":"<<reinterpret_cast<std::uint64_t>(images[i].get())<<",\"executable_spans\":[";
            bool comma=false;for(const auto& span:metadata[i].executable_spans){if(comma)std::cout<<",";comma=true;std::cout<<"["<<span.first<<","<<span.second<<"]";}std::cout<<"]}\n"<<std::flush;
        }
        condition_observation_image=+[](std::uint64_t caller,std::uint64_t& offset)->const char* {
            if(entry_images && entry_metadata)for(std::size_t i=0;i<entry_images->size();++i){
                const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[i].get());
                for(const auto& span:(*entry_metadata)[i].executable_spans)if(caller>=base+span.first && caller<base+span.second){offset=caller-base;return (*entry_metadata)[i].sha256.c_str();}
            }
            offset=0;return "unmapped";
        };
        entry_function_bank=&functions;entry_data_bank=&data;
        if(!AddVectoredExceptionHandler(1,entry_dependency_exception)) throw std::runtime_error("Entry dependency handler missing");
        bool enabled=false;
        for(std::size_t i=1;i<metadata.size();++i) for(const auto& symbol:metadata[i].exports)
            if(symbol.nid=="bzQExy189ZI" && symbol.library=="libc" && symbol.address==0xad7b0 && images[i].get()[symbol.address]==0xc3) {
                DWORD old=0;const auto page=symbol.address&~std::uint64_t(4095);
                if(!VirtualProtect(images[i].get()+page,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),images[i].get()+page,4096)) throw std::runtime_error("Libc startup page enable failed");
                enabled=true;
                if(bind_guard) {
                    if(metadata[i].sha256!="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816") throw std::runtime_error("Startup libc SHA differs");
                    // Run the pinned original module's executable PT_LOAD spans;
                    // dependency traps and data guards remain separate mappings.
                    for(const auto& span:metadata[i].executable_spans)
                        if(!VirtualProtect(images[i].get()+span.first,static_cast<SIZE_T>(span.second-span.first),PAGE_EXECUTE_READ,&old)) throw std::runtime_error("Libc original executable span enable failed");
                    if(!FlushInstructionCache(GetCurrentProcess(),images[i].get(),0)) throw std::runtime_error("Libc original instruction cache failed");
                }
            }
        if(!enabled) throw std::runtime_error("Verified original init_env page missing");
    }
    if(!AddVectoredExceptionHandler(1,entry_boundary_exception)) throw std::runtime_error("Entry boundary handler missing");
    // CTR_60FPS_PATCH=1, v01.21 only (ctrport/GUEST_PATCHES.md). Third-party patch, not
    // project work: "60 FPS Unlock" 2.0 by illusion, GoldHEN_Patch_Repository
    // patches/xml/CrashTeamRacing{EU,US}-Orbis.xml, AppVer 01.21, address 0x017a9d80
    // (GoldHEN adds 0x400000 to the image offset). That repository declares no license.
    // Original "mov [r14+0x488],eax" stores the configuration value [r12+0x18]; the patch
    // stores edx instead, which no instruction in the function sets here (it holds what
    // the preceding call left), so the effect is observed, not derived.
    if(pin=="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5"){
        static const bool patch_60fps=[]{char v[2]{};return GetEnvironmentVariableA("CTR_60FPS_PATCH",v,sizeof(v))==1 && v[0]=='1';}();
        if(patch_60fps){
            const unsigned char original_bytes[]={0x41,0x89,0x86,0x88,0x04,0x00,0x00};
            const unsigned char patch_bytes[]={0x41,0x89,0x96,0x88,0x04,0x00,0x00};
            auto* site=images.front().get()+0x013a9d80;
            if(std::memcmp(site,original_bytes,sizeof(original_bytes))!=0) throw std::runtime_error("60 FPS patch site differs from v01.21 original bytes");
            std::memcpy(site,patch_bytes,sizeof(patch_bytes));
            std::cout<<"{\"scope\":\"guest_60fps_patch_applied\",\"target\":20618624,\"bytes\":7,\"origin\":\"GoldHEN_Patch_Repository 60 FPS Unlock 2.0 by illusion\"}\n"<<std::flush;
        }
    }
    // CTR_ONLINE=1: project server key in place of the auth3 traffic signing key (R8).
    apply_online_traffic_key(images.front().get(),pin);
    // CTR_NATIVE_4K=1 (with CTR_NEO_MODE=1, ctrport/GUEST_PATCHES.md):
    // 1. In the Neo branch of graphics device init (v01.00 0x12e74a8, v01.21 0x13aa638),
    //    the backbuffer at [r14+0x540..0x544] is doubled to 3840x2160 while [r14+0x548] is
    //    loaded from [r12+0x48] (4/3 = 1.3333334f), so the scene scale helper (0x131f9e0 /
    //    0x13e2ea0) returns 0.5f * 4/3 = 2/3 (2560x1440). Replacing "mov eax,[r12+0x48]" with
    //    "mov eax,0x40000000" (2.0f) sets the multiplier to 0.5f * 2.0f = 1.0f (3840x2160 3D scene).
    // 2. Scaling all 3D scene render targets from 2560x1440 to 3840x2160 (2.25x pixels) exhausts
    //    the default 960 MiB (0x3c000000) Neo Gfx/@vramBSize (Garlic pool [1]{7,Garlic}) with
    //    "OOM [1]{7,Garlic}->16711680". In the Neo branch of Gfx/@vramBSize setup (v01.00
    //    0x1aacfcb, v01.21 0x1bbb918), raise the immediate from 0x3c000000 (960 MiB) to
    //    0x78000000 (1920 MiB, matching native_4k_direct_bytes in direct_memory.hpp).
    if(native_4k_selected()){
        const std::uint64_t scale_offset=(pin=="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")?0x013aa652ULL:
                                         (pin=="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1")?0x012e74c2ULL:0ULL;
        const std::uint64_t vram_b_offset=(pin=="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")?0x01bbb918ULL:
                                          (pin=="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1")?0x01aacfcbULL:0ULL;
        if(!scale_offset || !vram_b_offset) throw std::runtime_error("CTR_NATIVE_4K has no binding for this eboot");
        const unsigned char original_scale_bytes[]={0x41,0x8b,0x44,0x24,0x48,0x41,0x89,0x86,0x48,0x05,0x00,0x00};
        const unsigned char patch_scale_bytes[]={0xb8,0x00,0x00,0x00,0x40};
        const unsigned char original_vram_b_bytes[]={0xc7,0x45,0xc8,0x00,0x00,0x00,0x3c,0xe9};
        const unsigned char patch_vram_b_bytes[]={0xc7,0x45,0xc8,0x00,0x00,0x00,0x78};
        auto* scale_site=images.front().get()+scale_offset;
        auto* vram_b_site=images.front().get()+vram_b_offset;
        if(std::memcmp(scale_site,original_scale_bytes,sizeof(original_scale_bytes))!=0) throw std::runtime_error("CTR_NATIVE_4K scale patch site differs from original bytes");
        if(std::memcmp(vram_b_site,original_vram_b_bytes,sizeof(original_vram_b_bytes))!=0) throw std::runtime_error("CTR_NATIVE_4K vramBSize patch site differs from original bytes");
        std::memcpy(scale_site,patch_scale_bytes,sizeof(patch_scale_bytes));
        std::memcpy(vram_b_site,patch_vram_b_bytes,sizeof(patch_vram_b_bytes));
        std::cout<<"{\"scope\":\"guest_native_4k_patch_applied\",\"scale_target\":"<<scale_offset<<",\"vram_b_target\":"<<vram_b_offset<<",\"scene_scale\":2.0,\"garlic_bytes\":2013265920}\n"<<std::flush;
    }
    // CTR_UNLOCK_ALL=1, test-only (ctrport/GUEST_PATCHES.md, unlock_all.hpp). Only the
    // non-returning startup path keeps the image mapped for the watcher's lifetime.
    if(continue_after_libc_init){
        static const bool unlock_all=[]{char v[2]{};return GetEnvironmentVariableA("CTR_UNLOCK_ALL",v,sizeof(v))==1 && v[0]=='1';}();
        if(unlock_all) start_unlock_all(images.front().get(),pin);
    }
    for(const auto& span:metadata.front().executable_spans) {
        DWORD previous=0;
        if(!VirtualProtect(images.front().get()+span.first,static_cast<SIZE_T>(span.second-span.first),PAGE_EXECUTE_READ,&previous)) throw std::runtime_error("Entry code protection failed");
    }
    if(!FlushInstructionCache(GetCurrentProcess(),images.front().get(),0)) throw std::runtime_error("Entry instruction cache failed");
    if(continue_after_libc_init){
        unsigned matches=0;
        for(std::size_t i=1;i<metadata.size();++i)if(metadata[i].sha256=="480a769db6697a8aa3b17019f0efe60095b85f05f8bfc67f13ed3efb675e5c9c"){
            ++matches;
            // DT_INIT0, no PT_SCE_PROCPARAM; audited original startup contract.
            nptoolkit_module_state.configure([i](){
                if(!entry_images || !entry_metadata)throw std::runtime_error("Original module images expired");
                auto& image=(*entry_images)[i];const auto& meta=(*entry_metadata)[i];
                for(const auto& span:meta.executable_spans){DWORD old=0;
                    if(!VirtualProtect(image.get()+span.first,static_cast<SIZE_T>(span.second-span.first),PAGE_EXECUTE_READ,&old))throw std::runtime_error("Original module code permission failed");
                }
                if(!FlushInstructionCache(GetCurrentProcess(),image.get(),0))throw std::runtime_error("Original module instruction cache failed");
                std::cout<<"{\"scope\":\"original_nptoolkit_dt_init_begin\",\"offset\":0,\"args\":0,\"argp\":0,\"param\":0}\n"<<std::flush;
                auto* target=resolve_original_call(*entry_images,*entry_metadata,entry_metadata->front().sha256,meta.sha256,0);
                const auto result=static_cast<int>(ctr_call_original_integer3(target,0,0,0));
                std::cout<<"{\"scope\":\"original_nptoolkit_dt_init_return\",\"result\":"<<result<<"}\n"<<std::flush;return result;
            });
        }
        if(matches!=1)throw std::runtime_error("Pinned original NpToolkit2 missing/ambiguous");
    }
    if(initialize_libc_only) {
        bool initialized=false;
        for(std::size_t i=1;i<metadata.size();++i) if(metadata[i].sha256=="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816") {
            if(continue_after_libc_init && GetEnvironmentVariableA("CTR_TRACE_MSPACE",nullptr,0)) install_original_once_trace(images[i],metadata[i]);
            const auto log_game_guard=[&](const char* phase) {
                std::uint64_t guard=0,context=0;SIZE_T read=0;
                if(!ReadProcessMemory(GetCurrentProcess(),images.front().get()+0x26ee0b0,&guard,8,&read) || read!=8 || !ReadProcessMemory(GetCurrentProcess(),images.front().get()+0x26ee0d0,&context,8,&read) || read!=8) throw std::runtime_error("Original game guard readback unavailable");
                std::cout<<"{\"scope\":\"original_game_heap_guard_readback\",\"phase\":\""<<phase<<"\",\"guard_u64\":"<<guard<<",\"context\":"<<context<<"}\n"<<std::flush;
            };
            if(continue_after_libc_init) log_game_guard("before_dt_init");
            std::cout << "{\"scope\":\"original_libc_dt_init_begin\",\"offset\":0,\"args\":0,\"argp\":0,\"procparam\":0}\n" << std::flush;
            auto* target=resolve_original_call(images,metadata,pin,metadata[i].sha256,0);
            const auto result=ctr_call_original_integer3(target,0,0,0);
            std::cout << "{\"scope\":\"original_libc_dt_init_return\",\"result\":" << result << ",\"main_reached\":false}\n";
            if(continue_after_libc_init) log_game_guard("after_dt_init");
            if(result!=0) throw std::runtime_error("Original libc initializer returned failure");
            if(continue_after_libc_init) {
                // Pinned shadPS4 linker.cpp calls the extra malloc/mutex pair only
                // for a separate system libSceLibcInternal.sprx, and only when both
                // exports exist. This pinned retail libc.prx instead runs DT_INIT.
                // Its game callback and real mutex initialization already ran above.
                // No missing import is bound or given a fabricated success here.
                std::cout<<"{\"scope\":\"retail_libc_boot_variant\",\"initializer\":\"DT_INIT\",\"extra_system_libc_pair\":false}\n"<<std::flush;
                std::uint64_t callback=0;SIZE_T bytes=0;
                if(!ReadProcessMemory(GetCurrentProcess(),images[i].get()+0x121880,&callback,8,&bytes) || bytes!=8) throw std::runtime_error("Original allocator callback slot unreadable");
                std::uint64_t game_context=0;
                if(!ReadProcessMemory(GetCurrentProcess(),images.front().get()+0x26ee0d0,&game_context,8,&bytes) || bytes!=8) throw std::runtime_error("Original game heap context unreadable");
                std::cout<<"{\"scope\":\"original_game_heap_context_after_libc_init\",\"global_offset\":40820944,\"value\":"<<game_context<<"}\n"<<std::flush;
                for(const auto slot:{0x125550ULL}) {
                    std::uint64_t table=0;
                    if(!ReadProcessMemory(GetCurrentProcess(),images[i].get()+slot,&table,8,&bytes) || bytes!=8) throw std::runtime_error("Heap initialization table slot unreadable");
                    std::cout<<"{\"scope\":\"original_heap_init_table\",\"slot_offset\":"<<slot<<",\"is_null\":"<<(table?"false":"true")<<"}\n";
                    if(table) {
                        std::uint64_t words[7]{};
                        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(table),words,sizeof(words),&bytes) || bytes!=sizeof(words)) throw std::runtime_error("Heap initialization table unreadable");
                        for(unsigned field=0;field<7;++field) {
                            std::cout<<"{\"scope\":\"original_heap_init_table_field\",\"slot_offset\":"<<slot<<",\"field\":"<<field<<",\"value\":"<<words[field];
                            for(std::size_t owner=0;owner<images.size();++owner) {
                                const auto base=reinterpret_cast<std::uint64_t>(images[owner].get());
                                for(const auto& span:metadata[owner].executable_spans) if(words[field]>=base+span.first && words[field]<base+span.second) std::cout<<",\"image_sha256\":\""<<metadata[owner].sha256<<"\",\"code_offset\":"<<(words[field]-base);
                            }
                            std::cout<<"}\n";
                        }
                    }
                }
                bool located=false;
                for(std::size_t owner=0;owner<images.size();++owner) {
                    const auto base=reinterpret_cast<std::uint64_t>(images[owner].get());
                    for(const auto& span:metadata[owner].executable_spans) if(callback>=base+span.first && callback<base+span.second) {
                        std::cout<<"{\"scope\":\"original_allocator_callback_readback\",\"slot_offset\":1185920,\"target_image_sha256\":\""<<metadata[owner].sha256<<"\",\"target_offset\":"<<(callback-base)<<"}\n"<<std::flush;
                        located=true;
                    }
                }
                if(!located) throw std::runtime_error("Original allocator callback outside original code");
            }
            initialized=true;
            if(!continue_after_libc_init) return 0;
            break;
        }
        if(!initialized) throw std::runtime_error("Initializer libc pin missing");
    }
    struct Startup { int argc; unsigned padding; const char* argv[33]; std::uint64_t entry; };
    Startup startup{};startup.argc=1;startup.argv[0]="/app0/eboot.bin";startup.entry=boundary_source+0x80;
    std::cout.flush();
    if(continue_after_libc_init) {
        GuestStack initial_stack(1024*1024,4096);
        std::cout<<"{\"scope\":\"original_startup_committed_stack\",\"usable_bytes\":"<<initial_stack.size()<<",\"guard_bytes\":4096,\"main_reached\":false}\n"<<std::flush;
        ctr_jump_startup_owned(images.front().get()+0x80,&startup,functions.memory.get(),initial_stack.top(),initial_stack.bottom());
    }else ctr_jump_startup(images.front().get()+0x80,&startup,functions.memory.get());
    throw std::runtime_error("Original entry unexpectedly returned");
}
}
