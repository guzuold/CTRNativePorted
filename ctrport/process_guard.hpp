// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
namespace ctrport {
// Our implementation of the observed stable 64-bit canary object contract.
// No upstream dummy constant or prof-build information is used.
inline const std::uint64_t* process_guard() {
    static const MappedImage storage=[] {
        MappedImage page(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,
            MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
        if(!page) throw std::runtime_error("Process guard allocation failed");
        if(BCryptGenRandom(nullptr,page.get(),8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
            throw std::runtime_error("Process guard entropy failed");
        DWORD previous=0;
        if(!VirtualProtect(page.get(),4096,PAGE_READONLY,&previous))
            throw std::runtime_error("Process guard protection failed");
        return page;
    }();
    return reinterpret_cast<const std::uint64_t*>(storage.get());
}
inline int process_guard_control() {
    const auto* first=process_guard(); const auto initial=*first;
    const auto* second=process_guard(); MEMORY_BASIC_INFORMATION info{};
    if(first!=second || initial!=*second ||
       !VirtualQuery(first,&info,sizeof(info)) || info.Protect!=PAGE_READONLY)
        throw std::runtime_error("Process guard lifetime/protection differs");
    std::cout << "{\"scope\":\"host_process_guard_contract\",\"stable_address\":true,"
                 "\"stable_value\":true,\"read_only\":true,\"entropy_api\":\"BCryptGenRandom\","
                 "\"retail_executed\":false,\"runtime_bound\":false,\"runtime_replacements\":0}\n";
    return 0;
}
}
