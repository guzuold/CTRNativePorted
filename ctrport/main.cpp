#include "settings/hot_trace.hpp"
#include "settings/startup.hpp"
#include "video_clock_binding.hpp"
#include "gnm_frame_end.hpp"
#include "flip_surface_lock.hpp"
#include "prepare_flip.hpp"
// SPDX-License-Identifier: GPL-2.0-only
#include <cstdint>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include "direct_mapping_lease.hpp"
#include "video_label_memory.hpp"
#include "gnm_submission_snapshot.hpp"
#include "gpu_resident_memory.hpp"
#include "batch_map.hpp"
#include "exception_dispatch_control.hpp"
#include <string>
#include "app0.hpp"
#include "display_event_queue.hpp"
#include "display_event_host.hpp"
#include "kernel_equeue.hpp"
#include "service_generation.hpp"
#include "service_abi_validation.hpp"
#include "service_module_memory.hpp"
#include "service_dll.hpp"
#include "service_integer_dispatch.hpp"
#include "service_integer9_control.hpp"
#include "service_import_record.hpp"
#include "service_record_registry.hpp"
#include "service_write_host.hpp"
#include "service_write_dll_control.hpp"
#include "service_slot_admission.hpp"
#include "service_retail_slot_control.hpp"
#include "flat_loader.hpp"
#include "original_binding.hpp"
#include "missing_import.hpp"
#include "bank_dispatch.hpp"
#include <algorithm>
#include "entry_boundary.hpp"
#include "octr_mods.hpp"
#include "single_r32_draw_control.hpp"
#include "gnm_flip_tail.hpp"
#include "gnm_write_data.hpp"
#include "color_guest_writeback.hpp"
#include "depth_guest_writeback.hpp"
#include "guest_copy_host.hpp"
#include "guest_copy_images.hpp"
#include "owner_dispatch_host.hpp"
#include "original_call.hpp"
#include "original_call_session.hpp"
#include "original_register_call.hpp"
#include "process_guard.hpp"
#include "kernel_time.hpp"
#include "system_parameters.hpp"
#include "posix_clock.hpp"
#include "local_time.hpp"
#include "kernel_mutex.hpp"
#include "thread_attributes.hpp"
#include "tls_storage.hpp"
#include "worker_control.hpp"
#include "guest_stack.hpp"
#include "guest_worker.hpp"
#include "thread_create_domain.hpp"
#include "thread_completion.hpp"
#include "thread_identity.hpp"
#include "net_connection_state.hpp"
#include "thread_keys.hpp"
#include "cancellation_control.hpp"
#include "cancellation_lifecycle_control.hpp"
#include "tsc_clock.hpp"
#include "user_service.hpp"
#include "cxa_guard.hpp"
#include "cxa_atexit.hpp"
#include "net_pool.hpp"
#include "ssl_context.hpp"
#include "http_context.hpp"
#include "np_web_context.hpp"
#include "flexible_memory.hpp"
#include "kernel_semaphore.hpp"
#include "semaphore_queue_control.hpp"
#include "common_dialog.hpp"
#include "ime_backend.hpp"
#include "save_data_dialog.hpp"
#include "kernel_condition.hpp"
#include "kernel_mutex_attributes.hpp"
#include "random_service.hpp"
#include "sysmodule_service.hpp"
#include "json2_compat.hpp"
#include "font_memory.hpp"
#include "font_guest_memory.hpp"
#include "font_ft_selection.hpp"
// Explicit user-authorized offline R8 exception, 2026-10-11.
// Public layout/result reference: shadPS4 e3ce810 shareplay.h/cpp (GPL-2.0-or-later).
extern "C" int ctr_shareplay_info_compat(void* output,std::uint64_t caller) {
    unsigned char dormant[56]{};
    try{ctrport::require_time_output(output,sizeof(dormant));}catch(const std::exception&){std::fprintf(stderr,"NID=+MCXJlWdi+s library=libSceSharePlay caller=0x%llx invalid compatibility output\n",static_cast<unsigned long long>(caller));ExitProcess(4);}
    SIZE_T written=0;
    if(!output || !WriteProcessMemory(GetCurrentProcess(),output,dormant,sizeof(dormant),&written) || written!=sizeof(dormant)) {
        std::fprintf(stderr,"NID=+MCXJlWdi+s library=libSceSharePlay caller=0x%llx invalid compatibility output\n",static_cast<unsigned long long>(caller));ExitProcess(4);
    }
    return ctr_json2_compat(19,caller);
}
extern "C" int ctr_json2_stub(unsigned index,std::uint64_t caller) {return ctr_json2_compat(index,caller);}
#include "direct_memory.hpp"
#include "direct_memory_copy.hpp"
#include "gnm_init_dispatch.hpp"
#include "video_session_owner.hpp"
#include "thread_destructor.hpp"
#include "thread_atexit.hpp"
#include "application_heap.hpp"
#include "heap_trace.hpp"
#include "original_call_session.hpp"
// Public reference: pinned shadPS4 kernel.cpp87-90, GPL-2.0-or-later.
// Independent per-host-thread guest errno cell; error conversion is separate.
extern "C" int* ctr_guest_errno() {static thread_local int value=0;return &value;}
extern "C" void ctr_sysv_guest_errno();
extern "C" void ctr_original_integer_target_control();
extern "C" int ctr_font_backend_initialize();
extern "C" bool ctr_font_backend_ready();
extern "C" bool ctr_font_backend_opentype_ready();
extern "C" int ctr_font_memory_init(ctrport::FontMem* memory,void* region,std::uint32_t size,const ctrport::FontMemInterface* iface,void* mspace,void* destroy,void* context,std::uint64_t caller){
 try{return ctrport::font_guest_memory_init(memory,region,size,iface,mspace,destroy,context);}
 catch(const std::exception& e){std::cerr<<"NID=whrS4oksXc4 library=libSceFont caller="<<caller<<" unsupported: "<<e.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_font_memory_term(ctrport::FontMem* memory,std::uint64_t caller){
 try{return ctrport::font_guest_memory_term(memory);}
 catch(const std::exception& e){std::cerr<<"NID=h6hIgxXEiEc library=libSceFont caller="<<caller<<" unsupported: "<<e.what()<<'\n';ExitProcess(4);}
}
extern "C" const void* ctr_font_select_library(int value){return ctrport::font_select_library(value);}
extern "C" const void* ctr_font_select_renderer(int value){return ctrport::font_select_renderer(value);}
extern "C" void ctr_font_callback_missing(unsigned renderer,std::uint64_t caller){std::cerr<<"NID="<<(renderer?"Xx974EW-QFY":"oM+XCzVG3oM")<<" library=libSceFontFt caller="<<caller<<" unsupported: "<<(renderer?"renderer":"library")<<" callback implementation missing\n";ExitProcess(4);}
extern "C" int ctr_sysmodule_load(unsigned short id,std::uint64_t caller) {
    if((id==0xb2 || id==0xa2) && ctrport::hle_library) {
        const auto load=reinterpret_cast<int(*)(int(*)(int))>(GetProcAddress(ctrport::hle_library,id==0xb2?"ctr_hle_profile_load":"ctr_hle_invitation_load"));
        if(load && load([](int op)->int {
            if(op==0)return ctrport::common_dialog.ready()?1:0;
            if(op==1)return ctrport::common_dialog.claim()?1:0;
            if(op==2){ctrport::common_dialog.release();return 0;}
            return -1;
        })==0)return 0;
    }
    try {if(id==0x96){const auto rc=ctrport::ime_text_backend.load();std::cout<<"{\"scope\":\"ime_backend_module_initialize\",\"module_id\":150,\"native_unicode_edit_ready\":true,\"guest_dialog_exports_bound\":false}\n"<<std::flush;return rc;}if(id==0x83)return ctrport::playgo_metadata.load(ctrport::kernel_files.metadata_path("/app0/sce_sys/playgo-chunk.dat"));if(id==0xb4)return ctrport::app_content.load(ctrport::kernel_files.metadata_path("/app0/sce_sys/param.sfo"));if(id==0x84 || id==0x98 || id==0x99){const auto rc=ctr_font_backend_initialize();if(rc || !ctr_font_backend_ready() || (id==0x99 && !ctr_font_backend_opentype_ready()))throw std::runtime_error("FreeType backend initialization failed");std::cout<<"{\"scope\":\"font_backend_module_initialize\",\"module_id\":"<<id<<",\"native_backend_ready\":true,\"full_font_service_support\":false}\n"<<std::flush;return 0;}if(id==0xe7)return ctr_json2_compat(7,caller);if(id==0x19)return ctr_json2_compat(10,caller);if(id==0x9d)return ctr_json2_compat(11,caller);if(id==0xad)return ctr_json2_compat(12,caller);if(id==0xd3)return ctr_json2_compat(13,caller);if(id==0x81)return ctr_json2_compat(15,caller);if(id==0x9c)return ctr_json2_compat(17,caller);if(id==0xa8)return ctr_json2_compat(18,caller);const auto result=id==0x102?ctrport::nptoolkit_module_state.load():id==0xa0?ctrport::load_save_dialog_module():ctrport::random_module_state.load(id);std::cout<<"{\"scope\":\"sysmodule_activation\",\"id\":"<<id<<",\"result\":"<<result<<",\"concrete_module_exports\":true}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=g8cM39EUZ6o library=libSceSysmodule caller=0x"<<std::hex<<caller<<" module=0x"<<id<<" module_decimal="<<std::dec<<id<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_direct_memory_query(std::uint64_t offset,int flags,ctrport::DirectQueryInfo* output,std::uint64_t size,std::uint64_t caller) {
    try {
        if(!ctrport::direct_memory_backing) throw std::runtime_error("Direct backing unavailable");
        const auto result=ctrport::direct_memory_backing->query(offset,flags,output,size);
        std::cout<<"{\"scope\":\"direct_memory_query\",\"result\":"<<result<<",\"offset\":"<<offset<<",\"flags\":"<<flags<<",\"size\":"<<size<<"}\n"<<std::flush;return result;
    } catch(const std::exception& error) {std::cerr<<"NID=BHouLQzh0X0 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_sysv_direct_memory_size();
extern "C" std::uint64_t ctr_direct_memory_size(std::uint64_t caller) {
    try {const auto value=ctrport::direct_memory_size();std::cout<<"{\"scope\":\"direct_memory_size\",\"reserved_bytes\":"<<value<<"}\n"<<std::flush;return value;}
    catch(const std::exception& error) {std::cerr<<"NID=pO96TwzOm5E library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_sysv_sysmodule_load();
extern "C" void ctr_sysv_random_bytes();
extern "C" int ctr_random_bytes(unsigned char* output,std::size_t size,std::uint64_t caller) {
    try {return ctrport::random_bytes(output,size);}
    catch(const std::exception& error) {std::cerr<<"NID=PI7jIZj4pcE library=libSceRandom caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_lock(std::uint64_t* cell,std::uint64_t caller) {
    try { const auto result=ctrport::kernel_mutex_lock(cell);if(ctrport::settings::hot_trace()){std::cout<<"{\"scope\":\"kernel_mutex_lock\",\"result\":"<<result<<"}\n"<<std::flush;}return result; }
    catch(const std::exception& error) {std::cerr<<"NID=7H0iTOciTLo library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_unlock(std::uint64_t* cell,std::uint64_t caller) {
    try { const auto result=ctrport::kernel_mutex_unlock(cell);if(ctrport::settings::hot_trace()){std::cout<<"{\"scope\":\"kernel_mutex_unlock\",\"result\":"<<result<<"}\n"<<std::flush;}return result; }
    catch(const std::exception& error) {std::cerr<<"NID=2Z+PpY6CaJg library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}

extern "C" int ctr_orbis_condition_broadcast(std::uint64_t* cell,std::uint64_t caller,std::uint64_t original_frame) {
    try {ctrport::observe_condition("orbis_broadcast",caller,cell,nullptr);ctrport::observe_condition_frames(original_frame);return ctrport::kernel_condition_broadcast(cell);}
    catch(const std::exception& error) {std::cerr<<"NID=JGgj7Uvrl+A library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_condition_broadcast(std::uint64_t* cell,std::uint64_t caller,std::uint64_t original_frame) {
    try { ctrport::observe_condition("broadcast",caller,cell,nullptr);ctrport::observe_condition_frames(original_frame);const auto result=ctrport::kernel_condition_broadcast(cell);std::cout<<"{\"scope\":\"kernel_condition_broadcast\",\"result\":"<<result<<"}\n"<<std::flush;return result; }
    catch(const std::exception& error) {std::cerr<<"NID=mkx2fVhNMsg library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_orbis_mutex_trylock(std::uint64_t* cell,std::uint64_t caller) {
 try{const int error=ctrport::kernel_mutex_trylock(cell);const int result=error?static_cast<int>(0x80020000U+static_cast<unsigned>(error)):0;
  if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"original_OrbisMutexTrylock_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);}return result;
 }catch(const std::exception& error){std::cerr<<"NID=upoVrzMHFeE library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_posix_mutex_trylock(std::uint64_t* cell,std::uint64_t caller) {
    try{return ctrport::kernel_mutex_trylock(cell);}catch(const std::exception& error){std::cerr<<"NID=K-jXhbt2gn4 library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_posix_mutexattr_destroy(std::uint64_t* cell,std::uint64_t caller) {
    try{auto result=ctrport::kernel_mutex_attributes_destroy(cell);return result==static_cast<int>(0x80020016U)?22:result;}
    catch(const std::exception& error){std::cerr<<"NID=HF7lK46xzjY library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_posix_mutex_init(std::uint64_t* cell,const std::uint64_t* attributes,std::uint64_t caller) {
    try{return ctrport::kernel_mutex_init(cell,attributes,nullptr);}catch(const std::bad_alloc&){return 12;}
    catch(const std::exception& error){std::cerr<<"NID=ttHNfU+qDBU library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_posix_mutexattr_settype(std::uint64_t* cell,unsigned type,std::uint64_t caller) {
    try{auto result=ctrport::kernel_mutex_attributes_settype(cell,type);return result==static_cast<int>(0x80020016U)?22:result;}
    catch(const std::exception& error){std::cerr<<"NID=mDmgMOGVUqg library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_posix_mutexattr_init(std::uint64_t* cell,std::uint64_t caller) {
    try{return ctrport::kernel_mutex_attributes_init(cell);}catch(const std::bad_alloc&){return 12;}
    catch(const std::exception& error){std::cerr<<"NID=dQHWEsJtoE4 library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_attributes_init(std::uint64_t* cell,std::uint64_t caller) {
    try { const auto result=ctrport::kernel_mutex_attributes_init(cell);std::cout<<"{\"scope\":\"kernel_mutex_attributes_init\",\"result\":"<<result<<"}\n"<<std::flush;return result; }
    catch(const std::bad_alloc&) {return static_cast<int>(0x8002000cU);}
    catch(const std::exception& error) {std::cerr<<"NID=F8bUHwAG284 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_attributes_settype(std::uint64_t* cell,std::uint32_t type,std::uint64_t caller) {
    try {const auto result=ctrport::kernel_mutex_attributes_settype(cell,type);std::cout<<"{\"scope\":\"kernel_mutex_attributes_settype\",\"type\":"<<type<<",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=iMp8QpE+XO4 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_init(std::uint64_t* cell,const std::uint64_t* attributes,const char* name,std::uint64_t caller) {
    try {const auto result=ctrport::kernel_mutex_init(cell,attributes,name);std::cout<<"{\"scope\":\"kernel_mutex_init\",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::bad_alloc&) {return static_cast<int>(0x8002000cU);}
    catch(const std::exception& error) {std::cerr<<"NID=cmo1RIYva9o library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_attributes_destroy(std::uint64_t* cell,std::uint64_t caller) {
    try {const auto result=ctrport::kernel_mutex_attributes_destroy(cell);std::cout<<"{\"scope\":\"kernel_mutex_attributes_destroy\",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=smWEktiyyG0 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
namespace {
const LARGE_INTEGER ctr_process_epoch=[] {LARGE_INTEGER value{};if(!QueryPerformanceCounter(&value))std::terminate();return value;}();
const LARGE_INTEGER ctr_process_frequency=[] {LARGE_INTEGER value{};if(!QueryPerformanceFrequency(&value) || value.QuadPart<=0)std::terminate();return value;}();
}
extern "C" void ctr_sysv_process_time();
extern "C" std::uint64_t ctr_kernel_process_time() {
    LARGE_INTEGER now{};if(!QueryPerformanceCounter(&now))ExitProcess(4);
    const auto ticks=static_cast<std::uint64_t>(now.QuadPart-ctr_process_epoch.QuadPart);
    const auto frequency=static_cast<std::uint64_t>(ctr_process_frequency.QuadPart);
    return (ticks/frequency)*1000000ULL+((ticks%frequency)*1000000ULL)/frequency;
}
extern "C" void ctr_sysv_gettimeofday();
extern "C" int ctr_kernel_gettimeofday(ctrport::KernelTimeval* value,ctrport::KernelTimezone* zone,std::uint64_t caller) {
    // No C++ exceptions may cross the SysV boundary.
    try { return ctrport::kernel_gettimeofday(value,zone); }
    catch(const std::exception& error) {std::cerr<<"NID=n88vx3C5nW8 library=libkernel caller=0x"<<std::hex<<caller<<" gettimeofday unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_orbis_gettimeofday(ctrport::KernelTimeval* value,std::uint64_t caller){
 try{const int result=ctrport::kernel_gettimeofday(value,nullptr);if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"owned_orbis_gettimeofday\",\"nid\":\"ejekcaNQNq0\",\"library\":\"libkernel\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);}return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=ejekcaNQNq0 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" unsigned ctr_original_xmm_control();
extern "C" std::uint64_t ctr_call_sysv_probe(std::uint64_t, std::uint64_t);
extern "C" std::uint64_t ctr_msvc_probe(std::uint64_t a, std::uint64_t b) {
    return (a ^ 0x1122334455667788ULL) + b;
}
extern "C" void ctr_startup_control(const void*, std::uint64_t, std::uint64_t*);
extern "C" void ctr_dtor_control_increment() {++ctrport::destructor_control_count;}
extern "C" void ctr_thread_destructor_set(void* callback,std::uint64_t caller) {
    try {ctrport::thread_destructors.set(callback);}
    catch(const std::exception& error) {std::cerr<<"NID=rNhWz+lvOMU library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_thread_atexit_set(void* callback,std::uint64_t caller) {
    try {ctrport::thread_atexit.set(callback);}
    catch(const std::exception& error) {std::cerr<<"NID=pB-yGZ2nQ9o library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_atexit_report_capture(int value) {ctrport::report_discriminator=value;}
extern "C" void ctr_thread_atexit_report_set(void* callback,std::uint64_t caller) {
    try {ctrport::thread_atexit_report.set(callback);}
    catch(const std::exception& error) {std::cerr<<"NID=WhCc1w3EhSI library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" const void* ctr_get_proc_param(std::uint64_t caller) {
    if(!ctrport::process_parameter) {std::cerr<<"NID=959qrazPIrg library=libkernel caller=0x"<<std::hex<<caller<<" process parameter unavailable\n";ExitProcess(4);}
    std::cout<<"{\"scope\":\"original_mapped_process_parameter\",\"original_size\":88,\"sdk\":100696065}\n"<<std::flush;
    return ctrport::process_parameter;
}
extern "C" void ctr_application_heap_set(const void* table,std::uint64_t caller) {
    try {ctrport::application_heap.set(table);}
    catch(const std::exception& error) {std::cerr<<"NID=p5EcQeEeJAE library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_heap_trace_query(void* output,std::uint64_t caller) {
    try {ctrport::heap_trace.query(output);}
    catch(const std::exception& error) {std::cerr<<"NID=NWtTN10cJzE library=libSceLibcInternalExt caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_is_neo_mode(std::uint64_t caller) {
    try {return ctrport::process_neo_mode();}
    catch(const std::exception& error) {std::cerr<<"NID=WslcK1FQcGI library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_allocate_direct(std::int64_t start,std::int64_t end,std::uint64_t bytes,std::uint64_t alignment,int type,std::int64_t* output,std::uint64_t caller) {
    try {
        if(!ctrport::direct_memory_backing) throw std::runtime_error("Direct allocation owner unavailable");
        const auto result=ctrport::direct_memory_backing->allocate(start,end,bytes,alignment,type,output);
        std::cout<<"{\"scope\":\"owned_direct_allocation\",\"bytes\":"<<bytes<<",\"alignment\":"<<alignment<<",\"type\":"<<type<<",\"result\":"<<result<<"}\n"<<std::flush;
        return result;
    } catch(const std::exception& error) {std::cerr<<"NID=rTXw65xmLIA library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_map_direct(void** output,std::uint64_t bytes,int protection,int flags,std::int64_t physical,std::uint64_t alignment,std::uint64_t caller) {
    try {
        if(!ctrport::direct_memory_backing) throw std::runtime_error("Direct map owner unavailable");
        const auto result=ctrport::direct_memory_backing->map(output,bytes,protection,flags,physical,alignment);
        if(!result)ctrport::record_query_mapping(reinterpret_cast<uint64_t>(*output),bytes,protection);
        std::cout<<"{\"scope\":\"owned_direct_map\",\"bytes\":"<<bytes<<",\"result\":"<<result<<"}\n"<<std::flush;return result;
    } catch(const std::exception& error) {std::cerr<<"NID=L-Q3LEjIbgA library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" unsigned ctr_asan_enabled() {
#if defined(__SANITIZE_ADDRESS__)
    return 1;
#else
    return 0;
#endif
}
extern "C" int ctr_orbis_mutex_lock(std::uint64_t* cell,std::uint64_t caller) {
    try {ctrport::observe_mutex_call(false,caller,cell);const auto error=ctrport::kernel_mutex_lock(cell);return error?static_cast<int>(0x80020000U+static_cast<unsigned>(error)):0;}
    catch(const std::exception& error) {std::cerr<<"NID=9UK1vLZQft4 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_orbis_mutex_unlock(std::uint64_t* cell,std::uint64_t caller) {
    try {ctrport::observe_mutex_call(true,caller,cell);const auto error=ctrport::kernel_mutex_unlock(cell);return error?static_cast<int>(0x80020000U+static_cast<unsigned>(error)):0;}
    catch(const std::exception& error) {std::cerr<<"NID=tn3VlD0hG60 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_thread_attr_destroy();
extern "C" int ctr_posix_thread_attr_destroy(std::uint64_t* cell,std::uint64_t caller) {
    try {const auto rc=ctrport::thread_attributes_destroy(cell);return rc?static_cast<int>(static_cast<unsigned>(rc)&0xffffU):0;}
    catch(const std::exception& error){std::cerr<<"NID=zHchY8ft5pk library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_thread_attr_stack_size();
extern "C" int ctr_posix_thread_attr_stack_size(std::uint64_t* cell,std::uint64_t size,std::uint64_t caller) {
    try {const auto rc=ctrport::thread_attributes_set_stack_size(cell,size);return rc?static_cast<int>(static_cast<unsigned>(rc)&0xffffU):0;}
    catch(const std::exception& error){std::cerr<<"NID=2Q0z6rnBrTE library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_thread_attr_detach();
extern "C" int ctr_posix_thread_attr_detach(std::uint64_t* cell,int state,std::uint64_t caller) {
    try {const auto rc=ctrport::thread_attributes_set_detach(cell,state);return rc?static_cast<int>(static_cast<unsigned>(rc)&0xffffU):0;}
    catch(const std::exception& error){std::cerr<<"NID=E+tyo3lp5Lw library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_thread_attr_init();
extern "C" int ctr_posix_thread_attr_init(std::uint64_t* cell,std::uint64_t caller) {
    try {return ctrport::thread_attributes_init(cell);}catch(const std::bad_alloc&){return 12;}
    catch(const std::exception& error){std::cerr<<"NID=wtkt-teR1so library=libScePosix caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_attributes_init(std::uint64_t* cell,std::uint64_t caller) {
    try {return ctrport::thread_attributes_init(cell);}
    catch(const std::exception& error) {std::cerr<<"NID=nsYoNRywwNg library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" std::uint64_t ctr_thread_self() {return ctrport::thread_self();}
extern "C" int ctr_thread_attributes_current(std::uint64_t handle,std::uint64_t* cell,std::uint64_t caller) {
    try {return ctrport::thread_attributes_current(handle,cell);}
    catch(const std::exception& error) {std::cerr<<"NID=x1X76arYMxU library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_attributes_affinity(std::uint64_t* cell,std::uint64_t* output,std::uint64_t caller) {
    try {return ctrport::thread_attributes_affinity(cell,output);}
    catch(const std::exception& error) {std::cerr<<"NID=8+s5BzZjxSg library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_attributes_destroy(std::uint64_t* cell,std::uint64_t caller) {
    try {return ctrport::thread_attributes_destroy(cell);}
    catch(const std::exception& error) {std::cerr<<"NID=62KCwEMmzcM library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_key_create(std::uint32_t* output,std::uint64_t destructor,std::uint64_t caller) {
    try {return ctrport::thread_keys.create(output,destructor);}
    catch(const std::exception& error) {std::cerr<<"NID=geDaqgH9lTg library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" const void* ctr_thread_key_get(std::uint32_t key) {return ctrport::thread_keys.get(key);}
extern "C" int ctr_thread_key_set(std::uint32_t key,const void* value) {return ctrport::thread_keys.set(key,value);}
extern "C" int ctr_thread_key_delete(std::uint32_t key) {return ctrport::thread_keys.erase(key);}
extern "C" int ctr_cpu_mode(std::uint64_t caller) {
    try {return ctrport::process_cpu_mode();}
    catch(const std::exception& error) {std::cerr<<"NID=VOx8NGmHXTs library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_set_affinity(std::uint64_t handle,std::uint64_t mask,std::uint64_t caller) {
    try {if(auto worker=ctrport::find_startup_worker(handle)){worker->set_host_affinity(static_cast<DWORD_PTR>(mask));ctrport::record_thread_affinity(handle,mask);return 0;}return ctrport::thread_set_affinity(handle,mask);}
    catch(const std::exception& error) {std::cerr<<"NID=bt3CTBKmGyI library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" std::uint64_t ctr_tsc_frequency(std::uint64_t caller) {
    try {return ctrport::native_tsc_frequency();}
    catch(const std::exception& error) {std::cerr<<"NID=1j3S3n-tTW4 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" std::uint64_t ctr_read_tsc() {return ctrport::fenced_tsc();}
extern "C" int ctr_user_service_initialize(const void* params,std::uint64_t caller) {
    try {return ctrport::user_service.initialize(params);}
    catch(const std::exception& error) {std::cerr<<"NID=j3YMu1MVNNo library=libSceUserService caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
#include "semaphore_call_trace.hpp"
extern "C" int ctr_semaphore_create(std::uint32_t* output,const char* name,unsigned attr,int initial,int maximum,const void* options,std::uint64_t caller) {
    try {return ctrport::kernel_semaphores.create(output,name,attr,initial,maximum,options);}
    catch(const std::exception& error) {std::cerr<<"NID=188x57JYp0g library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_semaphore_wait(std::uint32_t id,int need,std::uint32_t* timeout,std::uint64_t caller) {
    try {ctrport::trace_semaphore_call(id,caller,1,0,need);const int result=ctrport::kernel_semaphores.wait(id,need,timeout);ctrport::trace_semaphore_call(id,caller,2,result,need);return result;}
    catch(const std::exception& error) {std::fprintf(stderr,"NID=Zxa0VhQVTsk library=libkernel caller=0x%llx host_thread=%lu unsupported: %s\n",static_cast<unsigned long long>(caller),GetCurrentThreadId(),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_convert_local(std::int64_t utc,std::int64_t* local,ctrport::LocalTimeInfo* info,std::uint64_t* dst,std::uint64_t caller) {
    try{return ctrport::convert_local(utc,local,info,dst);}catch(const std::exception& error){std::cerr<<"NID=-o5uEDpN+oY library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_posix_clock(unsigned id,ctrport::PosixTimespec* output,std::uint64_t caller) {
    try{return ctrport::posix_clock(id,output);}catch(const std::exception& error){std::cerr<<"NID=lLMT9vJAck0 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
// Public kernel/time.cpp288: POSIX clock result with errno->SCE conversion.
extern "C" int ctr_kernel_clock(unsigned id,ctrport::PosixTimespec* output,uint64_t caller){
 try{const int result=ctrport::posix_clock(id,output);const int rc=result<0?static_cast<int>(0x80020000U+static_cast<unsigned>(*ctr_guest_errno())):0;
 if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"original_KernelClockGettime_return\",\"clock_id\":%u,\"result\":%d,\"caller\":%llu}\n",id,rc,static_cast<unsigned long long>(caller));std::fflush(stdout);}return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=QBi7HCK03hw library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
// Explicit unavailable-network error, from shadPS4 netctl.cpp offline branch.
// No host network interface or PSN connection is owned by this runtime yet.
extern "C" int ctr_netctl_info(int code,void*,std::uint64_t caller) {
    constexpr unsigned result=0x80412108U;
    std::fprintf(stderr,"NID=obuxdTiwkF8 library=libSceNetCtl caller=0x%llx code=%d result=0x%x unsupported: runtime network connection unavailable; no info output written\n",static_cast<unsigned long long>(caller),code,result);std::fflush(stderr);
    return static_cast<int>(result);
}
extern "C" int ctr_system_parameter(int id,int* output,std::uint64_t caller) {
    try {auto result=ctrport::system_parameter(id,output);std::fprintf(stderr,"NID=fZo48un7LK4 library=libSceSystemService caller=0x%llx param=%d result=0x%x\n",static_cast<unsigned long long>(caller),id,result);return result;}
    catch(const std::exception& error) {std::cerr<<"NID=fZo48un7LK4 library=libSceSystemService caller=0x"<<std::hex<<caller<<std::dec<<" param="<<id<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_semaphore_poll(std::uint32_t id,int need,std::uint64_t caller) {
    try {return ctrport::kernel_semaphores.poll(id,need);}
    catch(const std::exception& error) {std::cerr<<"NID=12wOHk8ywb0 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_semaphore_signal(std::uint32_t id,int count,std::uint64_t caller) {
    try {ctrport::trace_semaphore_call(id,caller,3,0,count);const int result=ctrport::kernel_semaphores.signal(id,count);ctrport::trace_semaphore_call(id,caller,4,result,count);return result;}
    catch(const std::exception& error) {std::cerr<<"NID=4czppHBiriw library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_common_dialog_initialize() {return ctrport::common_dialog.initialize();}
extern "C" int ctr_condition_init(std::uint64_t* cell,const std::uint64_t* attributes,const char* name,std::uint64_t caller) {
    try {const auto result=ctrport::kernel_condition_init(cell,attributes,name);std::cout<<"{\"scope\":\"condition_init\",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::bad_alloc&) {return static_cast<int>(0x8002000cU);}
    catch(const std::exception& error) {std::cerr<<"NID=2Tb92quprl0 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_condition_attributes_destroy(std::uint64_t* cell,std::uint64_t caller) {
    try {const auto result=ctrport::condition_attributes_destroy(cell);std::cout<<"{\"scope\":\"condition_attributes_destroy\",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=waPcxYiR3WA library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_stack_size(std::uint64_t* cell,std::uint64_t size,std::uint64_t caller) {
    try {const auto result=ctrport::thread_attributes_set_stack_size(cell,size);std::cout<<"{\"scope\":\"thread_stack_size\",\"size\":"<<size<<",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=UTXzJbWhhTE library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_policy(std::uint64_t* cell,int policy,std::uint64_t caller) {
    try {const auto result=ctrport::thread_attributes_set_policy(cell,policy);std::cout<<"{\"scope\":\"thread_policy_attribute\",\"policy\":"<<policy<<",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=4+h9EzwKF4I library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_priority(std::uint64_t* cell,const int* parameter,std::uint64_t caller) {
    try {const auto result=ctrport::thread_attributes_set_priority(cell,parameter);std::cout<<"{\"scope\":\"thread_priority_attribute\",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=DzES9hQF4f4 library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_thread_inherit(std::uint64_t* cell,int inherit,std::uint64_t caller) {
    try {const auto result=ctrport::thread_attributes_set_inherit(cell,inherit);std::cout<<"{\"scope\":\"thread_inherit_attribute\",\"inherit\":"<<inherit<<",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::exception& error) {std::cerr<<"NID=eXbUSpEaTsA library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_condition_attributes_init(std::uint64_t* cell,std::uint64_t caller) {
    try {const auto result=ctrport::condition_attributes_init(cell);std::cout<<"{\"scope\":\"condition_attributes_init\",\"result\":"<<result<<"}\n"<<std::flush;return result;}
    catch(const std::bad_alloc&) {return static_cast<int>(0x8002000cU);}
    catch(const std::exception& error) {std::cerr<<"NID=m5-2bsNfv7s library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_save_dialog_initialize() {return ctrport::save_data_dialog.initialize();}
extern "C" int ctr_save_dialog_status() {return ctrport::save_data_dialog.update_status();}
extern "C" int ctr_save_dialog_terminate() {return ctrport::save_data_dialog.terminate();}
// Isolated control context only; no startup service binding.

struct CtrTlsIndex {std::uint64_t module,offset;};
extern "C" void ctr_sysv_tls_address();
extern "C" void* ctr_tls_address(const CtrTlsIndex* index,std::uint64_t caller) {
    try {
        if(!ctrport::current_tls_storage || !index) throw std::runtime_error("TLS context/index missing");
        MEMORY_BASIC_INFORMATION region{};
        const auto address=reinterpret_cast<std::uintptr_t>(index);
        if(!VirtualQuery(index,&region,sizeof(region)) || region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS))) throw std::runtime_error("TLS index unreadable");
        const auto protection=region.Protect&0xff;
        if(protection!=PAGE_READONLY && protection!=PAGE_READWRITE && protection!=PAGE_WRITECOPY && protection!=PAGE_EXECUTE_READ && protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY) throw std::runtime_error("TLS index permission rejected");
        const auto base=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if(address<base || address-base>region.RegionSize || sizeof(CtrTlsIndex)>region.RegionSize-(address-base)) throw std::runtime_error("TLS index extent rejected");
        CtrTlsIndex copy{};std::memcpy(&copy,index,sizeof(copy));
        return ctrport::current_tls_storage->address(copy.module,copy.offset);
    } catch(const std::exception& error) {std::cerr<<"NID=vNe1w4diLCs library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
// ctrport.exe <eboot> <modules...> starts the game (the launcher and play_gpu.py build this
// command line). The other two commands are maintenance tools.
int main(int argc, char** argv) {
    const std::string command = argc >= 2 ? argv[1] : "";
    if (argc == 3 && command == "--settings-reset-graphics") {
        try {
            auto changes=ctr_online::Json::object();
            changes.set("quality_preset",ctr_online::Json::of("original"));
            for(const char* key:{"window_resolution","window_mode","display_refresh","presentation_limit","hdr_output","display_gamma","hdr_peak_nits","hdr_exposure","present_mode","fsr1_sharpen"})
                changes.set(key,ctr_online::Json::of("original"));
            changes.set("upscaler",ctr_online::Json::of("off"));
            ctrport::settings::Store(std::filesystem::u8path(argv[2])).update(changes);
            std::cout << "SETTINGS_GRAPHICS_RESET backup_preserved=true\n";
            return 0;
        } catch(const std::exception& error) { std::cerr << error.what() << "\n";return 2; }
    }
    if (argc == 2 && command == "--user-name-control") return ctrport::user_name_control();
    if (argc == 3 && command == "--user-name-control") return ctrport::user_name_control(argv[2]);
    if (argc >= 4 && command == "--entry-after-libc-init-images") {
        try {
            namespace fs = std::filesystem;
            fs::path base_path = fs::path(argv[2]).parent_path();
            fs::path patch_path;
            char base_buf[32768]{}, patch_buf[32768]{};
            const DWORD base_len = GetEnvironmentVariableA("CTR_BASE_DIR", base_buf, sizeof(base_buf));
            const DWORD patch_len = GetEnvironmentVariableA("CTR_PATCH_DIR", patch_buf, sizeof(patch_buf));
            if (base_len && base_len < sizeof(base_buf)) {
                base_path = fs::path(base_buf);
            }
            if (patch_len && patch_len < sizeof(patch_buf)) {
                patch_path = fs::path(patch_buf);
            } else if (!fs::exists(base_path / "sce_module") && fs::exists(fs::path("dump/CUSA14876"))) {
                patch_path = base_path;
                base_path = fs::canonical("dump/CUSA14876");
            }
            ctrport::kernel_files.configure(base_path, patch_path);
            ctrport::kernel_files.configure_user("runtime/user/data");
            // CTR_MODS_DIR: enabled loose-file mods override /app0 files (mod_overlay.hpp).
            // octr-mod mods (mod.json): asset changes inside archives and their patch files (octr_mods.hpp).
            try { ctrport::mods::overlay.configure_from_environment({patch_path, base_path}); ctrport::octr::configure(base_path, patch_path); }
            catch (const std::exception& error) { throw std::runtime_error(std::string("Mods: ") + error.what()); }
            std::vector<ctrport::MappedImage> images;
            std::vector<ctrport::ImageMetadata> metadata;
            for (int index = 2; index < argc; ++index) {
                ctrport::ImageMetadata info;
                auto image = ctrport::load_retail_image(argv[index],false,&info);
                metadata.push_back(std::move(info));
                for (const auto& previous : images)
                    if (previous.get() == image.get())
                        throw std::runtime_error("native image reservations alias");
                images.push_back(std::move(image));
            }
            ctrport::bind_original_images(images,metadata);
            return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true);
        } catch (const std::exception& error) {
            std::cerr << "CTRPort multi-image loader: " << error.what() << '\n';
            return 4;
        }
    }
    std::cerr << "usage: ctrport.exe --entry-after-libc-init-images <eboot.bin> <sce_module/*.prx>...\n"
                 "       ctrport.exe --settings-reset-graphics <launcher_config.json>\n";
    return 4;
}
