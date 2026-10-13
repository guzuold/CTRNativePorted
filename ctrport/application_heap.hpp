// SPDX-License-Identifier: GPL-2.0-only
// New registry; public layout/provenance in application_heap_api_contract.json.
#pragma once
#include "original_call.hpp"
#include <array>
namespace ctrport {
class ApplicationHeap {
    const void* table=nullptr;
    std::array<void*,10> read(const void* source) const {
        std::array<void*,10> values{};SIZE_T size=0;
        if(!source || !ReadProcessMemory(GetCurrentProcess(),source,values.data(),sizeof(values),&size) || size!=sizeof(values)) throw std::runtime_error("Heap callback table unreadable");
        for(std::size_t index=0;index<values.size();++index) {
            auto* callback=values[index];
            if(!callback && index>=2) continue;
            MEMORY_BASIC_INFORMATION info{};
            if(!callback || !VirtualQuery(callback,&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect!=PAGE_EXECUTE_READ && info.Protect!=PAGE_EXECUTE_READWRITE)) throw std::runtime_error("Heap callback not executable");
        }
        return values;
    }
public:
    void set(const void* source) {(void)read(source);table=source;}
    void* allocate(std::uint64_t size) const {
        auto values=read(table);
        return reinterpret_cast<void*>(ctr_call_original_integer3(values[0],size,0,0));
    }
    void release(void* value) const {auto values=read(table);(void)ctr_call_original_integer3(values[1],reinterpret_cast<std::uint64_t>(value),0,0);}
};
inline ApplicationHeap application_heap;
extern "C" void ctr_sysv_heap_malloc_control();
extern "C" void ctr_sysv_heap_free_control();
inline int application_heap_control() {
    ApplicationHeap state;unsigned rejected=0;
    try {state.allocate(16);} catch(const std::runtime_error&) {++rejected;}
    try {state.set(reinterpret_cast<void*>(1));} catch(const std::runtime_error&) {++rejected;}
    std::array<void*,10> table{};table[0]=reinterpret_cast<void*>(&ctr_sysv_heap_malloc_control);table[1]=reinterpret_cast<void*>(&ctr_sysv_heap_free_control);
    state.set(table.data());auto* p=static_cast<unsigned char*>(state.allocate(64));
    if(!p) throw std::runtime_error("Host fixture allocation failed");
    p[0]=0x42;p[63]=0x24;if(p[0]!=0x42 || p[63]!=0x24) throw std::runtime_error("Host fixture readback failed");state.release(p);
    table[0]=nullptr;
    try {state.allocate(64);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=3) throw std::runtime_error("Heap negatives differ");
    std::cout<<"{\"scope\":\"synthetic_heap_callback_registry\",\"real_host_allocate_free\":true,\"optional_null_slots_accepted\":true,\"live_table_mutation_rejected\":true,\"negative_rejections\":3,\"guest_tls_supported\":false}\n";
    return 0;
}
}
