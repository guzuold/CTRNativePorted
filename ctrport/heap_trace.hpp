// SPDX-License-Identifier: GPL-2.0-only
// New owned storage; public/retail evidence: heap_trace_storage_contract.json.
#pragma once
#include "original_call.hpp"
#include <array>
namespace ctrport {
struct HeapTraceInfo {
    std::uint64_t size;
    std::uint32_t flags,segment_info;
    std::uint64_t* mask;
    std::uint64_t* states;
};
static_assert(sizeof(HeapTraceInfo)==32);
class HeapTraceStorage {
    alignas(8) std::uint64_t mask=0;
    std::array<std::uint64_t,65> states{};
public:
    void query(void* output) {
        HeapTraceInfo info{};SIZE_T got=0;
        if(!output || !ReadProcessMemory(GetCurrentProcess(),output,&info,sizeof(info),&got) || got!=sizeof(info) || info.size!=sizeof(info)) throw std::runtime_error("Heap trace output layout unreadable or unsupported");
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(output,&region,sizeof(region)) || region.State!=MEM_COMMIT || (region.Protect!=PAGE_READWRITE && region.Protect!=PAGE_EXECUTE_READWRITE) || reinterpret_cast<std::uintptr_t>(output)-reinterpret_cast<std::uintptr_t>(region.BaseAddress)>region.RegionSize-sizeof(info)) throw std::runtime_error("Heap trace output not writable");
        info.segment_info=0;info.mask=&mask;info.states=states.data();
        std::memcpy(output,&info,sizeof(info));
    }
};
inline HeapTraceStorage heap_trace;
inline int heap_trace_control() {
    HeapTraceStorage owner;HeapTraceInfo first{32,0x1234,99,nullptr,nullptr},second{32,0,0,nullptr,nullptr};
    owner.query(&first);owner.query(&second);
    if(first.flags!=0x1234 || first.segment_info!=0 || first.mask!=second.mask || first.states!=second.states || *first.mask) throw std::runtime_error("Heap trace stable storage failed");
    for(unsigned i=0;i<65;++i) if(first.states[i]) throw std::runtime_error("Heap trace initial state failed");
    // Synthetic publication tests ownership only; no guest allocation credit.
    InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(first.mask),LONG64(1ULL<<63),0);
    first.states[64]=0x12345678;
    owner.query(&second);
    if(*second.mask!=(1ULL<<63) || second.states[64]!=0x12345678) throw std::runtime_error("Heap trace mutation lost");
    unsigned rejected=0;
    try {owner.query(nullptr);} catch(const std::runtime_error&) {++rejected;}
    try {owner.query(reinterpret_cast<void*>(1));} catch(const std::runtime_error&) {++rejected;}
    first.size=31;try {owner.query(&first);} catch(const std::runtime_error&) {++rejected;}
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!page) throw std::runtime_error("Heap trace fixture page failed");
    HeapTraceInfo valid{32,0,0,nullptr,nullptr};std::memcpy(page,&valid,sizeof(valid));DWORD old=0;
    VirtualProtect(page,4096,PAGE_READONLY,&old);
    try {owner.query(page);} catch(const std::runtime_error&) {++rejected;}
    VirtualProtect(page+4096,4096,PAGE_NOACCESS,&old);
    try {owner.query(page+4096-16);} catch(const std::runtime_error&) {++rejected;}
    VirtualFree(page,0,MEM_RELEASE);
    if(rejected!=5) throw std::runtime_error("Heap trace negatives differ");
    std::cout<<"{\"scope\":\"synthetic_owned_heap_trace_storage\",\"stable_writable_storage\":true,\"slots\":65,\"bit63_index64_preserved\":true,\"negative_rejections\":5,\"original_allocator_publication_verified\":false}\n";return 0;
}
}
