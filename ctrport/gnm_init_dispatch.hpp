// SPDX-License-Identifier: GPL-2.0-only
// Infra transport only. No Gnm packet implementation or guest binding here.
#pragma once
#include "gnm_init_transport.h"
#include "direct_memory_copy.hpp"
#include "original_call.hpp"
#include "video/gnm_init.h"
namespace ctrport {
struct GnmInit350Binding {
    CtrGnmInit350Transport invoke=nullptr;
    CtrGuestCopyApi memory{};
    std::shared_ptr<void> owner;
    int neo_mode=-1;
    bool real_implementation=false;
};
inline std::mutex gnm_init_binding_lock;
inline std::shared_ptr<const GnmInit350Binding> gnm_init_binding;
inline void publish_gnm_init350(std::shared_ptr<const GnmInit350Binding> binding) {
    if(binding && (!binding->invoke || !binding->owner || binding->memory.size!=sizeof(CtrGuestCopyApi) || binding->memory.version!=1 || !binding->memory.context || !binding->memory.write || (binding->neo_mode!=0 && binding->neo_mode!=1)))throw std::runtime_error("Gnm init transport binding rejected");
    std::lock_guard<std::mutex> guard(gnm_init_binding_lock);gnm_init_binding=std::move(binding);
}
inline std::uint32_t dispatch_gnm_init350(std::uint64_t destination,std::uint32_t capacity,bool* real=nullptr) {
    std::shared_ptr<const GnmInit350Binding> binding;
    {std::lock_guard<std::mutex> guard(gnm_init_binding_lock);binding=gnm_init_binding;}
    if(!binding)throw std::runtime_error("Gnm init350 real adapter missing");
    std::uint32_t written=0;
    if(binding->invoke(&binding->memory,destination,capacity,binding->neo_mode,&written))throw std::runtime_error("Gnm init350 adapter transport rejected");
    if(written>capacity)throw std::runtime_error("Gnm init350 result exceeds command capacity");
    if(real)*real=binding->real_implementation;
    return written;
}
extern "C" std::uint32_t ctr_gnm_init350(std::uint64_t destination,std::uint32_t capacity,std::uint64_t caller) {
    try{bool real=false;const auto result=dispatch_gnm_init350(destination,capacity,&real);
        char line[256];const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_Gnm_init350_return\",\"caller\":%llu,\"capacity_dwords\":%u,\"returned_dwords\":%u,\"real_implementation\":true}\n",static_cast<unsigned long long>(caller),capacity,result);
        DWORD written=0;if(real && length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);return result;}catch(const std::exception& error){std::fprintf(stderr,"NID=yb2cRhagD1I library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_init350();
inline int real_gnm_init_transport(const CtrGuestCopyApi* memory,std::uint64_t address,std::uint32_t capacity,int neo,std::uint32_t* written) {
    if(!memory || !memory->context)return 60;
    auto transaction=static_cast<DirectMemoryBacking*>(memory->context)->copy_transaction();
    return ctr_gnm_draw_init_350(memory,address,capacity,neo,written);
}
inline void install_real_gnm_init350(const std::shared_ptr<DirectMemoryBacking>& memory,int neo) {
    if(!memory)throw std::runtime_error("Gnm init direct mapping authority missing");
    auto binding=std::make_shared<GnmInit350Binding>();binding->invoke=real_gnm_init_transport;binding->memory=make_direct_copy_api(*memory);binding->owner=memory;binding->neo_mode=neo;binding->real_implementation=true;publish_gnm_init350(std::move(binding));
}
// Explicit synthetic transport fixture: these words are NOT PM4/Gnm packets.
inline int synthetic_gnm_transport(const CtrGuestCopyApi* memory,std::uint64_t address,std::uint32_t capacity,int neo,std::uint32_t* written) {
    if(neo!=0)return 1;
    if(capacity<3){*written=0;return 0;}
    const std::uint32_t fixture[]={0x12345678,0xabcdef01,0x31415926};
    if(memory->write(memory->context,address,fixture,sizeof(fixture)))return 1;
    *written=3;return 0;
}
struct SyntheticGnmTransportGate {std::promise<void> entered;std::shared_future<void> resume;};
inline SyntheticGnmTransportGate* synthetic_gnm_gate=nullptr;
inline int synthetic_blocking_gnm_transport(const CtrGuestCopyApi* memory,std::uint64_t address,std::uint32_t capacity,int neo,std::uint32_t* written) {
    auto* gate=synthetic_gnm_gate;if(!gate)return 1;gate->entered.set_value();gate->resume.wait();return synthetic_gnm_transport(memory,address,capacity,neo,written);
}
inline int gnm_init_transport_control(){
    unsigned rejected=0;try{dispatch_gnm_init350(1,3);}catch(const std::runtime_error&){++rejected;}
    auto memory=std::make_shared<DirectMemoryBacking>(65536);std::int64_t physical=-1;void* mapped=nullptr;
    if(memory->allocate(0,65536,16384,0,0,&physical) || memory->map(&mapped,16384,51,0,physical,0))return 1;
    auto binding=std::make_shared<GnmInit350Binding>();binding->invoke=synthetic_gnm_transport;binding->memory=make_direct_copy_api(*memory);binding->owner=memory;binding->neo_mode=0;publish_gnm_init350(binding);
    const auto address=reinterpret_cast<std::uint64_t>(mapped);
    if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_gnm_init350),address,3)!=3)return 1;
    std::uint32_t output[3]{};if(binding->memory.read(binding->memory.context,address,output,sizeof(output)) || output[0]!=0x12345678 || output[2]!=0x31415926)return 1;
    if(dispatch_gnm_init350(address,2)!=0)return 1;
    DWORD old=0;if(!VirtualProtect(mapped,16384,PAGE_READONLY,&old))return 1;
    try{dispatch_gnm_init350(address,3);}catch(const std::runtime_error&){++rejected;}
    if(!VirtualProtect(mapped,16384,PAGE_READWRITE,&old))return 1;
    publish_gnm_init350({});try{dispatch_gnm_init350(address,3);}catch(const std::runtime_error&){++rejected;}
    if(rejected!=3)return 1;
    SyntheticGnmTransportGate gate;std::promise<void> resume;gate.resume=resume.get_future().share();synthetic_gnm_gate=&gate;
    auto in_flight=std::make_shared<GnmInit350Binding>(*binding);in_flight->invoke=synthetic_blocking_gnm_transport;
    publish_gnm_init350(in_flight);auto entered=gate.entered.get_future();
    std::weak_ptr<DirectMemoryBacking> retained=memory;
    auto worker=std::async(std::launch::async,[address]{return dispatch_gnm_init350(address,3);});
    if(entered.wait_for(std::chrono::seconds(2))!=std::future_status::ready)ExitProcess(5);
    publish_gnm_init350({});in_flight.reset();binding.reset();memory.reset();
    if(retained.expired())ExitProcess(5);
    resume.set_value();if(worker.get()!=3 || !retained.expired())return 1;synthetic_gnm_gate=nullptr;

    std::cout<<"{\"scope\":\"synthetic_Gnm_init_transport_only\",\"SysV_integer_widths\":true,\"copied_fixture_dwords\":3,\"short_capacity_return\":0,\"explicit_rejections\":3,\"in_flight_owner_retained_until_return\":true,\"actual_Gnm_implementation\":false,\"guest_binding_installed\":false}\n";return 0;
}
}
