// SPDX-License-Identifier: GPL-2.0-only
// New Windows CNG implementation; public PS4 size/error contract from pinned
// shadPS4 random.h/random_error.h, hashes in random_service_contract.json.
#pragma once
#include "process_guard.hpp"
#include "kernel_time.hpp"
#include <array>
namespace ctrport {
inline int random_bytes(unsigned char* output,std::size_t size) {
    if(size>64) return static_cast<int>(0x817c0016U);
    if(!size) return 0;
    if(!output) throw std::runtime_error("Random output null unsupported");
    require_time_output(output,size);
    if(BCryptGenRandom(nullptr,output,static_cast<ULONG>(size),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return static_cast<int>(0x817c00ffU);
    return 0;
}
inline int random_service_control() {
    std::array<unsigned char,66> bytes;bytes.fill(0x5a);
    if(random_bytes(nullptr,0) || random_bytes(bytes.data()+1,64) || bytes.front()!=0x5a || bytes.back()!=0x5a) throw std::runtime_error("Random size/output extent failed");
    const auto before=bytes;
    if(random_bytes(bytes.data(),65)!=static_cast<int>(0x817c0016U) || bytes!=before) throw std::runtime_error("Oversize random mutated output");
    if(random_bytes(bytes.data()+1,1) || bytes.front()!=0x5a || bytes.back()!=0x5a) throw std::runtime_error("One byte random extent failed");
    unsigned rejected=0;
    try {random_bytes(nullptr,1);} catch(const std::runtime_error&) {++rejected;}
    try {random_bytes(reinterpret_cast<unsigned char*>(1),1);} catch(const std::runtime_error&) {++rejected;}
    MappedImage page(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
    if(!page) throw std::runtime_error("Random control page allocation failed");
    for(auto protection:{DWORD(PAGE_READONLY),DWORD(PAGE_NOACCESS)}) {
        DWORD old=0;if(!VirtualProtect(page.get(),4096,protection,&old)) throw std::runtime_error("Random control protection failed");
        try {random_bytes(page.get(),64);} catch(const std::runtime_error&) {++rejected;}
    }
    if(rejected!=4) throw std::runtime_error("Invalid random output accepted");
    std::cout<<"{\"scope\":\"host_random_service\",\"sizes_verified\":[0,1,64],\"oversize_error_preserves_output\":true,\"extent_sentinels_preserved\":true,\"invalid_outputs_rejected\":4,\"entropy_provider\":\"BCryptGenRandom\",\"runtime_bound\":false}\n";
    return 0;
}
}
