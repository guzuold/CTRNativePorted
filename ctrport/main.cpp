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
#include "single_r32_draw_control.hpp"
#include "gnm_flip_tail.hpp"
#include "gnm_write_data.hpp"
#include "color_guest_writeback.hpp"
#include "depth_guest_writeback.hpp"
#include "guest_copy_host.hpp"
#include "guest_copy_images.hpp"
#include "owner_dispatch_host.hpp"
#include "original_call.hpp"
#include "../tools/harness/original_call_session.hpp"
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
#include "../tools/harness/original_call_session.hpp"
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
    try { const auto result=ctrport::kernel_mutex_lock(cell);std::cout<<"{\"scope\":\"kernel_mutex_lock\",\"result\":"<<result<<"}\n"<<std::flush;return result; }
    catch(const std::exception& error) {std::cerr<<"NID=7H0iTOciTLo library=libkernel caller=0x"<<std::hex<<caller<<" unsupported: "<<error.what()<<'\n';ExitProcess(4);}
}
extern "C" int ctr_kernel_mutex_unlock(std::uint64_t* cell,std::uint64_t caller) {
    try { const auto result=ctrport::kernel_mutex_unlock(cell);std::cout<<"{\"scope\":\"kernel_mutex_unlock\",\"result\":"<<result<<"}\n"<<std::flush;return result; }
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
  std::printf("{\"scope\":\"original_OrbisMutexTrylock_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;
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
 try{const int result=ctrport::kernel_gettimeofday(value,nullptr);std::printf("{\"scope\":\"owned_orbis_gettimeofday\",\"nid\":\"ejekcaNQNq0\",\"library\":\"libkernel\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);return result;}
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
 std::printf("{\"scope\":\"original_KernelClockGettime_return\",\"clock_id\":%u,\"result\":%d,\"caller\":%llu}\n",id,rc,static_cast<unsigned long long>(caller));std::fflush(stdout);return rc;
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
int main(int argc, char** argv) {
    if(argc==2 && std::string(argv[1])=="--observed-pm4-control")return ctrport::observed_pm4_control();
    if(argc==2 && std::string(argv[1])=="--direct-output-permission-control")return ctrport::direct_output_permission_control();
    if(argc==2 && std::string(argv[1])=="--shader-snapshot-owner-control")return ctrport::shader_snapshot_owner_control();
    if(argc==2 && std::string(argv[1])=="--shader-owned-ranges-control")return ctrport::shader_owned_ranges_control();
    if(argc==2 && std::string(argv[1])=="--stencil-metadata-control")return ctrport::stencil_metadata_control();
    if(argc==2 && std::string(argv[1])=="--graphics-pair-input-control")return ctrport::graphics_pair_input_control();
    if(argc==2 && std::string(argv[1])=="--video-depth-batch-control")return ctrport::video_depth_batch_control();
    if(argc==2 && std::string(argv[1])=="--video-color-batch-control")return ctrport::video_color_batch_control();
    if(argc==2 && std::string(argv[1])=="--graphics-pair-color-control")return ctrport::graphics_pair_color_control();
    if(argc==2 && std::string(argv[1])=="--video-vertex-layout-batch-control")return ctrport::video_vertex_layout_batch_control();
    if(argc==2 && std::string(argv[1])=="--vertex-only-sink-control")return ctrport::graphics_vertex_only_sink_control();
    if(argc==2 && std::string(argv[1])=="--color-sampled-gpu-control")return ctrport::color_sampled_gpu_control();
    if(argc==2 && std::string(argv[1])=="--color-sampled-r32-gpu-control")return ctrport::color_sampled_gpu_control(true);
    if(argc==2 && std::string(argv[1])=="--color-sampled-r32-half-gpu-control")return ctrport::color_sampled_gpu_control(true,true);
    if(argc==2 && std::string(argv[1])=="--color-sampled-coherence-control")return ctrport::color_sampled_coherence_control();
    if(argc==2 && std::string(argv[1])=="--graphics-auxiliary-transport-control")return ctrport::graphics_auxiliary_transport_control();
    if(argc==2 && std::string(argv[1])=="--draw-resource-inputs-control")return ctrport::draw_resource_inputs_control();
    if(argc==2 && std::string(argv[1])=="--draw-packet-inputs-control")return ctrport::draw_packet_inputs_control();
    if(argc==2 && std::string(argv[1])=="--vertex-buffer-owners-control")return ctrport::vertex_buffer_owners_control();
    if(argc==2 && std::string(argv[1])=="--graphics-push-resources-control")return ctrport::graphics_push_resources_control();
    if(argc==2 && std::string(argv[1])=="--graphics-viewport-transport-control")return ctrport::graphics_viewport_transport_control();
    if(argc==2 && std::string(argv[1])=="--graphics-metadata-host-control")return ctrport::graphics_metadata_host_control();
    if(argc==2 && std::string(argv[1])=="--gnm-event-metadata-flush-control")return ctrport::gnm_event_metadata_flush_control();
    if(argc==2 && std::string(argv[1])=="--gnm-register-executor-control")return ctrport::gnm_register_executor_control();
    if(argc==2 && std::string(argv[1])=="--gnm-flip-tail-control")return ctrport::gnm_flip_tail_control();
    if(argc==2 && std::string(argv[1])=="--gnm-write-data-control")return ctrport::gnm_write_data_control();
    if(argc==2 && std::string(argv[1])=="--compute-color-refresh-gpu-control")return ctrport::compute_color_refresh_gpu_control();
    if(argc==2 && std::string(argv[1])=="--compute-sampled-refresh-control")return ctrport::compute_sampled_refresh_control();
    if(argc==2 && std::string(argv[1])=="--gnm-write-data-gpu-control")return ctrport::gnm_write_data_gpu_control();
    if(argc==2 && std::string(argv[1])=="--gnm-command-cursor-control")return ctrport::gnm_command_cursor_control();
    if(argc==2 && std::string(argv[1])=="--graphics-pair-register-control")return ctrport::graphics_pair_register_control();
    if(argc==2 && std::string(argv[1])=="--graphics-draw-parameter-transport-control")return ctrport::graphics_draw_parameter_transport_control();
    if(argc==2 && std::string(argv[1])=="--gnm-wait-reg-mem-control")return ctrport::gnm_wait_reg_mem_control();
    if(argc==2 && std::string(argv[1])=="--graphics-pair-sink-control")return ctrport::graphics_pair_sink_control();
    if(argc==2 && std::string(argv[1])=="--graphics-stage-sink-control")return ctrport::graphics_stage_sink_control();
    if(argc==2 && std::string(argv[1])=="--graphics-extended-sink-control")return ctrport::graphics_extended_sink_control();
    if(argc==2 && std::string(argv[1])=="--large-image-snapshot-control")return ctrport::shader_large_image_snapshot_control();
    if(argc==2 && std::string(argv[1])=="--graphics-image-owner-control")return ctrport::graphics_image_owner_control();
    if(argc==2 && std::string(argv[1])=="--vertex-capture-budget-control")return ctrport::vertex_capture_budget_control();
    if(argc==2 && std::string(argv[1])=="--graphics-submission-owner-control")return ctrport::graphics_submission_owner_control();
    if(argc==2 && std::string(argv[1])=="--dispatch-packet-inputs-control")return ctrport::dispatch_packet_inputs_control();
    if(argc==2 && std::string(argv[1])=="--gnm-route-handoff-control")return ctrport::gnm_route_handoff_control();
    if(argc==2 && std::string(argv[1])=="--published-color-generations-control")return ctrport::published_color_generations_control();
    if(argc==2 && std::string(argv[1])=="--compute-large-image-writeback-control")return ctrport::compute_large_image_writeback_control();
    if(argc==2 && std::string(argv[1])=="--compute-guest-writeback-control")return ctrport::compute_guest_writeback_control();
    if(argc==2 && std::string(argv[1])=="--depth-large-guest-writeback-control")return ctrport::depth_large_guest_writeback_control();
    if(argc==2 && std::string(argv[1])=="--gnm-depth-publication-eop-gpu-control")return ctrport::gnm_depth_publication_eop_gpu_control();
    if(argc==2 && std::string(argv[1])=="--gnm-gpu-resident-eop-control")return ctrport::gnm_gpu_resident_eop_control();
    if(argc==2 && std::string(argv[1])=="--prepare-flip-control")return ctrport::prepare_flip_control();
    if(argc==2 && std::string(argv[1])=="--depth-prefix-snapshot-control")return ctrport::depth_prefix_snapshot_control();
    if(argc==2 && std::string(argv[1])=="--depth-gpu-snapshot-preflight-control")return ctrport::depth_gpu_snapshot_preflight_control();
    if(argc==2 && std::string(argv[1])=="--depth-d16-residency-control")return ctrport::depth_d16_residency_control();
    if(argc==2 && std::string(argv[1])=="--depth-single-residency-control")return ctrport::depth_single_residency_control();
    if(argc==2 && std::string(argv[1])=="--gpu-replace-extend-control")return ctrport::gpu_replace_extend_control();
    if(argc==2 && std::string(argv[1])=="--gpu-overlap-prefix-publication-control")return ctrport::gpu_overlap_prefix_publication_control();
    if(argc==2 && std::string(argv[1])=="--gpu-resident-memory-control")return ctrport::gpu_resident_memory_control();
    if(argc==2 && std::string(argv[1])=="--depth-publication-owners-control")return ctrport::depth_publication_owners_control();
    if(argc==2 && std::string(argv[1])=="--depth-guest-writeback-control")return ctrport::depth_guest_writeback_control();
    if(argc==2 && std::string(argv[1])=="--compute-stage-transport-control")return ctrport::compute_stage_transport_control();
    if(argc==2 && std::string(argv[1])=="--gnm-dma-request-control")return ctrport::gnm_dma_request_control();
    if(argc==2 && std::string(argv[1])=="--gnm-eos-completion-control")return ctrport::gnm_eos_completion_control();
    if(argc==2 && std::string(argv[1])=="--gnm-acquire-mem-control")return ctrport::gnm_acquire_mem_control();
    if(argc==2 && std::string(argv[1])=="--gnm-eop-transaction-control")return ctrport::gnm_eop_transaction_control();
    if(argc==2 && std::string(argv[1])=="--gnm-eop-gpu-control")return ctrport::gnm_eop_gpu_control();
    if(argc==2 && std::string(argv[1])=="--gnm-eop-completion-control")return ctrport::gnm_eop_completion_control();
    if(argc==2 && std::string(argv[1])=="--indexed-ordered-transaction-control")return ctrport::indexed_ordered_transaction_control();
    if(argc==2 && std::string(argv[1])=="--effective-null-requirement-control")return ctrport::effective_null_requirement_control();
    if(argc==2 && std::string(argv[1])=="--single-ms2-inactive-depth-draw-control")return ctrport::single_r32_draw_control(false,false,true);
    if(argc==2 && std::string(argv[1])=="--single-r32-depth-draw-control")return ctrport::single_r32_draw_control(true,true);
    if(argc==2 && std::string(argv[1])=="--single-ms2-stencil-draw-control")return ctrport::single_r32_draw_control(true);
    if(argc==2 && std::string(argv[1])=="--single-r32-draw-control")return ctrport::single_r32_draw_control();
    if(argc==2 && std::string(argv[1])=="--shader-provenance-trace-control")return ctrport::shader_provenance_trace_control();
    if(argc==2 && std::string(argv[1])=="--indexed-call-control")return ctrport::indexed_call_control();
    if(argc==2 && std::string(argv[1])=="--indexed-command-parameters-control")return ctrport::indexed_command_parameters_control();
    if(argc==2 && std::string(argv[1])=="--indexed-vertex-ranges-control")return ctrport::indexed_vertex_ranges_control();
    if(argc==2 && std::string(argv[1])=="--shader-capture-trace-control")return ctrport::shader_capture_trace_control();
    if(argc==2 && std::string(argv[1])=="--indexed-vertex-generation-control")return ctrport::indexed_vertex_generation_control();
    if(argc==2 && std::string(argv[1])=="--indexed-draw-transport-control")return ctrport::indexed_draw_transport_control();
    if(argc==2 && std::string(argv[1])=="--graphics-index-requirement-control")return ctrport::graphics_index_requirement_control();
    if(argc==2 && std::string(argv[1])=="--graphics-flatbuf-owner-control")return ctrport::graphics_flatbuf_owner_control();
    if(argc==2 && std::string(argv[1])=="--graphics-buffer-capture-control")return ctrport::graphics_buffer_capture_control();
    if(argc==2 && std::string(argv[1])=="--submit-flip-abi-control")return ctrport::gnm_flip_abi_control();
    if(argc==2 && std::string(argv[1])=="--draw-index-abi-control")return ctrport::index_abi_control();
    if(argc==2 && std::string(argv[1])=="--gnm-push-marker-control")return ctrport::gnm_push_marker_control();
    if(argc==2 && std::string(argv[1])=="--gnm-dispatch-guest-control")return ctrport::gnm_dispatch_guest_control();
    if(argc==2 && std::string(argv[1])=="--gnm-submit-ordered-control")return ctrport::gnm_submit_ordered_control();
    if(argc==2 && std::string(argv[1])=="--virtual-query-control")return ctrport::virtual_query_control();
    if(argc==2 && std::string(argv[1])=="--condition-timed-control")return ctrport::condition_timed_control();
    if(argc==2 && std::string(argv[1])=="--audio-s16-control")return ctrport::audio_s16_control();
    if(argc==2 && std::string(argv[1])=="--audio-pcm-control")return ctrport::audio_pcm_control();
    if(argc==2 && std::string(argv[1])=="--live-worker-priority-control")return ctrport::live_worker_priority_control();
    if(argc==2 && std::string(argv[1])=="--audio-volume-control")return ctrport::audio_volume_control();
    if(argc==2 && std::string(argv[1])=="--audio-port-control")return ctrport::audio_port_control();
    if(argc==2 && std::string(argv[1])=="--audio-output-control")return ctrport::audio_output_control();
    if(argc==2 && std::string(argv[1])=="--ajm-codec-control")return ctrport::ajm_codec_control();
    if(argc==2 && std::string(argv[1])=="--ajm-context-control")return ctrport::ajm_context_control();
    if(argc==2 && std::string(argv[1])=="--gnm-update-vs-guest-control")return ctrport::gnm_update_vs_guest_control();
    if(argc==2 && std::string(argv[1])=="--gnm-draw-auto-guest-control")return ctrport::gnm_draw_auto_guest_control();
    if(argc==2 && std::string(argv[1])=="--ime-backend-control")return ctrport::ime_backend_control();
    if(argc==2 && std::string(argv[1])=="--font-selection-control")return ctrport::font_selection_control();
    if(argc==2 && std::string(argv[1])=="--font-ft-missing-control")return static_cast<int>(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_font_library_callback_missing),0,0,0));
    if(argc==2 && std::string(argv[1])=="--font-guest-memory-control")return ctrport::font_guest_memory_control();
    if(argc==2 && std::string(argv[1])=="--font-memory-control")return ctrport::font_memory_control();
    if(argc==2 && std::string(argv[1])=="--compat-log-concurrency-control"){
        std::vector<std::thread> workers;
        for(unsigned worker=0;worker<16;++worker)workers.emplace_back([worker]{for(unsigned call=0;call<64;++call)ctr_json2_compat((worker+call)%ctr_declared_compat_count,0x100000ULL+worker*64+call);});
        for(auto& worker:workers)worker.join();return 0;
    }
    if(argc==2 && std::string(argv[1])=="--compat-stubs-control") {
        void* bridges[]={reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_0),reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_1),reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_2),reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_3),reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_4),reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_5),reinterpret_cast<void*>(&ctrport::ctr_sysv_json2_6)};
        for(auto bridge:bridges)if(ctrport::ctr_call_original_integer3(bridge,0,0,0)!=0)return 1;
        if(ctr_sysmodule_load(0xe7,0x12345678)!=0)return 1;
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_net_init_compat),0,0,0)!=0)return 1;
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_netctl_init_compat),0,0,0)!=0)return 1;
        if(ctr_sysmodule_load(0x19,0x12345678)!=0)return 1;
        if(ctr_sysmodule_load(0x9d,0x12345678)!=0)return 1;
        if(ctr_sysmodule_load(0xad,0x12345678)!=0)return 1;
        if(ctr_sysmodule_load(0xd3,0x12345678)!=0)return 1;
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_shareplay_init_compat),0,0,0)!=0)return 1;
        if(ctr_sysmodule_load(0x81,0x12345678)!=0)return 1;
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_live_stream_init_compat),0,0,0)!=0)return 1;
        if(ctr_sysmodule_load(0x9c,0x12345678)!=0)return 1;
        if(ctr_sysmodule_load(0xa8,0x12345678)!=0)return 1;
        unsigned char info[58];std::memset(info,0xa5,sizeof(info));
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_shareplay_info_compat),reinterpret_cast<std::uint64_t>(info+1),0,0)!=0)return 3;
        if(info[0]!=0xa5 || info[57]!=0xa5)return 4;
        for(unsigned i=1;i<57;++i)if(info[i]!=0)return 5;
        bool rejected=false;try{ctr_json2_compat(ctr_declared_compat_count,0);}catch(const std::exception&){rejected=true;}
        return rejected?0:2;
    }
    if(argc==2 && std::string(argv[1])=="--attr-set-detach-control") return ctrport::attr_set_detach_control();
    if(argc==2 && std::string(argv[1])=="--attr-set-affinity-control") return ctrport::attr_set_affinity_control();
    if(argc==2 && std::string(argv[1])=="--event-flag-set-control") return ctrport::event_flag_set_control();
    if(argc==2 && std::string(argv[1])=="--event-flag-wait-control") return ctrport::event_flag_wait_control();
    if(argc==2 && std::string(argv[1])=="--event-flag-create-control") return ctrport::event_flag_create_control();
    if(argc==2 && std::string(argv[1])=="--kernel-close-control") return ctrport::kernel_close_control();
    if(argc==2 && std::string(argv[1])=="--kernel-read-control")return ctrport::kernel_read_control();
    if(argc==2 && std::string(argv[1])=="--kernel-pread-control") return ctrport::kernel_pread_control();
    if(argc==2 && std::string(argv[1])=="--playgo-speed-control") return ctrport::playgo_speed_control();
    if(argc==3 && std::string(argv[1])=="--playgo-metadata-control") return ctrport::playgo_metadata_control(argv[2]);
    if(argc==3 && std::string(argv[1])=="--app-content-control") return ctrport::app_content_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--posix-write-control") return ctrport::posix_write_control();
    if(argc==2 && std::string(argv[1])=="--kernel-stat-control") return ctrport::kernel_stat_control();
    if(argc==2 && std::string(argv[1])=="--kernel-fstat-control") return ctrport::kernel_fstat_control();
    if(argc==2 && std::string(argv[1])=="--kernel-write-bridge-control"){
        const auto root=std::filesystem::path("runtime/user/tests/write_bridge");std::filesystem::create_directories(root);
        ctrport::kernel_files.configure_user(root);const int fd=ctrport::kernel_files.open("/data/bytes",0x602,0777);
        const char bytes[]="bridge";
        const auto result=ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_console_write),static_cast<std::uint64_t>(fd),reinterpret_cast<std::uint64_t>(bytes),6);
        if(result!=6 || ctrport::kernel_files.close(fd))return 1;
        std::ifstream in(root/"bytes",std::ios::binary);std::string actual((std::istreambuf_iterator<char>(in)),{});
        if(actual!="bridge")return 1;
        std::cout<<"{\"scope\":\"sysv_kernel_write_bridge\",\"bytes\":6,\"actual_file_verified\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--kernel-equeue-unsupported-control"){
        auto token=ctrport::kernel_display_events.queues.create();
        ctrport::DisplayKernelEvent event{};int count=0;const std::uint32_t zero=0;
        return ctrport::ctr_test_equeue_wait(token,&event,65,&count,&zero);
    }
    if(argc==2 && std::string(argv[1])=="--kernel-equeue-bridge-control"){
        std::uint64_t token=0;const char name[]="bridge";
        auto result=ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_equeue_create),reinterpret_cast<std::uint64_t>(&token),reinterpret_cast<std::uint64_t>(name),0);
        if(result || !token)return 1;
        ctrport::DisplayKernelEvent event{};int count=-1;const std::uint32_t zero=0;
        if(ctrport::ctr_test_equeue_wait(token,&event,1,&count,&zero)!=static_cast<int>(0x8002003cU) || count)return 1;
        auto api=ctrport::display_event_api(ctrport::kernel_display_events);
        if(api.add(api.context,token,6,0x1234) || api.trigger(api.context,token,6,6) ||
           ctrport::ctr_test_equeue_wait(token,&event,1,&count,&zero) || count!=1 || event.udata!=0x1234)return 1;
        result=ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_equeue_delete),token,0,0);
        if(result || ctrport::ctr_test_equeue_wait(token,&event,1,&count,&zero)!=static_cast<int>(0x80020009U))return 1;
        return 0;
    }
    if(argc==2 && std::string(argv[1])=="--color-r32float-guest-writeback-gpu-control")return ctrport::color_guest_writeback_gpu_control(true);
    if(argc==2 && std::string(argv[1])=="--display-color-guest-writeback-gpu-control")return ctrport::color_guest_writeback_gpu_control(false,true);
    if(argc==2 && std::string(argv[1])=="--color-guest-writeback-gpu-control")return ctrport::color_guest_writeback_gpu_control();
    if(argc==2 && std::string(argv[1])=="--gnm-image-completion-gpu-control")return ctrport::gnm_image_completion_gpu_control();
    if(argc==2 && std::string(argv[1])=="--gnm-graphics-equeue-gpu-control")return ctrport::gnm_graphics_equeue_gpu_control();
    if(argc==2 && std::string(argv[1])=="--gnm-graphics-equeue-abi-control"){
      uint64_t queue=0;if(ctrport::kernel_equeue_create(&queue,"graphics-abi"))return 1;
      if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_gnm_add_eq_event),queue,64,0xabc123))return 2;
      ctrport::kernel_display_events.queues.graphics_trigger(queue,64);auto rows=ctrport::kernel_display_events.queues.poll(queue,1);
      if(rows.size()!=1 || rows[0].udata!=0xabc123)return 3;
      if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_gnm_get_eq_event_type),reinterpret_cast<uint64_t>(&rows[0]),0,0)!=64)return 4;
      if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_gnm_delete_eq_event),queue,64,0))return 5;
      if(ctrport::kernel_equeue_delete(queue))return 6;
      std::cout<<"{\"scope\":\"gnm_graphics_equeue_SysV_control\",\"passed\":true,\"services\":3,\"actual_assembly_bridges\":true,\"synthetic_IRQ_source\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--gnm-graphics-equeue-control")return ctrport::gnm_graphics_equeue_control();
    if(argc==2 && std::string(argv[1])=="--kernel-equeue-boundary-control") return ctrport::kernel_equeue_boundary_control();
    if(argc==2 && std::string(argv[1])=="--display-event-wait-control") return ctrport::display_event_wait_control();
    if(argc==2 && std::string(argv[1])=="--display-event-api-control") return ctrport::display_event_api_control();
    if(argc==2 && std::string(argv[1])=="--display-event-queue-control") return ctrport::display_event_queue_control();
    if(argc==4 && std::string(argv[1])=="--retail-write-slot-control") {try{return ctrport::service_retail_slot_control(argv[2],argv[3]);}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 4;}}
    if(argc==2 && std::string(argv[1])=="--service-integer9-failure-control") return ctrport::service_integer9_failure_control(0);
    if(argc==2 && std::string(argv[1])=="--service-integer9-signature-control") return ctrport::service_integer9_failure_control(1);
    if(argc==2 && std::string(argv[1])=="--service-integer9-stale-control") return ctrport::service_integer9_failure_control(2);
    if(argc==3 && std::string(argv[1])=="--service-integer9-dll-control") return ctrport::service_integer9_dll_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--gnm-executor-host-dll-control")return ctrport::gnm_executor_host_control(true);
    if(argc==2 && std::string(argv[1])=="--gnm-executor-host-control")return ctrport::gnm_executor_host_control();
    if(argc==2 && std::string(argv[1])=="--video-buffer-batch-control")return ctrport::video_buffer_batch_control();
    if(argc==2 && std::string(argv[1])=="--video-layout-batch-control")return ctrport::video_layout_batch_control();
    if(argc==2 && std::string(argv[1])=="--video-sampler-batch-control")return ctrport::video_sampler_batch_control();
    if(argc==2 && std::string(argv[1])=="--video-image-batch-control")return ctrport::video_image_batch_control();
    if(argc==2 && std::string(argv[1])=="--graphics-buffer-writes-control")return ctrport::graphics_buffer_writes_control();
    if(argc==2 && std::string(argv[1])=="--service-integer9-control") return ctrport::service_integer9_control();
    if(argc==2 && std::string(argv[1])=="--service-slot-admission-control") return ctrport::service_slot_admission_control();
    if(argc==3 && std::string(argv[1])=="--guest-copy-image-control") return ctrport::guest_copy_image_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--owner-dispatch-api-control") return ctrport::owner_dispatch_api_control();
    if(argc==2 && std::string(argv[1])=="--owner-dispatch-control") return ctrport::owner_dispatch_control();
    if(argc==2 && std::string(argv[1])=="--guest-copy-api-control") return ctrport::guest_copy_api_control();
    if(argc==2 && std::string(argv[1])=="--guest-copy-control") return ctrport::guest_copy_control();
    if(argc==3 && std::string(argv[1])=="--service-write-dll-control") return ctrport::service_write_dll_control(argv[2],false);
    if(argc==3 && std::string(argv[1])=="--service-write-dll-failure-control") return ctrport::service_write_dll_control(argv[2],true);
    if(argc==2 && std::string(argv[1])=="--service-transport-control") return ctrport::service_transport_control();
    if(argc==2 && std::string(argv[1])=="--service-write-host-control") return ctrport::service_write_host_control();
    if(argc==2 && std::string(argv[1])=="--service-stale-thunk-control") return ctrport::service_stale_thunk_control();
    if(argc==2 && std::string(argv[1])=="--service-record-registry-control") return ctrport::service_record_registry_control();
    if(argc==3 && std::string(argv[1])=="--service-record-thunk-control") return ctrport::service_record_thunk_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--service-integer-version-control") return ctrport::service_integer_version_control();
    if(argc==2 && std::string(argv[1])=="--service-import-record-control") return ctrport::service_import_record_control();
    if(argc==2 && std::string(argv[1])=="--service-integer-shutdown-control") return ctrport::service_integer_shutdown_control();
    if(argc==3 && std::string(argv[1])=="--service-integer-dispatch-control") return ctrport::service_integer_dispatch_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--service-wait-control") return ctrport::service_wait_control();
    if(argc==3 && std::string(argv[1])=="--service-dll-control") return ctrport::service_dll_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--service-manifest-snapshot-control") return ctrport::service_manifest_snapshot_control();
    if(argc==2 && std::string(argv[1])=="--service-module-memory-control") return ctrport::service_module_memory_control();
    if(argc==2 && std::string(argv[1])=="--service-abi-control") return ctrport::service_abi_control();
    if(argc==2 && std::string(argv[1])=="--service-generation-control") return ctrport::service_generation_control();
    if(argc==2 && std::string(argv[1])=="--kernel-write-control") return ctrport::kernel_write_control();
    if(argc==2 && std::string(argv[1])=="--kernel-user-open-control") return ctrport::kernel_user_open_control();
    if(argc==2 && std::string(argv[1])=="--kernel-open-control") return ctrport::kernel_open_control();
    if(argc==2 && std::string(argv[1])=="--condition-wait-owned-control") return ctrport::condition_wait_owned_control();
    if(argc==2 && std::string(argv[1])=="--worker-join-bridge-control") return ctrport::startup_worker_join_bridge_control();
    if(argc==2 && std::string(argv[1])=="--thread-yield-control") return ctrport::thread_yield_control();
    if(argc==2 && std::string(argv[1])=="--thread-numeric-id-control") return ctrport::thread_numeric_id_control();
    if(argc==2 && std::string(argv[1])=="--signal-mask-query-control") return ctrport::signal_mask_query_control();
    if(argc==2 && std::string(argv[1])=="--pad-information-control")return ctrport::pad_information_control();
    if(argc==2 && std::string(argv[1])=="--pad-read-state-control")return ctrport::pad_read_state_control();
    if(argc==2 && std::string(argv[1])=="--pad-open-control")return ctrport::pad_open_control();
    if(argc==2 && std::string(argv[1])=="--user-name-control")return ctrport::user_name_control();
    if(argc==2 && std::string(argv[1])=="--flexible-memory-control")return ctrport::flexible_memory_control();
    if(argc==2 && std::string(argv[1])=="--np-web-context-control")return ctrport::np_web_context_control();
    if(argc==2 && std::string(argv[1])=="--http-context-control")return ctrport::http_context_control();
    if(argc==2 && std::string(argv[1])=="--ssl-context-control")return ctrport::ssl_context_control();
    if(argc==2 && std::string(argv[1])=="--net-pool-control")return ctrport::net_pool_control();
    if(argc==2 && std::string(argv[1])=="--cxa-atexit-control")return ctrport::cxa_atexit_control();
    if(argc==2 && std::string(argv[1])=="--cxa-guard-control")return ctrport::cxa_guard_control();
    if(argc==2 && std::string(argv[1])=="--user-initial-control")return ctrport::user_initial_control();
    if(argc==2 && std::string(argv[1])=="--user-callback-registration-control")return ctrport::user_callback_registration_control();
    if(argc==2 && std::string(argv[1])=="--thread-handle-prefix-control")return ctrport::thread_handle_prefix_control();
    if(argc==2 && std::string(argv[1])=="--system-status-control")return ctrport::system_status_control();
    if(argc==2 && std::string(argv[1])=="--thread-affinity-readback-control"){
        const auto self=ctrport::thread_self();std::uint64_t value=91;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_get_affinity),self,reinterpret_cast<std::uint64_t>(&value)) || value!=~std::uint64_t{0})return 1;
        DWORD_PTR process=0,system=0;if(!GetProcessAffinityMask(GetCurrentProcess(),&process,&system))return 1;
        const auto available=process&0x7fULL;if(!available)return 1;const auto selected=available&(~available+1);
        const auto previous=SetThreadAffinityMask(GetCurrentThread(),static_cast<DWORD_PTR>(selected));if(!previous)return 1;
        if(ctrport::thread_set_affinity(self,selected) || ctrport::thread_get_affinity(self,&value) || value!=selected)return 1;
        if(!SetThreadAffinityMask(GetCurrentThread(),previous))return 1;ctrport::record_thread_affinity(self,~std::uint64_t{0});
        value=91;if(ctrport::thread_get_affinity(~std::uint64_t{0},&value)!=static_cast<int>(0x80020003U) || value!=91 || ctrport::thread_get_affinity(self,nullptr)!=static_cast<int>(0x80020016U))return 1;
        std::cout<<"{\"scope\":\"thread_affinity_readback_control\",\"actual_SysV_query\":true,\"actual_host_set_then_guest_readback\":true,\"host_restored\":true,\"unknown_no_write\":true}\n";return 0;
    }
    if(argc==3 && std::string(argv[1])=="--app-list-control")return ctrport::app_list_control(argv[2]);
    if(argc==2 && std::string(argv[1])=="--np-delete-control")return ctrport::np_web_delete_control();
    if(argc==2 && std::string(argv[1])=="--np-send-control")return ctrport::np_web_send_control();
    if(argc==2 && std::string(argv[1])=="--np-request-control")return ctrport::np_web_request_control();
    if(argc==2 && std::string(argv[1])=="--netctl-state-control")return ctrport::net_connection_state_control();
    if(argc==2 && std::string(argv[1])=="--np-web-user-control")return ctrport::np_web_user_control();
    if(argc==2 && std::string(argv[1])=="--orbis-gettimeofday-control"){
        struct {std::uint64_t before=123;ctrport::KernelTimeval value{};std::uint64_t after=456;} frame;
        const auto lo=ctrport::unix_microseconds();const auto rc=ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_orbis_gettimeofday),reinterpret_cast<std::uint64_t>(&frame.value),~std::uint64_t{0});const auto hi=ctrport::unix_microseconds();
        const auto got=static_cast<std::uint64_t>(frame.value.seconds)*1000000+static_cast<std::uint64_t>(frame.value.microseconds);
        if(rc || got<lo || got>hi || frame.before!=123 || frame.after!=456 || frame.value.microseconds<0 || frame.value.microseconds>=1000000)return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_orbis_gettimeofday),0,~std::uint64_t{0}))return 1;
        std::cout<<"{\"scope\":\"orbis_time_control\",\"SysV1_ignores_second_argument\":true,\"actual_clock_bounds\":true,\"sentinels\":true,\"null_optional\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--posix-setschedparam-control")return ctrport::posix_set_sched_control();
    if(argc==2 && std::string(argv[1])=="--posix-thread-self-control") {
        using namespace ctrport;const auto expected=thread_self();
        for(int i=0;i<3;++i)if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_posix_thread_self),0,0,0)!=expected)return 1;
        auto worker=std::async(std::launch::async,[expected]{const auto own=ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_posix_thread_self),0,0,0);const bool ok=own==thread_self() && own!=expected;release_thread_identity();return ok;});
        if(!worker.get())return 1;std::cout<<"{\"scope\":\"posix_self_control\",\"same_owned_identity\":true,\"worker_distinct\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--attribute-get-stack-control") {
        using namespace ctrport;std::uint64_t cell=0,address=123,size=456;thread_attributes_init(&cell);
        auto query=[&]{return ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_attr_get_stack),reinterpret_cast<std::uint64_t>(&cell),reinterpret_cast<std::uint64_t>(&address),reinterpret_cast<std::uint64_t>(&size));};
        if(query() || address!=0 || size!=1048576)return 1;
        if(thread_attributes_current(thread_self(),&cell) || query())return 1;
        const auto local=reinterpret_cast<std::uint64_t>(&cell);if(local<address || local>=address+size)return 1;
        std::uint64_t bad=1;address=123;size=456;
        if(thread_attributes_get_stack(&bad,&address,&size)!=int(0x80020016U) || address!=123 || size!=456)return 1;
        if(thread_attributes_get_stack(nullptr,&address,&size)!=int(0x80020016U))return 1;
        if(thread_attributes_destroy(&cell))return 1;
        std::cout<<"{\"scope\":\"attribute_stack_control\",\"real_current_stack_contains_local\":true,\"sysv\":true,\"invalid_untouched\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--ime-dialog-status-control")return ctrport::ime_dialog_status_control();
    if(argc==2 && std::string(argv[1])=="--np-general-check-control")return ctrport::np_general_check_control();
    if(argc==2 && std::string(argv[1])=="--np-toolkit-dispatch-control")return ctrport::np_toolkit_dispatch_control();
    if(argc==2 && std::string(argv[1])=="--np-toolkit-callback-control")return ctrport::np_toolkit_callback_control();
    if(argc==2 && std::string(argv[1])=="--user-event-control")return ctrport::user_event_control();
    if(argc==2 && std::string(argv[1])=="--user-login-list-control") return ctrport::user_login_list_control();
    if(argc==2 && std::string(argv[1])=="--save-data-initialize-control") return ctrport::save_data_initialize_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-blocking-control") return ctrport::semaphore_blocking_control();
    _setmode(_fileno(stdout),_O_BINARY);_setmode(_fileno(stderr),_O_BINARY);
    if(argc==2 && std::string(argv[1])=="--console-output-control") return ctrport::console_output_control();
    if(argc==2 && std::string(argv[1])=="--thread-identity-lifecycle-control") return ctrport::thread_identity_lifecycle_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-contention-control") return ctrport::semaphore_contention_control();
    if(argc==2 && std::string(argv[1])=="--thread-create-output-control") return ctrport::thread_create_output_control();
    if(argc==2 && std::string(argv[1])=="--thread-create-domain-control") return ctrport::thread_create_domain_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-join-detach-control") return ctrport::guest_worker_join_detach_control();
    if(argc==2 && std::string(argv[1])=="--guest-thread-exit-control") return ctrport::guest_thread_exit_control();
    if(argc==2 && std::string(argv[1])=="--startup-worker-reaper-control") return ctrport::startup_worker_reaper_control();
    if(argc==2 && std::string(argv[1])=="--startup-worker-detach-bridge-control") return ctrport::startup_worker_detach_bridge_control();
    if(argc==2 && std::string(argv[1])=="--startup-worker-retained-lookup-control") return ctrport::startup_worker_retained_lookup_control();
    if(argc==2 && std::string(argv[1])=="--net-toolkit-dispatch-control")return ctrport::net_toolkit_dispatch_control();
    if(argc==2 && std::string(argv[1])=="--net-toolkit-register-control") return ctrport::net_toolkit_register_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-host-affinity-control") return ctrport::guest_worker_host_affinity_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-detach-exited-control") return ctrport::guest_worker_detach_exited_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-self-join-control") return ctrport::guest_worker_self_join_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-fp-control") return ctrport::guest_worker_fp_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-missing-control") return ctrport::guest_worker_missing_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-rollback-control") return ctrport::guest_worker_rollback_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-retention-control") return ctrport::guest_worker_retention_control();
    if(argc==2 && std::string(argv[1])=="--guest-stack-exception-control") return ctrport::guest_stack_exception_control();
    if(argc==2 && std::string(argv[1])=="--guest-stack-call-control") return ctrport::guest_stack_call_control();
    if(argc==2 && std::string(argv[1])=="--guest-stack-control") return ctrport::guest_stack_control();
    if(argc==2 && std::string(argv[1])=="--condition-cancel-protocol-control") return ctrport::condition_cancel_protocol_control();
    if(argc==2 && std::string(argv[1])=="--cancellation-lifecycle-control") return ctrport::cancellation_lifecycle_control();
    if(argc==2 && std::string(argv[1])=="--deferred-cancellation-control") return ctrport::deferred_cancellation_control();
    if(argc==2 && std::string(argv[1])=="--condition-mutex-protocol-control") return ctrport::condition_mutex_protocol_control();
    if(argc==2 && std::string(argv[1])=="--orbis-condition-broadcast-control") {
        if(ctrport::kernel_condition_control())return 1;
        std::uint64_t cell=0;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_orbis_condition_broadcast),reinterpret_cast<std::uint64_t>(&cell),0))return 1;
        if(ctrport::lookup_condition(&cell)->generation!=1)return 1;
        std::cout<<"{\"scope\":\"orbis_condition_broadcast_bridge\",\"generation_increment\":true,\"real_waiters_verified\":true,\"guest_wait_bound\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--condition-wait-bridge-missing-control") return ctrport::condition_wait_bridge_missing_control();
    if(argc==2 && std::string(argv[1])=="--condition-relative-foreign-owner-control") return ctrport::condition_relative_foreign_owner_control();
    if(argc==2 && std::string(argv[1])=="--condition-relative-bridge-missing-control") return ctrport::condition_relative_bridge_missing_control();
    if(argc==2 && std::string(argv[1])=="--condition-relative-duration-control") return ctrport::condition_relative_duration_control();
    if(argc==2 && std::string(argv[1])=="--condition-failure-reacquire-control") return ctrport::condition_failure_reacquire_control();
    if(argc==2 && std::string(argv[1])=="--condition-recursive-protocol-control") return ctrport::condition_recursive_protocol_control();
    if(argc==2 && std::string(argv[1])=="--condition-timeout-reacquire-control") return ctrport::condition_timeout_reacquire_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-atomic-control") return ctrport::semaphore_atomic_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-microsecond-control") return ctrport::semaphore_microsecond_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-timeout-race-control") return ctrport::semaphore_timeout_race_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-cancel-control") return ctrport::semaphore_cancel_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-priority-control") return ctrport::semaphore_priority_control();
    if(argc==2 && std::string(argv[1])=="--semaphore-queue-control") return ctrport::semaphore_queue_control();
    if(argc==2 && std::string(argv[1])=="--guest-errno-control") {
        auto call=[] {return reinterpret_cast<int*>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_guest_errno),0,0));};
        auto* host=call();if(!host || *host || call()!=host)return 1;*host=123;
        if(!ctrport::run_worker_control([&] {auto* own=call();if(!own || own==host || *own || call()!=own)return false;*own=456;return *call()==456;}))return 1;
        if(*host!=123)return 1;
        std::cout<<"{\"scope\":\"guest_errno_sysv_control\",\"stable_pointer\":true,\"zero_initialization\":true,\"two_thread_isolation\":true,\"error_conversion_implemented\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--publication-failure-control") return ctrport::publication_failure_control();
    if(argc==2 && std::string(argv[1])=="--thread-creation-rollback-control") return ctrport::thread_creation_rollback_control();
    if(argc==2 && std::string(argv[1])=="--thread-publication-control") return ctrport::thread_publication_control();
    if(argc==2 && std::string(argv[1])=="--guest-worker-competing-join-control") return ctrport::guest_worker_competing_join_control();
    if(argc==2 && std::string(argv[1])=="--thread-completion-control") return ctrport::thread_completion_control();
    if(argc==2 && std::string(argv[1])=="--tls-release-control") return ctrport::tls_release_control();
    if(argc==2 && std::string(argv[1])=="--worker-failure-control") {
        ctrport::TlsStorage storage({{{7},16,8}});bool propagated=false;
        try {ctrport::run_worker_control([&]() -> bool {ctrport::TlsThreadLifetime lifetime(storage);ctrport::TlsContextScope context(storage);storage.address(1,0);throw std::runtime_error("worker control failure");});}
        catch(const std::runtime_error& error) {propagated=std::string(error.what())=="worker control failure";}
        if(!propagated || ctrport::current_tls_storage) return 1;
        if(!ctrport::run_worker_control([&] {if(ctrport::current_tls_storage)return false;ctrport::TlsContextScope context(storage);return *static_cast<unsigned char*>(storage.address(1,0))==7;})) return 1;
        std::cout<<"{\"scope\":\"worker_failure_lifecycle\",\"exception_propagated_after_join\":true,\"next_worker_clean_tls\":true,\"guest_create_bound\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--tls-context-scope-control") return ctrport::tls_context_scope_control();
    if(argc>=3 && (std::string(argv[1])=="--tls-original-accessor-control" || std::string(argv[1])=="--tls-original-stack-control" || std::string(argv[1])=="--tls-retained-worker-control")) {
        const bool owned_stack=std::string(argv[1])=="--tls-original-stack-control";
        const bool retained_worker=std::string(argv[1])=="--tls-retained-worker-control";
        try {
            std::vector<ctrport::MappedImage> retained;std::vector<ctrport::TlsTemplate> templates;
            ctrport::ImageMetadata meta;unsigned char* image=nullptr;std::uint64_t libc_id=0;std::size_t count=0;
            for(int i=2;i<argc;++i) {
                ctrport::ImageMetadata current;auto mapped=ctrport::load_retail_image(argv[i],true,&current);
                if(current.has_tls) {templates.push_back({current.tls_initialized,static_cast<std::size_t>(current.tls_size),static_cast<std::size_t>(current.tls_alignment)});count+=ctrport::apply_tls_module_id(mapped,current,templates.size());}
                if(current.sha256=="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816") {if(image)throw std::runtime_error("Duplicate libc image");image=mapped.get();meta=current;libc_id=templates.size();}
                retained.push_back(std::move(mapped));
            }
            if(!image || !libc_id) throw std::runtime_error("TLS accessor image missing");
            auto retained_tls=std::make_shared<ctrport::TlsStorage>(std::move(templates));auto& storage=*retained_tls;
            unsigned bound=0;for(const auto& item:meta.imports) if(item.nid=="vNe1w4diLCs" && item.library=="libkernel") {const auto target=reinterpret_cast<std::uint64_t>(&ctr_sysv_tls_address);std::memcpy(image+item.slot,&target,8);++bound;}
            if(bound!=1) throw std::runtime_error("TLS accessor import identity differs");
            bool declared=false;for(const auto& item:meta.exports) if(item.address==0x2b970 && item.nid=="3rJJb81CDM4") declared=true;
            if(!declared) throw std::runtime_error("TLS accessor export differs");
            DWORD old=0;for(const auto& span:meta.executable_spans) if(!VirtualProtect(image+span.first,static_cast<SIZE_T>(span.second-span.first),PAGE_EXECUTE_READ,&old)) throw std::runtime_error("TLS accessor protection failed");
            if(!FlushInstructionCache(GetCurrentProcess(),image+0x2b970,25)) throw std::runtime_error("TLS accessor flush failed");
            const auto call=[&] {return reinterpret_cast<unsigned char*>(ctrport::ctr_call_original_integer2(image+0x2b970,0,0));};
            ctrport::TlsContextScope context(storage);auto* first=call();
            if(first!=static_cast<unsigned char*>(storage.address(libc_id,0))+0x180 || *first!=0 || call()!=first) throw std::runtime_error("Original TLS accessor relation failed");
            *first=91;
            if(retained_worker) {
                auto owner=std::make_shared<std::vector<ctrport::MappedImage>>(std::move(retained));
                auto* target=ctrport::validated_worker_target(image,meta,meta.sha256,0x2b970,25);
                unsigned rejected=0;
                for(int mode:{0,1,2})try{ctrport::validated_worker_target(image,meta,mode==0?"wrong":meta.sha256,mode==1?std::numeric_limits<std::uint64_t>::max():0x2b970,mode==2?0:25);}catch(const std::runtime_error&){++rejected;}
                if(rejected!=3)throw std::runtime_error("Worker target negatives failed");
                ctrport::GuestWorkerControl worker(retained_tls,owner,target,nullptr);owner.reset();
                worker.publish_and_start();const auto result=worker.join();
                if(!result || result==reinterpret_cast<std::uint64_t>(first) || *first!=91)throw std::runtime_error("Retained original TLS worker result differs");
                std::cout<<"{\"scope\":\"whole_original_tls_retained_worker\",\"original_returns\":3,\"target_negatives\":3,\"mapped_owner_retained\":true,\"parent_tls_unchanged\":true,\"guest_create_bound\":false}\n";return 0;
            }
            ctrport::ThreadCompletion completion;
            std::uint64_t published_identity=0;
            const auto joiner=ctrport::thread_self();bool claimed=false;
            const bool worker=ctrport::run_published_worker_control([&](std::uint64_t identity) {
                published_identity=identity;completion.publish();
                claimed=completion.claim_join(joiner,identity==joiner);
                if(!claimed)throw std::runtime_error("Original worker join claim rejected");
            },[&] {
                if(published_identity!=ctrport::thread_self())throw std::runtime_error("Original worker identity publication differs");
                bool okay=false;
                {ctrport::TlsThreadLifetime lifetime(storage);ctrport::TlsContextScope context(storage);
                 ctrport::GuestStack stack(32768,4096);
                 const auto worker_call=[&] {return owned_stack?reinterpret_cast<unsigned char*>(ctrport::ctr_guest_stack_call(image+0x2b970,nullptr,stack.top(),stack.bottom())):call();};
                 auto* own=worker_call();okay=own!=first && own==static_cast<unsigned char*>(storage.address(libc_id,0))+0x180 && *own==0 && worker_call()==own;*own=72;
                 if(owned_stack)std::cout<<"{\"scope\":\"original_tls_owned_worker_stack\",\"original_returns\":2,\"worker_tls_isolated\":true,\"guest_create_bound\":false}\n";
                }
                okay=okay && !ctrport::current_tls_storage && storage.release_current_thread()==0;
                completion.complete(okay?72:0);return okay;
            });
            std::uint64_t retained_result=0;
            if(!claimed || completion.is_collected() || !completion.consume_join(joiner,retained_result) || retained_result!=72 || !completion.is_collected())throw std::runtime_error("Original worker retained completion differs");
            std::cout<<"{\"scope\":\"original_tls_completion_integration\",\"identity_published_before_body\":true,\"tls_cleaned_before_completion\":true,\"completion_retained_after_worker_exit\":true,\"result_collected\":true,\"guest_create_bound\":false}\n";
            if(!worker || *first!=91) throw std::runtime_error("Original TLS accessor isolation failed");
            std::cout<<"{\"scope\":\"whole_original_retail_tls_accessor\",\"original_returns\":4,\"module_relocations\":"<<count<<",\"two_thread_isolation\":true,\"startup_bound\":false}\n";return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==3 && std::string(argv[1])=="--tls-sysv-negative") {
        ctrport::TlsStorage storage({{{1},32,16}});ctrport::current_tls_storage=&storage;
        CtrTlsIndex index{1,0};const CtrTlsIndex* pointer=&index;
        const std::string mode=argv[2];
        if(mode=="module") index.module=2;
        else if(mode=="offset") index.offset=32;
        else if(mode=="null") pointer=nullptr;
        else if(mode=="unmapped") pointer=reinterpret_cast<const CtrTlsIndex*>(1);
        else if(mode=="context") ctrport::current_tls_storage=nullptr;
        else if(mode=="extent") {auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!page)return 1;pointer=reinterpret_cast<const CtrTlsIndex*>(page+4090);}
        else return 2;
        ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_tls_address),reinterpret_cast<std::uint64_t>(pointer),0);
        return 1;
    }
    if(argc==2 && std::string(argv[1])=="--tls-sysv-control") {
        ctrport::TlsStorage storage({{{11,22,33,44},32,16}});
        const auto call=[&](std::uint64_t offset) {CtrTlsIndex index{1,offset};return reinterpret_cast<unsigned char*>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_tls_address),reinterpret_cast<std::uint64_t>(&index),0));};
        ctrport::TlsContextScope context(storage);auto* first=call(0);
        if(first[0]!=11 || *call(3)!=44 || *call(31)!=0 || call(0)!=first) return 1;
        first[0]=99;
        auto worker=std::async(std::launch::async,[&]{ctrport::TlsContextScope context(storage);auto* own=call(0);const bool okay=own!=first && own[0]==11 && call(3)==own+3;return okay;});
        if(!worker.get() || first[0]!=99) return 1;
        std::cout<<"{\"scope\":\"synthetic_sysv_tls_consumer\",\"offset_readback\":true,\"two_thread_isolation\":true,\"startup_bound\":false}\n";return 0;
    }

    if(argc==2 && std::string(argv[1])=="--tls-relocation-negative-control") {
        try {
            ctrport::MappedImage image(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
            if(!image) throw std::runtime_error("control allocation failed");
            std::memset(image.get(),0x5a,4096);
            ctrport::ImageMetadata meta;meta.has_tls=true;meta.spans.push_back({0,4096});meta.tls_module_slots={16};
            unsigned rejected=0;
            const auto check=[&](const ctrport::ImageMetadata& candidate,std::uint64_t id) {
                bool failed=false;try {ctrport::apply_tls_module_id(image,candidate,id);} catch(const std::runtime_error&) {failed=true;}
                if(!failed) throw std::runtime_error("invalid TLS relocation accepted");
                for(std::size_t i=0;i<4096;++i) if(image.get()[i]!=0x5a) throw std::runtime_error("rejected TLS relocation mutated image");
                ++rejected;
            };
            check(meta,0);
            auto invalid=meta;invalid.has_tls=false;check(invalid,1);
            invalid=meta;invalid.tls_module_slots.push_back(4093);check(invalid,1);
            invalid=meta;invalid.tls_module_slots.push_back(UINT64_MAX);check(invalid,1);
            ctrport::MappedImage empty;bool failed=false;try {ctrport::apply_tls_module_id(empty,meta,1);} catch(const std::runtime_error&) {failed=true;}
            if(!failed) throw std::runtime_error("null TLS image accepted");
            std::cout<<"{\"scope\":\"synthetic_tls_relocation_negatives\",\"no_mutation_cases\":"<<rejected<<",\"null_image_rejected\":true,\"guest_bound\":false}\n";
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }

    if(argc>=3 && std::string(argv[1])=="--tls-retail-template-control") {
        try {
            std::vector<ctrport::TlsTemplate> templates;std::size_t relocated=0;
            for(int i=2;i<argc;++i) {ctrport::ImageMetadata meta;auto image=ctrport::load_retail_image(argv[i],true,&meta);if(meta.has_tls) {templates.push_back({meta.tls_initialized,static_cast<std::size_t>(meta.tls_size),static_cast<std::size_t>(meta.tls_alignment)});relocated+=ctrport::apply_tls_module_id(image,meta,templates.size());}}
            ctrport::TlsStorage storage(templates);std::size_t checked=0;
            for(std::size_t i=0;i<templates.size();++i) {
                const auto& value=templates[i];if(!value.size) continue;
                const auto* block=static_cast<const unsigned char*>(storage.address(i+1,0));
                if(reinterpret_cast<std::uintptr_t>(block)%value.alignment) return 1;
                for(std::size_t j=0;j<value.size;++j) if(block[j]!=(j<value.initialized.size()?value.initialized[j]:0)) return 1;
                ++checked;
            }
            std::cout<<"{\"scope\":\"retail_tls_templates\",\"module_ids\":"<<templates.size()<<",\"nonempty_checked\":"<<checked<<",\"module_relocations_readback\":"<<relocated<<",\"guest_bound\":false}\n";return 0;
        }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 4;}
    }

    if(argc==2 && std::string(argv[1])=="--video-tiled-guest-control") {return ctrport::video_tiled_guest_control();}
    if(argc==2 && std::string(argv[1])=="--video-mode-any-control") {return ctrport::video_mode_any_control();}
    if(argc==2 && std::string(argv[1])=="--usleep-guest-control") {return ctrport::kernel_usleep_control();}
    if(argc==2 && std::string(argv[1])=="--batch-map-guest-control") {return ctrport::batch_map_guest_control();}
    if(argc==2 && std::string(argv[1])=="--batch-map-memory-control") {return ctrport::batch_map_memory_control();}
    if(argc==2 && std::string(argv[1])=="--pad-init-control") {return ctrport::pad_init_control();}
    if(argc==2 && std::string(argv[1])=="--gnm-set-ps-guest-control") {return ctrport::gnm_set_ps_guest_control();}
    if(argc==2 && std::string(argv[1])=="--gnm-set-vs-guest-control") {return ctrport::gnm_set_vs_guest_control();}
    if(argc==2 && std::string(argv[1])=="--gnm-submit-guest-control") {return ctrport::gnm_submit_guest_control();}
    if(argc==2 && std::string(argv[1])=="--gnm-submission-snapshot-control") {return ctrport::gnm_submission_snapshot_control();}
    if(argc==2 && std::string(argv[1])=="--video-output-any-guest-control") {return ctrport::video_output_any_guest_control();}
    if(argc==2 && std::string(argv[1])=="--video-attribute-bridge-control") {return ctrport::video_attribute_bridge_control();}
    if(argc==2 && std::string(argv[1])=="--video-event-session-control") {return ctrport::video_event_session_control();}
    if(argc==2 && std::string(argv[1])=="--owned-guest-copy-control") {return ctrport::owned_guest_copy_control();}
    if(argc==2 && std::string(argv[1])=="--video-session-owner-control") {return ctrport::video_session_owner_control();}
    if(argc==2 && std::string(argv[1])=="--gnm-init-transport-control") {return ctrport::gnm_init_transport_control();}
    if(argc==2 && std::string(argv[1])=="--exception-dispatch-control") {return ctrport::exception_dispatch_control();}
    if(argc==2 && std::string(argv[1])=="--semaphore-trace-control") {return ctrport::semaphore_trace_control();}
    if(argc==2 && std::string(argv[1])=="--insert-wait-flip-control") {return ctrport::insert_wait_flip_control();}
    if(argc==2 && std::string(argv[1])=="--video-clock-binding-control") {return ctrport::video_clock_binding_control();}
    if(argc==2 && std::string(argv[1])=="--gnm-frame-end-control") {return ctrport::gnm_frame_end_control();}
    if(argc==2 && std::string(argv[1])=="--flip-surface-lock-control") {return ctrport::flip_surface_lock_control();}
    if(argc==2 && std::string(argv[1])=="--video-label-memory-control") {return ctrport::video_label_memory_control();}
    if(argc==2 && std::string(argv[1])=="--direct-mapping-publication-control") {return ctrport::direct_mapping_publication_control();}
    if(argc==2 && std::string(argv[1])=="--direct-mapping-lease-control") {return ctrport::direct_mapping_lease_control();}
    if(argc==2 && std::string(argv[1])=="--direct-copy-control") {return ctrport::direct_copy_control();}
    if(argc==2 && std::string(argv[1])=="--posix-thread-create-control") {
        using namespace ctrport;
        auto images=std::make_shared<std::vector<MappedImage>>();
        auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!memory)return 1;
        images->emplace_back(memory);const unsigned char code[]={0x48,0x89,0xf8,0xc3};std::memcpy(memory,code,sizeof(code));
        DWORD old=0;if(!VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old))return 1;
        ImageMetadata metadata;metadata.sha256="synthetic-owned-worker";metadata.executable_spans={{0,4}};
        startup_worker_metadata={metadata};startup_worker_images=images;startup_worker_tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{},24,8}});
        unsigned rejected=0;
        for(auto offset:{std::uint64_t(4),std::uint64_t(4096)})try{resolve_original_call(*images,startup_worker_metadata,metadata.sha256,metadata.sha256,offset);}catch(const std::runtime_error&){++rejected;}
        try{resolve_original_call(*images,startup_worker_metadata,"wrong-sha",metadata.sha256,0);}catch(const std::runtime_error&){++rejected;}
        if(rejected!=3)return 1;
        std::uint64_t handle=0;
        if(ctr_posix_thread_create(&handle,nullptr,memory,reinterpret_cast<void*>(123),"control",0x1234) || !handle)return 1;
        auto worker=find_startup_worker(handle);if(!worker || worker->join()!=123)return 1;
        reap_startup_workers();worker.reset();startup_worker_images.reset();startup_worker_tls.reset();startup_worker_metadata.clear();
        std::cout<<"{\"scope\":\"synthetic_posix_named_thread_create\",\"real_worker_return\":123,\"inherited_priority\":700,\"joined_and_reaped\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--posix-thread-attr-init-control") {
        std::uint64_t cell=0;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_init),reinterpret_cast<std::uint64_t>(&cell),0) || !cell)return 1;
        {std::lock_guard<std::mutex> guard(ctrport::thread_attributes_lock);const auto found=ctrport::thread_attributes.find(cell);if(found==ctrport::thread_attributes.end() || found->second->priority!=700)return 1;}
        for(int state:{0,1}) {
            if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_detach),reinterpret_cast<std::uint64_t>(&cell),state))return 1;
            std::lock_guard<std::mutex> guard(ctrport::thread_attributes_lock);if(ctrport::thread_attributes.at(cell)->detached!=(state!=0))return 1;
        }
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_detach),reinterpret_cast<std::uint64_t>(&cell),2)!=22)return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_stack_size),reinterpret_cast<std::uint64_t>(&cell),400000))return 1;
        {std::lock_guard<std::mutex> guard(ctrport::thread_attributes_lock);if(ctrport::thread_attributes.at(cell)->stack_size!=400000)return 1;}
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_stack_size),reinterpret_cast<std::uint64_t>(&cell),1)!=22)return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_destroy),reinterpret_cast<std::uint64_t>(&cell),0) || cell)return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_posix_thread_attr_destroy),reinterpret_cast<std::uint64_t>(&cell),0)!=22)return 1;
        std::cout<<"{\"scope\":\"synthetic_posix_thread_attr_init\",\"owned_handle\":true,\"default_priority\":700,\"destroyed\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-getprio-control") {return ctrport::thread_get_priority_control();}
    if(argc==2 && std::string(argv[1])=="--process-time-control") {
        const auto before=ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_process_time),0,0);
        Sleep(20);
        const auto after=ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_process_time),0,0);
        if(after<=before || after-before<15000 || after-before>2000000)return 1;
        std::cout<<"{\"scope\":\"synthetic_process_time_sysv\",\"elapsed_us\":"<<(after-before)<<",\"passed\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--static-tls-control") {return ctrport::static_tls_control();}
    if(argc==2 && std::string(argv[1])=="--tls-storage-control") {return ctrport::tls_storage_control();}

    if(argc==2 && std::string(argv[1])=="--save-dialog-bridge-control") {
        bool rejected=false;try {ctrport::load_save_dialog_module();}catch(const std::exception&) {rejected=true;}if(!rejected || ctrport::save_module_active) return 1;
        ctrport::save_module_exports_ready=true;if(ctrport::load_save_dialog_module() || !ctrport::save_module_active) return 1;
        ctrport::common_dialog.initialize();
        auto call=[](void* target){return static_cast<int>(ctrport::ctr_call_original_integer2(target,0,0));};
        if(call(reinterpret_cast<void*>(&ctrport::ctr_sysv_save_dialog_initialize)) || call(reinterpret_cast<void*>(&ctrport::ctr_sysv_save_dialog_status))!=1 || call(reinterpret_cast<void*>(&ctrport::ctr_sysv_save_dialog_terminate)) || call(reinterpret_cast<void*>(&ctrport::ctr_sysv_save_dialog_status))!=0) return 1;
        std::cout<<"{\"scope\":\"save_dialog_bridge\",\"module_requires_verified_exports\":true,\"sysv_lifecycle\":true,\"open_supported\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--save-dialog-lifecycle-control") {
        ctrport::CommonDialogLifecycle common;ctrport::SaveDataDialogLifecycle first(common),second(common);
        if(first.initialize()!=static_cast<int>(0x80B80001U) || first.update_status()!=0 || common.initialize()) return 1;
        if(first.initialize() || first.update_status()!=1 || !common.in_use() || first.initialize()!=static_cast<int>(0x80B80004U) || second.initialize()!=static_cast<int>(0x80B80008U)) return 1;
        if(first.terminate() || common.in_use() || first.update_status()!=0 || first.terminate()!=static_cast<int>(0x80B80003U) || second.initialize() || second.terminate()) return 1;
        std::cout<<"{\"scope\":\"save_dialog_lifecycle\",\"common_required\":true,\"exclusive_ownership\":true,\"repeat_errors\":true,\"terminate_releases\":true,\"ui_supported\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--common-dialog-init-control") {
        ctrport::CommonDialogLifecycle state;std::atomic<int> successes{0},repeats{0};
        auto run=[&]{const auto result=state.initialize();if(result==0) ++successes;else if(result==static_cast<int>(0x80B80002U)) ++repeats;};
        std::thread a(run),b(run);a.join();b.join();
        if(successes!=1 || repeats!=1 || !state.ready()) return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_common_dialog_initialize),0,0) || !ctrport::common_dialog.ready()) return 1;
        std::cout<<"{\"scope\":\"common_dialog_initialize\",\"one_concurrent_initializer\":true,\"repeat_error\":true,\"sysv_state_transition\":true,\"dialog_render_supported\":false}\n";return 0;
    }
    if(argc==2 && (std::string(argv[1])=="--posix-mutex-try-control" || std::string(argv[1])=="--orbis-mutex-try-control")) {
        const bool orbis=std::string(argv[1])=="--orbis-mutex-try-control";const int busy=orbis?static_cast<int>(0x80020010U):16;
        std::uint64_t cell=0;const auto call=[&]{return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(orbis?&ctrport::ctr_sysv_orbis_mutex_trylock:&ctrport::ctr_sysv_posix_mutex_trylock),reinterpret_cast<std::uint64_t>(&cell),0));};
        const auto unlock=[&]{return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_mutex_unlock),reinterpret_cast<std::uint64_t>(&cell),0));};
        if(call() || call()!=busy)return 1;
        auto other=std::async(std::launch::async,call);if(other.get()!=busy || unlock() || call() || unlock())return 1;
        std::uint64_t attr=0;ctrport::kernel_mutex_attributes_init(&attr);ctrport::kernel_mutex_attributes_settype(&attr,2);ctrport::kernel_mutex_init(&cell,&attr,nullptr);
        if(call() || call() || unlock() || unlock())return 1;
        std::cout<<"{\"scope\":\"posix_mutex_trylock\",\"self_and_foreign_busy16\":true,\"recursive_depth\":true,\"real_acquire_release\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--posix-mutex-init-control") {
        std::uint64_t attr=0,cell=0;ctrport::kernel_mutex_attributes_init(&attr);ctrport::kernel_mutex_attributes_settype(&attr,2);
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_posix_mutex_init),reinterpret_cast<std::uint64_t>(&cell),reinterpret_cast<std::uint64_t>(&attr)) || static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_mutex_lock),reinterpret_cast<std::uint64_t>(&cell),0)) || static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_mutex_lock),reinterpret_cast<std::uint64_t>(&cell),0)) || ctrport::kernel_mutex_unlock(&cell) || ctrport::kernel_mutex_unlock(&cell))return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_posix_mutexattr_destroy),reinterpret_cast<std::uint64_t>(&attr),0) || attr || ctr_posix_mutexattr_destroy(&attr,0x1234)!=22)return 1;
        std::cout<<"{\"scope\":\"posix_mutex_init_bridge\",\"recursive_owned_lock_unlock\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--posix-mutexattr-control") {
        std::uint64_t cell=0;if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_posix_mutexattr_init),reinterpret_cast<std::uint64_t>(&cell),0) || !cell)return 1;
        {std::lock_guard<std::mutex> guard(ctrport::mutex_attributes_registry_lock);auto object=ctrport::mutex_attributes_registry.at(cell);if(object->type!=1 || object->protocol!=0)return 1;}
        const auto set=[&](unsigned type){return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_posix_mutexattr_settype),reinterpret_cast<std::uint64_t>(&cell),type));};
        if(set(2) || set(0)!=22 || set(5)!=22)return 1;
        {std::lock_guard<std::mutex> guard(ctrport::mutex_attributes_registry_lock);if(ctrport::mutex_attributes_registry.at(cell)->type!=2)return 1;}
        if(ctr_posix_mutexattr_settype(nullptr,2,0x1234)!=22 || ctrport::kernel_mutex_attributes_destroy(&cell) || cell)return 1;
        std::cout<<"{\"scope\":\"posix_mutexattr_owned_bridge\",\"owned_default\":true,\"settype_destroy\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--local-time-control") {
        for(std::int64_t utc:{1767225600LL,1782864000LL}) {
            std::int64_t local=-1;ctrport::LocalTimeInfo info{};std::uint64_t dst=~0ULL;
            if(ctrport::convert_local(utc,&local,&info,&dst))return 1;
            std::tm expected{};const __time64_t source=utc;if(_localtime64_s(&expected,&source))return 1;
            const __time64_t shifted=local;std::tm actual{};if(_gmtime64_s(&actual,&shifted) || actual.tm_year!=expected.tm_year || actual.tm_mon!=expected.tm_mon || actual.tm_mday!=expected.tm_mday || actual.tm_hour!=expected.tm_hour || actual.tm_min!=expected.tm_min || info.utc!=utc || static_cast<std::int32_t>(info.offset)!=local-utc || dst!=info.dst)return 1;
        }
        std::int64_t local=91;if(ctrport::convert_local(-1,&local,nullptr,nullptr)!=static_cast<int>(0x80020016U) || local!=91 || ctrport::convert_local(0,nullptr,nullptr,nullptr)!=static_cast<int>(0x8002000eU))return 1;
        std::cout<<"{\"scope\":\"host_timezone_conversion\",\"winter_summer_calendar\":true,\"seconds_offset\":true,\"invalid_preserves\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--kernel-clock-control"){
 struct Frame{uint64_t before;ctrport::PosixTimespec time;uint64_t after;};Frame a{123,{-1,-1},456},b=a;
 for(unsigned id:{0u,4u,13u}){
 if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_kernel_clock),id,reinterpret_cast<uint64_t>(&a.time)))return 1;
 if(a.time.nanoseconds<0 || a.time.nanoseconds>=1000000000 || a.before!=123 || a.after!=456)return 1;}
 if(static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_kernel_clock),4,0))!=static_cast<int>(0x8002000eU))return 1;
 bool rejected=false;try{ctrport::posix_clock(99,&b.time);}catch(...){rejected=true;}
 if(!rejected || b.time.seconds!=-1 || b.time.nanoseconds!=-1)return 1;
 std::cout<<"{\"scope\":\"kernel_clock_control\",\"SysV2\":true,\"real_clocks\":3,\"null_kernel_EFAULT\":true,\"unknown_preserved\":true,\"passed\":true}\n";return 0;
 }
    if(argc==2 && std::string(argv[1])=="--posix-clock-control") {
        struct Frame{std::uint64_t before;ctrport::PosixTimespec time;std::uint64_t after;};Frame frame{0x1234,{-1,-1},0x5678};
        const auto low=ctrport::unix_microseconds();*ctr_guest_errno()=91;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_posix_clock),13,reinterpret_cast<std::uint64_t>(&frame.time)))return 1;
        const auto high=ctrport::unix_microseconds(),value=static_cast<std::uint64_t>(frame.time.seconds)*1000000+frame.time.nanoseconds/1000;
        if(value+20000<low || value>high+20000 || frame.before!=0x1234 || frame.after!=0x5678 || *ctr_guest_errno()!=91)return 1;
        ctrport::PosixTimespec first{},second{};if(ctrport::posix_clock(4,&first) || ctrport::posix_clock(4,&second) || second.seconds<first.seconds || (second.seconds==first.seconds && second.nanoseconds<first.nanoseconds))return 1;
        if(ctrport::posix_clock(13,nullptr)!=-1 || *ctr_guest_errno()!=14)return 1;
        bool rejected=false;frame.time={-1,-1};try{ctrport::posix_clock(99,&frame.time);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected || frame.time.seconds!=-1)return 1;
        std::cout<<"{\"scope\":\"posix_clock_host_contract\",\"realtime_bounds_20ms\":true,\"monotonic\":true,\"sentinels\":true,\"errno_null14\":true,\"unsupported_preserves\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--netctl-unavailable-control") {
        std::array<unsigned char,256> output{};output.fill(0x91);auto before=output;
        for(auto code:{4,14,-1})if(static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_netctl_info),code,reinterpret_cast<std::uint64_t>(output.data())))!=static_cast<int>(0x80412108U) || output!=before)return 1;
        if(ctr_netctl_info(4,nullptr,0x1234)!=static_cast<int>(0x80412108U))return 1;
        std::cout<<"{\"scope\":\"unavailable_runtime_network\",\"explicit_not_connected\":true,\"outputs_unchanged\":true,\"success_stub\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--enter-button-control") {
        _putenv_s("CTR_ENTER_BUTTON","");int value=91;
        if(ctrport::system_parameter(1000,&value) || value!=1)return 1;
        for(const char* setting:{"0","1"}){
            _putenv_s("CTR_ENTER_BUTTON",setting);
            if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_system_parameter),1000,reinterpret_cast<uint64_t>(&value)) || value!=setting[0]-'0')return 1;
        }
        unsigned rejected=0;
        for(const char* invalid:{"2","-1","cross","01","1234567890123456"}){
            _putenv_s("CTR_ENTER_BUTTON",invalid);value=91;
            try{ctrport::system_parameter(1000,&value);}catch(...){if(value==91)++rejected;}
        }
        _putenv_s("CTR_ENTER_BUTTON","");if(rejected!=5)return 1;
        std::cout<<"{\"scope\":\"enter_button_control\",\"runtime_default_cross\":true,\"configured_circle_and_cross\":true,\"SysV2\":true,\"negative_rejections\":5,\"failure_preserves_output\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--system-language-control") {
        _putenv_s("CTR_SYSTEM_LANGUAGE","");int value=91;
        if(ctrport::system_parameter(1,&value) || value!=1 || ctrport::system_parameter(1,nullptr)!=static_cast<int>(0x80a10003U))return 1;
        _putenv_s("CTR_SYSTEM_LANGUAGE","4");
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_system_parameter),1,reinterpret_cast<std::uint64_t>(&value)) || value!=4)return 1;
        _putenv_s("CTR_SYSTEM_LANGUAGE","999");value=91;unsigned rejected=0;
        try{ctrport::system_parameter(1,&value);}catch(const std::runtime_error&){++rejected;}
        try{ctrport::system_parameter(2,&value);}catch(const std::runtime_error&){++rejected;}
        _putenv_s("CTR_SYSTEM_LANGUAGE","");if(rejected!=2 || value!=91)return 1;
        std::cout<<"{\"scope\":\"virtual_console_language\",\"default_us_english\":true,\"configured_italian\":true,\"failure_preserves_output\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--semaphore-poll-control") {
        auto& sem=ctrport::kernel_semaphores;std::uint32_t id=0;
        if(sem.create(&id,"poll",1,7,7,nullptr))return 1;
        const auto call=[&](int need){return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_semaphore_poll),id,need));};
        if(call(8)!=static_cast<int>(0x80020016U) || call(0)!=static_cast<int>(0x80020016U) || sem.poll(0,1)!=static_cast<int>(0x80020003U))return 1;
        std::promise<void> release;auto start=release.get_future().share();std::vector<std::future<int>> attempts;
        for(unsigned i=0;i<8;++i)attempts.push_back(std::async(std::launch::async,[&]{start.wait();return call(2);}));
        release.set_value();unsigned consumed=0,busy=0;
        for(auto& attempt:attempts){auto result=attempt.get();if(!result)++consumed;else if(result==static_cast<int>(0x80020010U))++busy;else return 1;}
        if(consumed!=3 || busy!=5 || sem.find(id)->tokens!=1 || call(2)!=static_cast<int>(0x80020010U) || call(1) || sem.find(id)->tokens)return 1;
        std::cout<<"{\"scope\":\"semaphore_poll_sysv\",\"consumers\":3,\"busy\":5,\"no_partial_consume\":true,\"invalid_handle_count\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--semaphore-signal-bridge-control") {
        std::uint32_t id=0;if(ctrport::kernel_semaphores.create(&id,"bridge",2,0,1,nullptr)) return 1;
        const auto call=[&](int count) {return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_semaphore_signal),id,count));};
        if(call(1) || call(1)!=static_cast<int>(0x80020016U) || ctrport::kernel_semaphores.wait(id,1,nullptr) || call(0)!=static_cast<int>(0x80020016U)) return 1;
        std::cout<<"{\"scope\":\"semaphore_signal_bridge\",\"actual_token_received\":true,\"overflow_no_mutation\":true,\"invalid_count\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--semaphore-wait-control") {
        ctrport::KernelSemaphores sems;std::uint32_t id=0,timeout=0;
        if(sems.create(&id,"fixture",2,1,2,nullptr) || sems.wait(id,1,nullptr)) return 1;
        if(sems.wait(id,1,&timeout)!=static_cast<int>(0x8002003cU)) return 1;
        bool rejected=false;try {sems.wait(id,1,nullptr);}catch(const std::exception&) {rejected=true;}if(!rejected) return 1;
        if(sems.signal(id,2) || sems.signal(id,1)!=static_cast<int>(0x80020016U) || sems.wait(id,1,nullptr) || sems.wait(id,1,nullptr)) return 1;
        if(sems.wait(999,1,nullptr)!=static_cast<int>(0x80020003U) || sems.wait(id,0,nullptr)!=static_cast<int>(0x80020016U)) return 1;
        std::cout<<"{\"scope\":\"semaphore_wait_signal\",\"actual_tokens\":true,\"zero_timeout\":true,\"overflow_no_mutation\":true,\"blocking_explicit_missing\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--semaphore-create-control") {
        ctrport::KernelSemaphores sems;std::uint32_t id=999;
        if(sems.create(&id,"fixture",2,2,3,nullptr)) return 1;
        auto object=sems.find(id);if(!object || object->attribute!=2 || object->maximum!=3) return 1;
        std::uint32_t zero=0;if(!object || object->tokens!=2 || sems.wait(id,2,&zero) || sems.wait(id,1,&zero)!=static_cast<int>(0x8002003cU) || sems.signal(id,1) || sems.wait(id,1,&zero)) return 1;
        id=999;if(sems.create(&id,"fixture",3,0,1,nullptr)!=static_cast<int>(0x80020016U) || id!=999 || sems.find(999)) return 1;
        bool rejected=false;try {sems.create(&id,"fixture",0,0,1,reinterpret_cast<void*>(1));}catch(const std::exception&) {rejected=true;}if(!rejected || id!=999) return 1;
        std::cout<<"{\"scope\":\"semaphore_create\",\"real_count_consume_signal\":true,\"invalid_no_write\":true,\"options_rejected\":true,\"guest_wait_order_supported\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--user-service-init-control") {
        ctrport::UserService service;
        if(service.initialized() || service.terminate()!=static_cast<int>(0x80960002U) || service.initialize(nullptr) || !service.initialized() || service.initialize(nullptr)!=static_cast<int>(0x80960003U)) return 1;
        bool rejected=false;try {service.initialize(reinterpret_cast<void*>(1));}catch(const std::exception&) {rejected=true;}
        if(!rejected || !service.initialized() || service.terminate() || service.initialized() || service.initialize(nullptr)) return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_user_service_initialize),0,0) || !ctrport::user_service.initialized()) return 1;
        std::cout<<"{\"scope\":\"user_service_lifecycle\",\"owned_session\":true,\"repeat_init_error\":true,\"terminate_reinitialize\":true,\"unsupported_params_rejected\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--tsc-read-control") {
        const auto frequency=ctrport::native_tsc_frequency();
        const auto first=ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_read_tsc),0,0);
        const auto direct=ctrport::fenced_tsc();Sleep(50);
        const auto last=ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_read_tsc),0,0);
        if(direct<first || last<=direct || last-first<frequency/25 || last-first>frequency/5) return 1;
        std::cout<<"{\"scope\":\"native_tsc_read\",\"bridge_brackets_direct_read\":true,\"elapsed_ticks\":"<<(last-first)<<",\"hz\":"<<frequency<<"}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--tsc-frequency-control") {
        const auto frequency=ctrport::native_tsc_frequency(),independent=ctrport::measure_tsc(150);
        const auto difference=frequency>independent?frequency-independent:independent-frequency;
        if(difference>frequency/100 || ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_tsc_frequency),0,0)!=frequency) return 1;
        std::cout<<"{\"scope\":\"native_tsc_frequency\",\"hz\":"<<frequency<<",\"independent_hz\":"<<independent<<",\"sysv_stable\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-affinity-control") {
        DWORD_PTR process=0,system=0;if(!GetProcessAffinityMask(GetCurrentProcess(),&process,&system)) return 1;
        const auto selected=(process&0x7fULL)&(~(process&0x7fULL)+1);if(!selected) return 1;
        const auto previous=SetThreadAffinityMask(GetCurrentThread(),process);if(!previous) return 1;
        const auto handle=ctrport::thread_self();
        int result=static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_set_affinity),handle,selected));
        const auto observed=SetThreadAffinityMask(GetCurrentThread(),previous);
        if(result || observed!=selected || ctrport::thread_set_affinity(1,selected)!=static_cast<int>(0x80020003U) || ctrport::thread_set_affinity(handle,0)!=static_cast<int>(0x80020016U) || ctrport::thread_set_affinity(handle,128)!=static_cast<int>(0x80020016U)) return 1;
        std::cout<<"{\"scope\":\"thread_affinity\",\"actual_host_mask_readback\":true,\"original_mask_restored\":true,\"invalid_controls\":3}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--cpu-mode-control") {
        if(ctrport::cpu_mode_from_attributes(0)!=0 || ctrport::cpu_mode_from_attributes(1U<<15)!=0 || ctrport::cpu_mode_from_attributes(1U<<16)!=5 || ctrport::cpu_mode_from_attributes(3U<<15)!=2) return 1;
        bool rejected=false;try {ctrport::process_cpu_mode();}catch(const std::exception&) {rejected=true;}if(!rejected) return 1;
        ctrport::cpu_mode_retail_base=true;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_cpu_mode),0,0)!=0) return 1;
        std::cout<<"{\"scope\":\"cpu_mode\",\"four_attribute_branches\":true,\"missing_profile_rejected\":true,\"sysv_retail_mode\":0}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-key-values-control") {
        ctrport::ThreadKeys keys;std::uint32_t key=0;int first=1,second=2;
        if(keys.create(&key,0) || keys.get(key) || keys.set(key,&first) || keys.get(key)!=&first) return 1;
        bool isolated=false;std::thread other([&]{isolated=!keys.get(key) && !keys.set(key,&second) && keys.get(key)==&second;});other.join();
        if(!isolated || keys.get(key)!=&first || keys.set(key,nullptr) || keys.get(key)) return 1;
        keys.set(key,&first);if(keys.erase(key) || keys.get(key) || keys.set(key,&first)!=static_cast<int>(0x80020016U) || keys.erase(key)!=static_cast<int>(0x80020016U)) return 1;
        if(keys.create(&key,0) || keys.get(key) || keys.get(256)) return 1;
        std::cout<<"{\"scope\":\"thread_key_values\",\"thread_isolation\":true,\"delete_reuse_empty\":true,\"null_clear\":true,\"invalid_rejected\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-key-create-control") {
        ctrport::ThreadKeys keys;std::uint32_t output=999;
        if(keys.create(nullptr,0)!=static_cast<int>(0x80020016U)) return 1;
        bool bad=false;try {keys.create(reinterpret_cast<std::uint32_t*>(1),0);}catch(const std::exception&) {bad=true;}if(!bad) return 1;
        bad=false;try {keys.create(&output,1);}catch(const std::exception&) {bad=true;}if(!bad || output!=999) return 1;
        for(std::uint32_t i=0;i<256;++i) if(keys.create(&output,0) || output!=i) return 1;
        output=999;if(keys.create(&output,0)!=static_cast<int>(0x80020023U) || output!=999) return 1;
        if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_key_create),reinterpret_cast<std::uint64_t>(&output),0) || output!=0) return 1;
        std::cout<<"{\"scope\":\"thread_key_create\",\"unique_keys\":256,\"exhaustion_no_write\":true,\"destructor_explicit_missing\":true,\"sysv_bridge\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--session-resolver-control") {
        try {
            ctrport::OriginalCallSession session;auto* address=session.resolve_address("leaf");
            if(!address || address!=session.resolve_address("leaf")) return 1;
            const auto direct=ctrport::ctr_call_original_integer2(address,7,11);
            if(direct!=session.call("leaf",7,11)) return 1;
            bool rejected=false;try {session.resolve_address("undeclared");}catch(const std::exception&) {rejected=true;}
            if(!rejected) return 1;
            std::cout<<"{\"mapped_address_stable\":true,\"actual_original_return\":"<<direct<<",\"undeclared_id_rejected\":true}\n";return 0;
        }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 2;}
    }
    if(argc==2 && std::string(argv[1])=="--thread-attributes-destroy-control") {
        std::uint64_t cell=0;ctrport::thread_attributes_init(&cell);auto handle=cell;
        std::weak_ptr<ctrport::ThreadAttributes> lifetime=ctrport::thread_attributes.at(cell);
        const auto call=[&]{return static_cast<int>(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_attributes_destroy),reinterpret_cast<std::uint64_t>(&cell),0,0));};
        if(call() || cell || !lifetime.expired() || ctrport::thread_attributes.count(handle)) return 1;
        if(call()!=static_cast<int>(0x80020016U) || cell) return 1;
        cell=handle;if(call()!=static_cast<int>(0x80020016U) || cell!=handle) return 1;
        std::cout<<"{\"actual_release_and_cell_clear\":true,\"double_destroy_einval\":true,\"stale_handle_einval\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-attributes-affinity-control") {
        std::uint64_t cell=0,output=123;ctrport::thread_attributes_init(&cell);
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_attributes_affinity),reinterpret_cast<std::uint64_t>(&cell),reinterpret_cast<std::uint64_t>(&output),0) || output!=~std::uint64_t{0}) return 1;
        auto saved=cell;cell=1;output=123;
        if(ctrport::thread_attributes_affinity(&cell,&output)!=static_cast<int>(0x80020016U) || output!=123) return 1;
        cell=saved;ctrport::thread_attributes.at(cell)->cpu_set=1;
        bool rejected=false;try {ctrport::thread_attributes_affinity(&cell,&output);}catch(const std::exception&) {rejected=true;}
        if(!rejected || output!=123) return 1;
        std::cout<<"{\"unset_means_unrestricted\":true,\"unknown_attribute_einval\":true,\"configured_set_rejected_without_write\":true}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-attributes-current-control") {
        std::uint64_t cell=0;ctrport::thread_attributes_init(&cell);
        auto handle=ctrport::thread_self();
        const auto call=[&](std::uint64_t h){return static_cast<int>(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_attributes_current),h,reinterpret_cast<std::uint64_t>(&cell),0));};
        if(call(handle)) return 1;
        auto object=ctrport::thread_attributes.at(cell);const auto local=reinterpret_cast<std::uint64_t>(&cell);
        if(local<object->stack_address || local>=object->stack_address+object->stack_size) return 1;
        auto saved=object->stack_address;
        if(call(1)!=static_cast<int>(0x80020003U) || object->stack_address!=saved) return 1;
        std::cout<<"{\"actual_stack_contains_local\":true,\"unknown_thread_esrch\":true,\"scheduler_implemented\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-self-control") {
        const auto call=[] {return ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_self),0,0,0);};
        auto first=call();if(!first || call()!=first) return 1;
        auto worker=std::async(std::launch::async,call);auto second=worker.get();
        if(!second || first==second || ctrport::thread_identities.at(first)->host!=std::this_thread::get_id()) return 1;
        std::cout<<"{\"stable_current_thread\":true,\"distinct_worker_thread\":true,\"scheduler_implemented\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--thread-attributes-control") {
        std::uint64_t cell=0;
        if(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_thread_attributes_init),reinterpret_cast<std::uint64_t>(&cell),0,0) || !cell) return 1;
        auto object=ctrport::thread_attributes.at(cell);
        if(object->priority!=700 || object->inherit!=4 || object->stack_size!=1048576 || !object->system_scope) return 1;
        bool rejected=false;try {ctrport::thread_attributes_init(reinterpret_cast<std::uint64_t*>(1));}catch(const std::exception&) {rejected=true;}
        if(!rejected) return 1;
        std::cout<<"{\"owned_defaults\":true,\"invalid_output_rejected\":true,\"scheduler_implemented\":false}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--orbis-unlock-control") {
        std::uint64_t cell=0;
        const auto call=[&]{return static_cast<int>(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_orbis_mutex_unlock),reinterpret_cast<std::uint64_t>(&cell),0,0));};
        if(ctrport::kernel_mutex_lock(&cell) || call()!=0 || call()!=static_cast<int>(0x80020001U)) return 1;
        std::cout<<"{\"scope\":\"orbis_mutex_unlock_sysv\",\"actual_unlock\":true,\"unowned_eperm\":2147614721}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--orbis-lock-control") {
        std::uint64_t cell=0;
        const auto call=[&]{return static_cast<int>(ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_orbis_mutex_lock),reinterpret_cast<std::uint64_t>(&cell),0,0));};
        if(call()!=0 || call()!=static_cast<int>(0x8002000bU) || ctrport::kernel_mutex_unlock(&cell)) return 1;
        std::cout<<"{\"scope\":\"orbis_mutex_sysv_error_conversion\",\"actual_lock_unlock\":true,\"relock_edeadlk\":2147614731}\n";return 0;
    }

    if(argc==2 && std::string(argv[1])=="--named-mutex-control") return ctrport::named_mutex_control();
    if(argc==2 && std::string(argv[1])=="--asan-mode-control") {
        const auto result=ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_asan_enabled),0,0,0);
        if(result!=ctr_asan_enabled()) return 1;
        std::cout<<"{\"scope\":\"actual_build_sanitizer_query\",\"enabled\":"<<result<<"}\n";return 0;
    }

    if(argc==2 && std::string(argv[1])=="--direct-map-missing-control") {
        ctrport::OriginalRegisterArguments args;args.integer_count=6;void* output=nullptr;
        args.integers={reinterpret_cast<std::uint64_t>(&output),16384,51,0,0,2097152};
        (void)ctrport::invoke_original_registers(reinterpret_cast<void*>(&ctrport::ctr_sysv_map_direct),args,6,0);return 1;
    }

    if(argc==2 && std::string(argv[1])=="--owned-output-control") return ctrport::owned_output_control();
    if(argc==2 && std::string(argv[1])=="--direct-map-control") return ctrport::direct_map_control();
    if(argc==2 && std::string(argv[1])=="--direct-allocation-missing-control") {
        ctrport::OriginalRegisterArguments args;args.integer_count=6;std::int64_t output=-1;
        args.integers={0,65536,16384,16384,0,reinterpret_cast<std::uint64_t>(&output)};
        (void)ctrport::invoke_original_registers(reinterpret_cast<void*>(&ctrport::ctr_sysv_allocate_direct),args,6,0);return 1;
    }

    if(argc==2 && std::string(argv[1])=="--direct-allocation-control") return ctrport::direct_allocation_control();
    if(argc==2 && std::string(argv[1])=="--neo-mode-missing-control") {(void)ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_is_neo_mode),0,0,0);return 1;}

    if(argc==2 && std::string(argv[1])=="--heap-trace-control") return ctrport::heap_trace_control();
    if(argc==2 && std::string(argv[1])=="--typed-session-integer3-control") {
        ctrport::OriginalCallSession session;unsigned char source[64],destination[64];
        for(unsigned i=0;i<64;++i) {source[i]=static_cast<unsigned char>(i*3+1);destination[i]=0;}
        ctrport::OriginalRegisterArguments args;args.integer_count=3;
        args.integers={reinterpret_cast<std::uint64_t>(destination),reinterpret_cast<std::uint64_t>(source),64,0,0,0};
        const auto result=session.call_registers("copy",args);
        if(result.integer!=reinterpret_cast<std::uint64_t>(destination) || std::memcmp(source,destination,64)) throw std::runtime_error("Original memcpy session differs");
        std::cout<<"{\"scope\":\"whole_original_memcpy_typed_session\",\"bytes\":64,\"return_and_output_match\":true,\"runtime_replacements\":0}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--typed-session-control") {
        ctrport::OriginalCallSession session;ctrport::OriginalRegisterArguments args;
        args.xmm_count=1;args.xmm_kinds[0]=ctrport::XmmArgumentKind::Float32;args.xmm_bits[0]=0xc0200000U;args.return_kind=ctrport::OriginalReturnKind::Float32;
        auto result=session.call_registers("absolute",args);
        if((result.xmm_bits&0xffffffffULL)!=0x40200000U) throw std::runtime_error("Original fabsf result differs");
        unsigned rejected=0;
        args.integer_count=1;try {session.call_registers("absolute",args);} catch(const std::runtime_error&) {++rejected;}
        args.integer_count=0;args.xmm_kinds[0]=ctrport::XmmArgumentKind::Float64;try {session.call_registers("absolute",args);} catch(const std::runtime_error&) {++rejected;}
        args.xmm_kinds[0]=ctrport::XmmArgumentKind::Float32;args.return_kind=ctrport::OriginalReturnKind::Integer;try {session.call_registers("absolute",args);} catch(const std::runtime_error&) {++rejected;}
        if(rejected!=3) throw std::runtime_error("Typed session negatives differ");
        std::cout<<"{\"scope\":\"whole_original_fabsf_typed_session\",\"input_bits\":3223322624,\"output_bits\":1075838976,\"negative_rejections\":3,\"runtime_replacements\":0}\n";return 0;
    }
    if(argc==2 && std::string(argv[1])=="--original-register-control") return ctrport::original_register_control();
    if(argc==2 && std::string(argv[1])=="--application-heap-control") return ctrport::application_heap_control();
    if(argc==2 && std::string(argv[1])=="--procparam-missing-control") { (void)ctr_get_proc_param(0x1234);return 1; }
    if(argc==2 && std::string(argv[1])=="--thread-atexit-report-control") return ctrport::thread_atexit_report_control();
    if(argc==2 && std::string(argv[1])=="--thread-atexit-control") return ctrport::thread_atexit_control();
    if(argc==2 && std::string(argv[1])=="--thread-destructor-control") return ctrport::thread_destructor_control();
    if(argc==2 && std::string(argv[1])=="--original-call3-control") {
        std::uint64_t cells[2]={};
        const auto result=ctrport::ctr_call_original_integer3(reinterpret_cast<void*>(&ctrport::ctr_sysv_integer3_fixture),reinterpret_cast<std::uint64_t>(cells),0x1122334455667788ULL,0x8877665544332211ULL);
        const bool passed=cells[0]==0x1122334455667788ULL && cells[1]==0x8877665544332211ULL && result==cells[1];
        std::cout << "{\"scope\":\"synthetic_sysv_integer3_control\",\"passed\":" << (passed?"true":"false") << ",\"original_game_executed\":false}\n";
        return passed?0:1;
    }

    if(argc==2 && std::string(argv[1])=="--thread-inherit-control") {return ctrport::thread_inherit_control();}
    if(argc==2 && std::string(argv[1])=="--thread-priority-control") {
        try { return ctrport::thread_priority_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--thread-policy-control") {
        try { return ctrport::thread_policy_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--thread-stack-size-control") {
        try { return ctrport::thread_stack_size_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--video-adjust-color-negative-control")return ctrport::video_adjust_color_negative_control();
    if(argc==2 && std::string(argv[1])=="--video-gamma-control")return ctrport::video_gamma_control();
    if(argc==2 && std::string(argv[1])=="--mutex-destroy-control")return ctrport::mutex_destroy_control();
    if(argc==2 && std::string(argv[1])=="--condition-destroy-control")return ctrport::condition_destroy_control();
    if(argc==2 && std::string(argv[1])=="--condition-signal-control")return ctrport::condition_signal_control();
    if(argc==2 && std::string(argv[1])=="--condition-init-control") {
        try { return ctrport::condition_init_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--condition-attributes-control") {
        try { return ctrport::condition_attributes_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--kernel-condition-control") {
        try { return ctrport::kernel_condition_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--kernel-mutex-control") {
        try { return ctrport::kernel_mutex_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==3 && std::string(argv[1])=="--kernel-time-invalid") {
        const std::string domain=argv[2];
        DWORD protection=domain=="readonly"?PAGE_READONLY:PAGE_NOACCESS;
        ctrport::MappedImage page;
        std::uint64_t address=1;
        if(domain!="unmapped") {
            if(domain!="readonly" && domain!="noaccess") return 3;
            page.reset(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,protection)));
            if(!page) return 3;
            address=reinterpret_cast<std::uint64_t>(page.get());
        }
        (void)ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_gettimeofday),address,0);
        return 3; // An unsupported output must not return success.
    }
    if(argc==3 && std::string(argv[1])=="--original-libc-time") {
        try {
            ctrport::ImageMetadata info;std::vector<ctrport::MappedImage> images;
            images.push_back(ctrport::load_retail_image(argv[2],false,&info));
            if(info.sha256!="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816") throw std::runtime_error("Libc time SHA differs");
            std::vector<ctrport::ImageMetadata> metadata{info};
            auto functions=ctrport::install_missing_traps(images,metadata,{{"n88vx3C5nW8","libkernel","libkernel",reinterpret_cast<const void*>(&ctr_sysv_gettimeofday)}});
            auto data=ctrport::install_missing_data_guards(images,metadata,{{"f7uOxY9mM1U","libkernel","libkernel",ctrport::process_guard()}});
            DWORD old=0;for(const auto& span:info.executable_spans) if(!VirtualProtect(images[0].get()+span.first,static_cast<SIZE_T>(span.second-span.first),PAGE_EXECUTE_READ,&old)) throw std::runtime_error("Original libc permission failed");
            const auto low=ctrport::unix_microseconds();
            const auto result=ctrport::invoke_session_target(images[0].get()+0x3c0a0,0,0,functions,data);
            const auto high=ctrport::unix_microseconds();
            if(result<low || result>high) throw std::runtime_error("Original libc returned time outside host interval");
            std::cout<<"{\"scope\":\"whole_original_libc_time_caller\",\"address\":245920,\"original_returned\":true,\"time_within_host_interval\":true,\"main_reached\":false,\"runtime_replacements\":0}\n";
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--kernel-time-control") {
        try { return ctrport::kernel_time_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--process-guard-control") {
        try { return ctrport::process_guard_control(); }
        catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 4; }
    }
    if(argc==2 && (std::string(argv[1])=="--session-missing-control" || std::string(argv[1])=="--session-data-control")) {
        const bool objects=std::string(argv[1])=="--session-data-control";
        ctrport::MissingTrapBank functions,data;
        functions.memory.reset(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
        if(!functions.memory) return 3;
        functions.memory.get()[0]=0x0f;functions.memory.get()[1]=0x0b;
        if(objects) {
            // Synthetic SysV target: read one byte through its first argument.
            const unsigned char target[]={0x48,0x89,0xf8,0x8a,0x00,0xc3};
            std::memcpy(functions.memory.get(),target,sizeof(target));
            data.memory.reset(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS)));
            if(!data.memory) return 3;
            data.records.push_back({0,0,{"synthetic_data_nid","synthetic_library","synthetic_image"}});
        }
        functions.records.push_back({0,0,{"synthetic_nid","synthetic_library","synthetic_image"}});
        DWORD old=0;
        if(!VirtualProtect(functions.memory.get(),4096,PAGE_EXECUTE_READ,&old)) return 3;
        ctrport::invoke_session_target(functions.memory.get(),reinterpret_cast<std::uint64_t>(data.memory.get()),0,functions,data);
        return 3; // The missing call must never return.
    }
    if(argc==4 && std::string(argv[1])=="--original-session-probe") {
        try {
            ctrport::OriginalCallSession session;
            std::ifstream input(argv[2],std::ios::binary);std::ofstream output(argv[3],std::ios::binary);
            if(!input || !output) throw std::runtime_error("Session fixture/output unavailable");
            std::uint64_t argument=0;unsigned cases=0;
            while(input.read(reinterpret_cast<char*>(&argument),8)) {
                const auto result=session.call("leaf",argument,0);
                output.write(reinterpret_cast<const char*>(&result),8);++cases;
            }
            if(!input.eof() || input.gcount()!=0 || cases==0 || !output) throw std::runtime_error("Session fixture extent invalid");
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==3 && (std::string(argv[1])=="--original-guard-swap" || std::string(argv[1])=="--original-guard-mismatch")) {
        try {
            ctrport::ImageMetadata info; std::vector<ctrport::MappedImage> images;
            images.push_back(ctrport::load_retail_image(argv[2],false,&info));
            if(info.sha256!="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1") throw std::runtime_error("Guard witness version differs");
            std::vector<ctrport::ImageMetadata> metadata{info};
            const bool mismatch=std::string(argv[1])=="--original-guard-mismatch";
            alignas(8) unsigned char buffers[48];for(unsigned i=0;i<48;++i) buffers[i]=static_cast<unsigned char>(i);
            // Negative fixture deliberately aliases a writable guard cell. The
            // production process guard remains read-only and is never changed.
            const void* guard=mismatch?static_cast<const void*>(buffers):ctrport::process_guard();
            auto functions=ctrport::install_missing_traps(images,metadata);
            auto data=ctrport::install_missing_data_guards(images,metadata,{{"f7uOxY9mM1U","libkernel","libkernel",guard}});
            DWORD old=0;
            if(!VirtualProtect(images[0].get()+0x207000,4096,PAGE_EXECUTE_READ,&old)) throw std::runtime_error("Guard witness protection failed");
            if(mismatch && !VirtualProtect(images[0].get()+0x1adb000,4096,PAGE_EXECUTE_READ,&old)) throw std::runtime_error("Guard failure thunk protection failed");
            auto* target=ctrport::resolve_original_call(images,metadata,info.sha256,info.sha256,0x207dd0);
            const auto before=*ctrport::process_guard();
            (void)ctrport::invoke_session_target(target,reinterpret_cast<std::uint64_t>(buffers),reinterpret_cast<std::uint64_t>(buffers+24),functions,data);
            if(mismatch) throw std::runtime_error("Original mismatched canary unexpectedly returned");
            for(unsigned i=0;i<48;++i) if(buffers[i]!=static_cast<unsigned char>((i+24)%48)) throw std::runtime_error("Original swap result differs");
            if(before!=*ctrport::process_guard()) throw std::runtime_error("Original call changed guard");
            std::cout<<"{\"scope\":\"whole_original_guard_swap\",\"address\":2129360,\"original_returned\":true,\"guard_stable\":true,\"fixture\":\"synthetic_disjoint24byte_buffers\",\"main_reached\":false,\"runtime_replacements\":0}\n";
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==3 && std::string(argv[1])=="--original-game-leaf") {
        try {
            if(std::string(argv[1])=="--entry-after-libc-init-images") {ctrport::kernel_files.configure(std::filesystem::path(argv[2]).parent_path());ctrport::kernel_files.configure_user("runtime/user/data");}
            std::vector<ctrport::MappedImage> images;ctrport::ImageMetadata info;
            images.push_back(ctrport::load_retail_image(argv[2],false,&info));
            const std::string pin="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1";
            if(info.sha256!=pin) throw std::runtime_error("Game leaf version differs");
            std::vector<ctrport::ImageMetadata> metadata{info};DWORD old=0;
            if(!VirtualProtect(images.front().get()+0x44000,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),images.front().get()+0x44000,4096)) throw std::runtime_error("Game leaf permission failed");
            auto* target=ctrport::resolve_original_call(images,metadata,pin,pin,0x446f0);
            const auto result=ctrport::ctr_call_original_integer2(target,0,0);
            if(result!=0x30) throw std::runtime_error("Original game leaf return differs");
            std::cout<<"{\"scope\":\"whole_original_retail_game_leaf\",\"offset\":280304,\"length\":6,\"result\":48,\"original_returned\":true,\"main_reached\":false,\"runtime_replacements\":0}\n";
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--original-xmm-control") {
        const bool passed=ctr_original_xmm_control()==1;
        std::cout<<"{\"scope\":\"synthetic_original_bridge_XMM6_15_clobber\",\"passed\":"<<(passed?"true":"false")<<",\"registers\":10,\"retail_executed\":false}\n";
        return passed?0:5;
    }
    if(argc==2 && std::string(argv[1])=="--original-call-control") {
        try {
            const std::uint64_t cases[][2]={{0,0},{1,2},{~0ULL,0},{0x123456789abcdef0ULL,0xfedcba9876543210ULL}};
            for(const auto& pair:cases)
                if(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_original_integer_target_control),pair[0],pair[1])!=(pair[0]^pair[1]))
                    throw std::runtime_error("Original integer argument control differs");
            if(std::string(argv[1])=="--entry-after-libc-init-images") {ctrport::kernel_files.configure(std::filesystem::path(argv[2]).parent_path());ctrport::kernel_files.configure_user("runtime/user/data");}
            std::vector<ctrport::MappedImage> images;
            images.emplace_back(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
            if(!images.front()) throw std::runtime_error("Resolver control allocation failed");
            ctrport::ImageMetadata info;info.sha256="synthetic";info.executable_spans.push_back({0,16});
            std::vector<ctrport::ImageMetadata> metadata{info};
            unsigned rejected=0;
            for(unsigned check=0;check<4;++check) {
                try {
                    ctrport::resolve_original_call(images,metadata,check==0?"wrong":"synthetic",check==1?"missing":"synthetic",check==2?16:0);
                } catch(const std::runtime_error&) {++rejected;}
            }
            if(rejected!=4) throw std::runtime_error("Resolver negative control accepted");
            DWORD old=0;if(!VirtualProtect(images.front().get(),4096,PAGE_EXECUTE_READ,&old)) throw std::runtime_error("Resolver control permission failed");
            if(ctrport::resolve_original_call(images,metadata,"synthetic","synthetic",0)!=images.front().get()) throw std::runtime_error("Resolver positive differs");
            images.emplace_back(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
            if(!images.back())throw std::runtime_error("Callback module fixture allocation failed");
            auto module_info=info;module_info.sha256="synthetic_module";metadata.push_back(module_info);
            if(!VirtualProtect(images.back().get(),4096,PAGE_EXECUTE_READ,&old))throw std::runtime_error("Callback module permission failed");
            if(ctrport::resolve_owned_callback(images,metadata,"synthetic",images.back().get())!=1 || ctrport::resolve_owned_callback(images,metadata,"synthetic",images.front().get())!=0)throw std::runtime_error("Multiimage callback identity differs");
            unsigned callback_rejected=0;
            for(auto target:{images.back().get()+16,reinterpret_cast<unsigned char*>(1)})try{ctrport::resolve_owned_callback(images,metadata,"synthetic",target);}catch(const std::runtime_error&){++callback_rejected;}
            if(!VirtualProtect(images.back().get(),4096,PAGE_READWRITE,&old))throw std::runtime_error("Callback negative permission failed");
            try{ctrport::resolve_owned_callback(images,metadata,"synthetic",images.back().get());}catch(const std::runtime_error&){++callback_rejected;}
            try{ctrport::resolve_owned_callback(images,metadata,"wrong",images.front().get());}catch(const std::runtime_error&){++callback_rejected;}
            if(callback_rejected!=4)throw std::runtime_error("Multiimage callback negative admitted");
            std::cout << "{\"scope\":\"synthetic_original_integer2_and_resolver\",\"argument_cases\":4,\"negative_cases\":4,\"positive_resolver\":true,\"XMM_sentinel_checked\":false,\"retail_executed\":false}\n";
            return 0;
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 5;}
    }
    if(argc==2 && std::string(argv[1])=="--startup-abi-control") {
        alignas(8) const std::uint64_t block[]={1,0x123456789abcdef0ULL};
        std::uint64_t observed[5]={};
        ctr_startup_control(block,0xfedcba9876543210ULL,observed);
        const bool passed=observed[0]==8 && observed[1]==block[0] && observed[2]==block[1] &&
                          observed[3]==reinterpret_cast<std::uint64_t>(block) && observed[4]==0xfedcba9876543210ULL;
        std::cout << "{\"scope\":\"synthetic_startup_jump_stack_registers\",\"passed\":" << (passed?"true":"false")
                  << ",\"entry_rsp_mod16\":" << observed[0]
                  << ",\"stack_startup_words_match\":true,\"retail_executed\":false,\"main_reached\":false}\n";
        return passed?0:1;
    }

    if (argc == 2 && (std::string(argv[1]) == "--sysmodule-unavailable-bridge-control" || std::string(argv[1]) == "--sysmodule-unknown-bridge-control")) {
        const auto id=std::string(argv[1])=="--sysmodule-unknown-bridge-control"?0xbbULL:0xbaULL;
        return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_sysmodule_load),id,0));
    }
    if (argc == 2 && std::string(argv[1]) == "--direct-memory-missing-bridge-control") {
        return static_cast<int>(ctrport::ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_direct_memory_size),0,0));
    }
    if (argc == 2 && std::string(argv[1]) == "--direct-memory-control") {
        try {return ctrport::direct_memory_control();}
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if(argc==2 && std::string(argv[1])=="--original-module-state-control") return ctrport::original_module_state_control();
    if (argc == 2 && std::string(argv[1]) == "--sysmodule-state-control") {
        try {return ctrport::sysmodule_state_control();}
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if (argc == 2 && std::string(argv[1]) == "--random-bridge-control") {
        std::array<unsigned char,66> bytes;bytes.fill(0x5a);
        const auto target=reinterpret_cast<void*>(&ctr_sysv_random_bytes);
        for(auto size:{0ULL,1ULL,64ULL}) {
            if(ctrport::ctr_call_original_integer2(target,reinterpret_cast<std::uint64_t>(bytes.data()+1),size)!=0 || bytes.front()!=0x5a || bytes.back()!=0x5a) return 1;
        }
        const auto before=bytes;
        const auto error=static_cast<std::uint32_t>(ctrport::ctr_call_original_integer2(target,reinterpret_cast<std::uint64_t>(bytes.data()),65));
        if(error!=0x817c0016U || bytes!=before) return 1;
        std::cout<<"{\"scope\":\"synthetic_sysv_random_bridge\",\"sizes_verified\":[0,1,64],\"oversize_no_write\":true,\"retail_executed\":false,\"main_reached\":false}\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--random-service-control") {
        try {return ctrport::random_service_control();}
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if (argc == 2 && std::string(argv[1]) == "--kernel-mutex-attribute-destroy-control") {
        try {return ctrport::kernel_mutex_attributes_destroy_control();}
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if (argc == 2 && std::string(argv[1]) == "--kernel-recursive-mutex-control") {
        try {return ctrport::kernel_recursive_mutex_control();}
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if (argc == 2 && std::string(argv[1]) == "--kernel-mutex-settype-control") {
        try {return ctrport::kernel_mutex_attributes_settype_control();}
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if (argc == 2 && std::string(argv[1]) == "--kernel-mutex-attributes-control") {
        try { return ctrport::kernel_mutex_attributes_control(); }
        catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 4;}
    }
    if (argc == 2 && std::string(argv[1]) == "--missing-trap-control") {
        try { return ctrport::missing_import_control(); }
        catch(const std::exception& error) { std::cerr << "CTRPort missing trap: " << error.what() << '\n'; return 5; }
    }
    if (argc == 3 && std::string(argv[1]) == "--inspect-symbols") {
        try { auto image=ctrport::load_retail_image(argv[2],true); return 0; }
        catch(const std::exception& error) { std::cerr << "CTRPort symbols: " << error.what() << '\n'; return 4; }
    }
    if (argc >= 4 && (std::string(argv[1]) == "--map-images" || std::string(argv[1]) == "--link-images" || std::string(argv[1]) == "--trap-images" || std::string(argv[1]) == "--guard-data-images" || std::string(argv[1]) == "--dispatch-images" || std::string(argv[1]) == "--entry-boundary-images" || std::string(argv[1]) == "--original-init-env-images" || std::string(argv[1]) == "--entry-libc-boundary-images" || std::string(argv[1]) == "--entry-guard-boundary-images" || std::string(argv[1]) == "--entry-time-boundary-images" || std::string(argv[1]) == "--entry-mutex-boundary-images" || std::string(argv[1]) == "--entry-condition-boundary-images" || std::string(argv[1]) == "--entry-attributes-boundary-images" || std::string(argv[1]) == "--entry-settype-boundary-images" || std::string(argv[1]) == "--entry-recursive-boundary-images" || std::string(argv[1]) == "--entry-attribute-destroy-boundary-images" || std::string(argv[1]) == "--entry-random-boundary-images" || std::string(argv[1]) == "--entry-sysmodule-boundary-images" || std::string(argv[1]) == "--direct-memory-profile-images" || std::string(argv[1]) == "--entry-memory-boundary-images" || std::string(argv[1]) == "--entry-memory-query-boundary-images" || std::string(argv[1]) == "--original-libc-module-init-images" || std::string(argv[1]) == "--entry-after-libc-init-images")) {
        try {
            if(std::string(argv[1])=="--entry-after-libc-init-images") {
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
            }
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
            if(std::string(argv[1])!="--map-images") ctrport::bind_original_images(images,metadata);
            if(std::string(argv[1])=="--direct-memory-profile-images") {
                unsigned rejected=0;
                try {ctrport::direct_memory_size();} catch(const std::runtime_error&) {++rejected;}
                const auto proc=metadata.front().sha256=="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1"?0x2600000ULL:0x2a00000ULL;
                std::uint64_t original=0;std::memcpy(&original,images.front().get()+proc+16,8);
                const std::uint64_t invalid=0;std::memcpy(images.front().get()+proc+16,&invalid,8);
                try {ctrport::configure_direct_memory(images.front(),metadata.front());} catch(const std::runtime_error&) {++rejected;}
                std::memcpy(images.front().get()+proc+16,&original,8);
                if(ctrport::direct_memory_backing || rejected!=2) throw std::runtime_error("Profile negative accepted");
                ctrport::configure_direct_memory(images.front(),metadata.front());
                MEMORY_BASIC_INFORMATION info{};
                if(!VirtualQuery(ctrport::direct_memory_backing->address(),&info,sizeof(info)) || info.State!=MEM_RESERVE || info.RegionSize!=ctrport::direct_memory_size()) throw std::runtime_error("Profile reservation differs");
                std::cout<<"{\"scope\":\"mapped_retail_memory_profile\",\"eboot_sha256\":\""<<metadata.front().sha256<<"\",\"direct_bytes\":"<<ctrport::direct_memory_size()<<",\"negative_rejections\":2,\"original_executed\":false,\"runtime_bound\":false}\n";
                return 0;
            }
            if(std::string(argv[1])=="--original-init-env-images") return ctrport::execute_original_init_env(images,metadata);
            if(std::string(argv[1])=="--entry-libc-boundary-images") return ctrport::run_entry_boundary(images,metadata,true);
            if(std::string(argv[1])=="--original-libc-module-init-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-after-libc-init-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-memory-query-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-memory-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-sysmodule-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-random-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-attribute-destroy-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-recursive-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-settype-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-attributes-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-condition-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true,true);
            if(std::string(argv[1])=="--entry-mutex-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true,true);
            if(std::string(argv[1])=="--entry-time-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true,true);
            if(std::string(argv[1])=="--entry-guard-boundary-images") return ctrport::run_entry_boundary(images,metadata,true,true);
            if(std::string(argv[1])=="--entry-boundary-images") return ctrport::run_entry_boundary(images,metadata);
            if(std::string(argv[1])=="--dispatch-images") {
                auto functions=ctrport::install_missing_traps(images,metadata);
                auto data=ctrport::install_missing_data_guards(images,metadata);
                return ctrport::check_all_bank_slots(functions,data,images);
            }
            if(std::string(argv[1])=="--guard-data-images") {
                auto bank=ctrport::install_missing_data_guards(images,metadata);
                return ctrport::check_installed_missing_data(bank,images);
            }
            if(std::string(argv[1])=="--trap-images") {
                auto bank=ctrport::install_missing_traps(images,metadata);
                return ctrport::check_installed_missing_trap(bank);
            }
            std::cout << "{\"retained_images\":" << images.size()
                      << ",\"distinct_reservations\":true,\"executable_permissions\":false,"
                         "\"main_reached\":false,\"runtime_replacements\":0}\n";
            for (auto& image : images) {
                if (!VirtualFree(image.get(), 0, MEM_RELEASE))
                    throw std::runtime_error("native multi-image release failed");
                image.release();
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "CTRPort multi-image loader: " << error.what() << '\n';
            return 4;
        }
    }
    if (argc == 3 && std::string(argv[1]) == "--map-flat") {
        try { return ctrport::map_flat(argv[2]); }
        catch (const std::exception& error) { std::cerr << "CTRPort loader: " << error.what() << '\n'; return 4; }
    }
    if (argc == 5 && std::string(argv[1]) == "--resolve-app0") {
        try {
            std::cout << ctrport::app0_read_path(argv[2], argv[3], argv[4]).string() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "CTRPort app0: " << error.what() << '\n';
            return 4;
        }
    }
    if (argc == 2 && std::string(argv[1]) == "--abi-control") {
        const auto actual = ctr_call_sysv_probe(0xfedcba9876543210ULL, 37);
        const auto expected = ctr_msvc_probe(0xfedcba9876543210ULL, 37);
        std::cout << "{\"control\":\"synthetic_win64_sysv_win64\",\"passed\":"
                  << (actual == expected ? "true" : "false")
                  << ",\"retail_executed\":false,\"runtime_replacements\":0}\n";
        return actual == expected ? 0 : 1;
    }
    std::cerr << "CTRPort: loader not implemented; retail execution unavailable\n";
    return 4;
}
