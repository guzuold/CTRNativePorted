// SPDX-License-Identifier: GPL-2.0-only
// New Windows backing owner. Selected retail standard profile from public
// shadPS4 memory.h/linker.cpp; provenance in direct_memory_size_contract.json
// and measured retail_memory_profile.json. No PS4 allocation/map API yet.
#pragma once
#include "flat_loader.hpp"
#include "retail_profiles.hpp"
#include "kernel_time.hpp"
#include "gpu_hooks.hpp"
#include <array>
#include <thread>
#include <algorithm>
namespace ctrport {
struct DirectQueryInfo {std::uint64_t start,end;std::int32_t memory_type;};
static_assert(sizeof(DirectQueryInfo)==24);

struct GuestVirtualQueryInfo {uint64_t start,end,offset;int32_t protection,memory_type;uint8_t flags;char name[32];uint8_t padding[7];};
static_assert(sizeof(GuestVirtualQueryInfo)==72);
// param.sfo ATTRIBUTE of both pinned retail versions (v01.00 and the v01.21 patch):
// 0x20800020, measured from sce_sys/param.sfo. Bit 23 declares PS4 Pro (Neo) support.
inline constexpr std::uint32_t retail_psf_attributes=545259552U;
// Direct memory per hardware profile, as shadPS4 v0.18.0 memory.h and
// MemoryManager::SetupMemoryRegions compute it for CTR: total 5248 MiB (standard) or
// 5888 MiB (Neo) minus 512 MiB flexible memory. The process parameters give no
// flexible size, and SDK 0x6008001 >= 5.00 keeps both extended memory areas.
inline constexpr std::uint64_t standard_direct_bytes=4736ULL*1024*1024;
inline constexpr std::uint64_t neo_direct_bytes=5376ULL*1024*1024;
inline constexpr std::uint64_t native_4k_direct_bytes=6336ULL*1024*1024;
// PS4 Pro profile, opt-in with CTR_NEO_MODE=1 (experimental): like shadPS4's
// sceKernelIsNeoMode, selected only when the title declares Neo support.
inline bool neo_mode_selected() {
    static const bool requested=[]{char v[4]{};return GetEnvironmentVariableA("CTR_NEO_MODE",v,sizeof(v))==1 && v[0]=='1';}();
    return requested && ((retail_psf_attributes>>23)&1U);
}
// Native 4K 3D scene profile, opt-in with CTR_NATIVE_4K=1 (requires CTR_NEO_MODE=1):
// expands direct memory by 960 MiB so Gfx/@vramBSize (Garlic pool) can double from
// 960 MiB (0x3c000000) to 1920 MiB (0x78000000) for 3840x2160 3D render targets.
inline bool native_4k_selected() {
    static const bool requested=[]{char v[4]{};return GetEnvironmentVariableA("CTR_NATIVE_4K",v,sizeof(v))==1 && v[0]=='1';}();
    return requested && neo_mode_selected();
}
inline std::uint64_t selected_direct_bytes() {
    return native_4k_selected()?native_4k_direct_bytes:(neo_mode_selected()?neo_direct_bytes:standard_direct_bytes);
}
class DirectMemoryBacking {
    MappedImage region;
    std::uint64_t capacity;
    unsigned char* backing_address=nullptr;
    struct Allocation {std::uint64_t end;std::int32_t memory_type;};
    std::map<std::uint64_t,Allocation> allocations;
    mutable std::recursive_mutex ledger_lock;
    std::map<std::uint64_t,std::uint64_t> mappings;

public:
    explicit DirectMemoryBacking(std::uint64_t bytes,std::uint64_t alignment=65536):capacity(bytes) {
        if(!bytes || bytes%16384 || bytes>native_4k_direct_bytes) throw std::runtime_error("Direct backing size unsupported");
        if(alignment<65536 || alignment>2097152 || (alignment&(alignment-1))) throw std::runtime_error("Direct backing alignment unsupported");
        // Retail libc mspace rejects backing ends above 0xfbffffffff.
        // Reserve only free windows; MEM_RESERVE never replaces an existing mapping.
        const auto reservation_size=bytes+alignment-65536;
        constexpr std::uint64_t guest_end_limit=0xfbffffffffULL;
        for(std::uint64_t candidate=0x1000000000ULL;
            candidate<=guest_end_limit-reservation_size;candidate+=0x200000000ULL) {
            region.reset(static_cast<unsigned char*>(VirtualAlloc(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(candidate)),
                static_cast<SIZE_T>(reservation_size),MEM_RESERVE,PAGE_NOACCESS)));
            if(region) break;
        }
        if(!region) throw std::runtime_error("Direct backing reservation failed");
        const auto raw=reinterpret_cast<std::uintptr_t>(region.get());
        backing_address=reinterpret_cast<unsigned char*>((raw+alignment-1)&~(alignment-1));
    }
    ~DirectMemoryBacking(){revoke_owned_output(this);}
    int allocate(std::int64_t start,std::int64_t end,std::uint64_t bytes,std::uint64_t alignment,int type,std::int64_t* output) {
        constexpr int invalid=static_cast<int>(0x80020016U),again=static_cast<int>(0x80020023U);
        if(start<0 || end<0 || !bytes || bytes%16384 || (alignment && alignment%16384) || type<0 || type>10 || !output) return invalid;
        if(end<=start || static_cast<std::uint64_t>(end-start)<bytes) return again;
        require_time_output(output,8);
        const auto limit=(std::min)(capacity,static_cast<std::uint64_t>(end));
        const auto align=alignment?alignment:16384;
        auto candidate=static_cast<std::uint64_t>(start);
        auto round=[&](std::uint64_t value) {const auto rem=value%align;return rem?value+(align-rem):value;};
        if(candidate>limit || align>capacity) return again;
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        candidate=round(candidate);
        for(const auto& item:allocations) {
            if(candidate<=item.first && bytes<=item.first-candidate) break;
            if(candidate<item.second.end) candidate=round(item.second.end);
        }
        if(candidate>limit || bytes>limit-candidate) return again;
        allocations.emplace(candidate,Allocation{candidate+bytes,type});
        *output=static_cast<std::int64_t>(candidate);return 0;
    }
    int query(std::uint64_t offset,int flags,DirectQueryInfo* output,std::uint64_t info_size) const {
        if(flags!=0 && flags!=1) throw std::runtime_error("Direct query flags unsupported");
        if(info_size!=sizeof(DirectQueryInfo)) throw std::runtime_error("Direct query output size unsupported");
        if(offset>=capacity) return static_cast<int>(0x8002000dU);
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        auto at=allocations.upper_bound(offset);
        if(at!=allocations.begin()) {auto previous=std::prev(at);if(offset<previous->second.end) at=previous;}
        if(at==allocations.end() || (flags==0 && at->first>offset)) return static_cast<int>(0x8002000dU);
        if(!output) throw std::runtime_error("Direct query output unavailable");
        require_time_output(output,sizeof(*output));
        *output=DirectQueryInfo{at->first,at->second.end,at->second.memory_type};return 0;
    }
    int map(void** output,std::uint64_t bytes,int protection,int flags,std::int64_t physical,std::uint64_t alignment) {
        if(!output || !bytes || bytes%16384 || physical<0 || physical%16384) return static_cast<int>(0x80020016U);
        require_time_output(output,sizeof(*output));
        if(flags!=0 || (protection!=3 && protection!=51) || *output) throw std::runtime_error("Direct mapping flags/protection/address unsupported");
        const auto offset=static_cast<std::uint64_t>(physical);
        if(offset>capacity || bytes>capacity-offset) return static_cast<int>(0x80020016U);
        if(alignment && (alignment%16384 || reinterpret_cast<std::uintptr_t>(backing_address+offset)%alignment)) return static_cast<int>(0x80020016U);
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        auto allocation=allocations.upper_bound(offset);
        if(allocation==allocations.begin()) throw std::runtime_error("Direct map has no owned allocation");
        --allocation;
        if(offset<allocation->first || offset>=allocation->second.end || bytes>allocation->second.end-offset) throw std::runtime_error("Direct map exceeds owned allocation");
        for(const auto& item:mappings) if(offset<item.second && item.first<offset+bytes) throw std::runtime_error("Direct mapping alias unsupported");
        if(VirtualAlloc(backing_address+offset,static_cast<SIZE_T>(bytes),MEM_COMMIT,PAGE_READWRITE)!=backing_address+offset) return static_cast<int>(0x8002000cU);
        publish_owned_output(this,backing_address+offset,static_cast<std::size_t>(bytes));
        mappings.emplace(offset,offset+bytes);*output=backing_address+offset;
        if(direct_memory_map_hook)direct_memory_map_hook(reinterpret_cast<std::uint64_t>(backing_address+offset),bytes);
        return 0;
    }
    bool virtual_query(uint64_t guest,GuestVirtualQueryInfo& info) const {
        const auto base=reinterpret_cast<uint64_t>(backing_address);
        if(guest<base || guest-base>=capacity)return false;
        std::lock_guard<std::recursive_mutex> lock(ledger_lock);
        const auto offset=guest-base;auto map=mappings.upper_bound(offset);
        if(map==mappings.begin())return false;--map;
        if(offset>=map->second)return false;
        auto allocation=allocations.upper_bound(offset);
        if(allocation==allocations.begin())throw std::runtime_error("VirtualQuery no physical allocation");--allocation;
        if(offset>=allocation->second.end)throw std::runtime_error("VirtualQuery allocation gap");
        info={};info.start=base+map->first;info.end=base+map->second;
        info.offset=map->first;info.memory_type=allocation->second.memory_type;info.flags=18;return true;
    }
    std::unique_lock<std::recursive_mutex> copy_transaction(){return std::unique_lock<std::recursive_mutex>(ledger_lock);}
    // Authority comes exclusively from this owner's mapping ledger. Host page
    // queries may restrict that extent, never manufacture mapping authority.
    std::uint64_t permitted_mapped_extent(std::uint64_t guest,std::uint64_t limit,bool writing,std::uint64_t maximum_bytes=16*1024*1024) const {
        if(!limit || maximum_bytes>32*1024*1024 || limit>maximum_bytes)return 0;
        const auto base=reinterpret_cast<std::uint64_t>(backing_address);
        if(guest<base || guest-base>=capacity)return 0;
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        const auto offset=guest-base;auto found=mappings.upper_bound(offset);
        if(found==mappings.begin())return 0;--found;
        if(offset<found->first || offset>=found->second)return 0;
        const auto bytes=(std::min)(limit,found->second-offset);auto cursor=guest;
        while(cursor<guest+bytes){MEMORY_BASIC_INFORMATION info{};
            if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))break;
            const auto protection=info.Protect&0xff;
            const bool writable=protection==PAGE_READWRITE || protection==PAGE_EXECUTE_READWRITE;
            const bool readable=writable || protection==PAGE_READONLY || protection==PAGE_EXECUTE_READ;
            if(writing?!writable:!readable)break;
            const auto end=reinterpret_cast<std::uint64_t>(info.BaseAddress)+info.RegionSize;
            if(end<=cursor)break;cursor=(std::min)(end,guest+bytes);
        }
        return cursor-guest;
    }
    std::uint64_t readable_mapped_extent(std::uint64_t guest,std::uint64_t limit,std::uint64_t maximum_bytes=16*1024*1024) const {return permitted_mapped_extent(guest,limit,false,maximum_bytes);}
    // Permission observation only; never a guest-writer/GPU completion contract.
    std::uint64_t writable_mapped_extent(std::uint64_t guest,std::uint64_t limit) const {return permitted_mapped_extent(guest,limit,true);}
    // Mapping authority only. Does not read pixels or promise GPU completion.
    bool contains_mapped_range(std::uint64_t guest,std::uint64_t bytes) const {
        const auto base=reinterpret_cast<std::uint64_t>(backing_address);
        if(!bytes || guest<base || guest-base>=capacity || bytes>capacity-(guest-base))return false;
        const auto offset=guest-base;
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        auto found=mappings.upper_bound(offset);if(found==mappings.begin())return false;--found;
        if(offset<found->first || offset>=found->second || bytes>found->second-offset)return false;
        for(auto cursor=guest;cursor<guest+bytes;) {
            MEMORY_BASIC_INFORMATION info{};
            if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Protect!=PAGE_READWRITE)return false;
            const auto end=reinterpret_cast<std::uint64_t>(info.BaseAddress)+info.RegionSize;
            if(end<=cursor)return false;cursor=(std::min)(end,guest+bytes);
        }
        return true;
    }
    void copy_mapped(std::uint64_t guest,void* host,std::size_t bytes,bool writing,std::size_t maximum_bytes=16*1024*1024) {
        if(!host || !bytes || maximum_bytes>32*1024*1024 || bytes>maximum_bytes)throw std::runtime_error("Direct copy host/size rejected");
        const auto base=reinterpret_cast<std::uint64_t>(backing_address);
        if(guest<base || guest-base>=capacity || bytes>capacity-(guest-base))throw std::runtime_error("Direct copy outside backing");
        const auto offset=guest-base;
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        auto found=mappings.upper_bound(offset);if(found==mappings.begin())throw std::runtime_error("Direct copy has no mapped allocation");--found;
        if(offset<found->first || offset>=found->second || bytes>found->second-offset)throw std::runtime_error("Direct copy exceeds mapped allocation");
        const auto trusted=reinterpret_cast<std::uint64_t>(host);
        if(trusted>UINT64_MAX-bytes || (trusted<guest+bytes && guest<trusted+bytes))throw std::runtime_error("Direct copy host overlaps guest");
        for(auto cursor=guest;cursor<guest+bytes;) {
            MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&PAGE_GUARD))throw std::runtime_error("Direct copy page inaccessible");
            const auto protection=info.Protect&0xffU;
            const bool writable=protection==PAGE_READWRITE || protection==PAGE_EXECUTE_READWRITE;
            const bool readable=writable || protection==PAGE_READONLY || protection==PAGE_EXECUTE_READ;
            if(writing?!writable:!readable)throw std::runtime_error("Direct copy page permission rejected");
            const auto end=reinterpret_cast<std::uint64_t>(info.BaseAddress)+info.RegionSize;if(end<=cursor)throw std::runtime_error("Direct copy query overflow");cursor=(std::min)(end,guest+bytes);
        }
        if(writing)std::memcpy(reinterpret_cast<void*>(guest),host,bytes);
        else {std::vector<unsigned char> staged(bytes);SIZE_T copied=0;if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(guest),staged.data(),bytes,&copied) || copied!=bytes)throw std::runtime_error("Direct copy read failed");std::memcpy(host,staged.data(),bytes);}
    }
    // Reached write-back type3 / CPU-write GPU-RW domain. Windows x64 has no
    // write-only page mode, matching the public reference's RW host mapping.
    // Renderer invalidation/permission publication belongs to the caller.
    int type_protect_writeback(std::uint64_t guest,std::uint64_t bytes,unsigned protection,int type) {
        // With the GPU library every type/protection pair is accepted: host access is the
        // union of CPU and GPU permissions (shadPS4 memory.cpp ProtectBytes, 1049-1072),
        // because the renderer reads and writes GPU-only memory from the host.
        if(gpu_library_present){
            if(type<0 || type>10 || (protection&~0x37u))return static_cast<int>(0x80020016u);
            if(!bytes || (guest&16383) || (bytes&16383))return static_cast<int>(0x80020016u);
            std::lock_guard<std::recursive_mutex> guard(ledger_lock);
            const auto base=reinterpret_cast<std::uint64_t>(backing_address);
            if(guest<base || guest-base>=capacity || bytes>capacity-(guest-base))throw std::runtime_error("TypeProtect outside direct memory");
            // The range may span several adjacent allocations: retype each covered piece.
            auto updated=allocations;
            for(auto cursor=guest-base;cursor<guest-base+bytes;){
                auto allocation=updated.upper_bound(cursor);if(allocation==updated.begin())throw std::runtime_error("TypeProtect no allocation");--allocation;
                if(cursor>=allocation->second.end)throw std::runtime_error("TypeProtect range has an unallocated gap");
                const auto first=allocation->first;const auto old=allocation->second;
                const auto piece_end=(std::min)(old.end,guest-base+bytes);
                updated.erase(first);
                if(first<cursor)updated.emplace(first,Allocation{cursor,old.memory_type});
                updated.emplace(cursor,Allocation{piece_end,type});
                if(piece_end<old.end)updated.emplace(piece_end,Allocation{old.end,old.memory_type});
                cursor=piece_end;
            }
            const DWORD host=(protection&0x22u)?PAGE_READWRITE:(protection&0x11u)?PAGE_READONLY:PAGE_NOACCESS;
            DWORD previous=0;
            if(!VirtualProtect(reinterpret_cast<void*>(guest),static_cast<SIZE_T>(bytes),host,&previous))return static_cast<int>(0x8002000cu);
            if(guest_cpu_write_hook)guest_cpu_write_hook(reinterpret_cast<const void*>(guest),bytes);
            allocations.swap(updated);return 0;
        }
        if(type!=3 || (protection!=50 && protection!=51 && protection!=3))throw std::runtime_error("TypeProtect unobserved type/protection");
        if(!bytes || (guest&16383) || (bytes&16383))return static_cast<int>(0x80020016u);
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        if(!contains_mapped_range(guest,bytes))throw std::runtime_error("TypeProtect outside live RW mapping");
        const auto offset=guest-reinterpret_cast<std::uint64_t>(backing_address);
        auto allocation=allocations.upper_bound(offset);if(allocation==allocations.begin())throw std::runtime_error("TypeProtect no allocation");--allocation;
        if(offset>=allocation->second.end || bytes>allocation->second.end-offset)throw std::runtime_error("TypeProtect spanning allocations unsupported");
        auto updated=allocations;const auto first=allocation->first;const auto old=allocation->second;
        updated.erase(first);
        if(first<offset)updated.emplace(first,Allocation{offset,old.memory_type});
        updated.emplace(offset,Allocation{offset+bytes,type});
        if(offset+bytes<old.end)updated.emplace(offset+bytes,Allocation{old.end,old.memory_type});
        DWORD previous=0;
        if(!VirtualProtect(reinterpret_cast<void*>(guest),static_cast<SIZE_T>(bytes),PAGE_READWRITE,&previous))return static_cast<int>(0x8002000cu);
        if(guest_cpu_write_hook)guest_cpu_write_hook(reinterpret_cast<const void*>(guest),bytes);
        allocations.swap(updated);return 0;
    }
    void replay_mappings(){
        std::lock_guard<std::recursive_mutex> guard(ledger_lock);
        if(direct_memory_map_hook)for(const auto& [first,end]:mappings)direct_memory_map_hook(reinterpret_cast<std::uint64_t>(backing_address+first),end-first);
    }
    std::uint64_t size() const {return capacity;}
    unsigned char* address() const {return backing_address;}
    void commit(std::uint64_t offset,std::uint64_t bytes) {
        if(!bytes || offset%16384 || bytes%16384 || offset>capacity || bytes>capacity-offset) throw std::runtime_error("Direct commit range unsupported");
        if(VirtualAlloc(backing_address+offset,static_cast<SIZE_T>(bytes),MEM_COMMIT,PAGE_READWRITE)!=backing_address+offset) throw std::runtime_error("Direct backing commit failed");
        publish_owned_output(this,backing_address+offset,static_cast<std::size_t>(bytes));
    }
};
inline std::shared_ptr<DirectMemoryBacking> direct_memory_backing;
inline bool cpu_mode_retail_base=false;
inline int cpu_mode_from_attributes(std::uint32_t attribute) {
    const bool six=(attribute&(1U<<15))!=0,seven=(attribute&(1U<<16))!=0;
    return six && seven?2:seven?5:0;
}
inline int process_cpu_mode() {
    if(!cpu_mode_retail_base) throw std::runtime_error("Measured retail CPU mode profile unavailable");
    return cpu_mode_from_attributes(retail_psf_attributes);
}
inline void configure_direct_memory(const MappedImage& image,const ImageMetadata& metadata) {
    const auto* profile=retail_profile(metadata.sha256);
    if(!profile) throw std::runtime_error("Direct memory profile SHA unsupported");
    auto read=[&](std::uint64_t offset) {
        bool backed=false;
        for(const auto& span:metadata.spans) if(offset>=span.first && offset<=span.second && 8<=span.second-offset) backed=true;
        if(!backed) throw std::runtime_error("Direct memory process parameter outside mapped span");
        std::uint64_t value=0;std::memcpy(&value,image.get()+offset,8);return value;
    };
    const auto proc=profile->proc;
    const auto mem=profile->mem;
    if(read(proc)!=88 || read(proc+16)!=0x6008001ULL || read(proc+64)!=reinterpret_cast<std::uint64_t>(image.get())+mem || read(mem)!=56) throw std::runtime_error("Direct memory process profile differs");
    for(unsigned i=1;i<7;++i) if(read(mem+i*8)!=0) throw std::runtime_error("Direct memory optional parameter unsupported");
    if(direct_memory_backing) throw std::runtime_error("Direct memory already configured");
    direct_memory_backing=std::make_shared<DirectMemoryBacking>(selected_direct_bytes(),2097152);
    // Same ATTRIBUTE in both EU versions' param.sfo, so the measured CPU mode holds for both;
    // the US param.sfo ATTRIBUTE has not been read yet.
    cpu_mode_retail_base=true;
}
inline int process_neo_mode() {
    const bool neo=neo_mode_selected();
    if(!direct_memory_backing || direct_memory_backing->size()!=selected_direct_bytes()) throw std::runtime_error("Validated memory profile unavailable");
    return neo?1:0; // Selected hardware profile; not a service-success return.
}
inline std::uint64_t direct_memory_size() {
    if(!direct_memory_backing) throw std::runtime_error("Direct memory backing unavailable");
    return direct_memory_backing->size();
}
inline int owned_output_control(){
    std::uintptr_t released=0;unsigned rejected=0;
    {
        DirectMemoryBacking backing(65536);backing.commit(0,16384);
        auto* start=backing.address();released=reinterpret_cast<std::uintptr_t>(start);
        const auto queries=output_query_observations.load();
        require_time_output(start,8);require_time_output(start+16376,8);
        if(output_query_observations.load()!=queries)throw std::runtime_error("Owned outputs still query host");
        for(auto test:{std::pair<const void*,std::size_t>{start+16380,8},{start+16384,8},{reinterpret_cast<void*>(~std::uintptr_t(0)-3),8}}){
            try{require_time_output(test.first,test.second);}catch(const std::runtime_error&){++rejected;}
        }
        if(is_owned_output(released+16380,released+16388) || is_owned_output(released+16384,released+16392))throw std::runtime_error("Owned extent exceeds committed range");
    }
    if(is_owned_output(released,released+8))throw std::runtime_error("Owner destruction did not revoke extent");
    try{require_time_output(reinterpret_cast<void*>(released),8);}catch(const std::runtime_error&){++rejected;}
    auto* readonly=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READONLY);
    if(!readonly)throw std::runtime_error("Readonly fixture failed");
    try{require_time_output(readonly,8);}catch(const std::runtime_error&){++rejected;}
    VirtualFree(readonly,0,MEM_RELEASE);
    if(rejected!=5)throw std::runtime_error("Owned output negative controls differ");
    std::cout<<"{\"scope\":\"owned_output_extent_control\",\"valid_boundaries_no_query\":2,\"negative_rejections\":5,\"destruction_revoked\":true}\n";return 0;
}
inline int direct_map_control() {
    DirectMemoryBacking backing(65536,2097152);std::int64_t physical=-1;
    if(backing.allocate(0,65536,32768,0,0,&physical)) throw std::runtime_error("Map fixture allocation failed");
    void* mapped=nullptr;
    if(backing.map(&mapped,32768,51,0,physical,2097152) || reinterpret_cast<std::uintptr_t>(mapped)%2097152) throw std::runtime_error("Direct map failed");
    auto* data=static_cast<unsigned char*>(mapped);data[0]=0x31;data[32767]=0x72;
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(mapped,&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Protect!=PAGE_READWRITE || data[0]!=0x31 || data[32767]!=0x72) throw std::runtime_error("Map real storage failed");
    unsigned rejected=0;void* other=nullptr;
    try {backing.map(&other,16384,51,0,0,0);} catch(const std::runtime_error&) {++rejected;}
    try {backing.map(&other,16384,51,0,32768,0);} catch(const std::runtime_error&) {++rejected;}
    try {backing.map(&other,16384,51,16,0,0);} catch(const std::runtime_error&) {++rejected;}
    try {backing.map(reinterpret_cast<void**>(1),16384,51,0,0,0);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=4 || other) throw std::runtime_error("Map unsupported domain accepted");
    std::cout<<"{\"scope\":\"owned_direct_map\",\"actual_committed_rw_bytes\":32768,\"alignment\":2097152,\"negative_rejections\":4,\"gpu_renderer_supported\":false}\n";return 0;
}
inline int direct_allocation_control() {
    DirectMemoryBacking backing(65536);std::int64_t a=-1,b=-1,unchanged=123;
    if(backing.allocate(0,65536,16384,32768,0,&a) || a!=0 || backing.allocate(0,65536,16384,32768,3,&b) || b!=32768) throw std::runtime_error("Direct allocation placement failed");
    DirectQueryInfo info{};
    if(backing.query(32769,0,&info,24) || info.start!=32768 || info.end!=49152 || info.memory_type!=3) throw std::runtime_error("Direct allocation query differs");
    if(backing.allocate(0,65536,65536,0,0,&unchanged)!=static_cast<int>(0x80020023U) || unchanged!=123) throw std::runtime_error("Direct exhaustion mutated output");
    if(backing.allocate(-1,65536,16384,0,0,&unchanged)!=static_cast<int>(0x80020016U) || unchanged!=123) throw std::runtime_error("Direct invalid mutated output");
    for(auto params:{std::array<std::int64_t,5>{0,65536,0,0,0},{0,65536,1,0,0},{0,65536,16384,1,0},{0,65536,16384,0,11},{0,65536,16384,0,-1}}) {
        if(backing.allocate(params[0],params[1],params[2],params[3],static_cast<int>(params[4]),&unchanged)!=static_cast<int>(0x80020016U) || unchanged!=123) throw std::runtime_error("Direct error matrix differs");
    }
    if(backing.allocate(0,65536,16384,~16383ULL,0,&unchanged)!=static_cast<int>(0x80020023U) || unchanged!=123) throw std::runtime_error("Direct overflow alignment accepted");
    DirectMemoryBacking concurrent(65536);std::array<std::int64_t,4> outputs{};std::array<std::thread,4> threads;
    for(unsigned i=0;i<4;++i) threads[i]=std::thread([&,i]{if(concurrent.allocate(0,65536,16384,0,0,&outputs[i])) std::terminate();});
    for(auto& thread:threads) thread.join();std::sort(outputs.begin(),outputs.end());
    for(unsigned i=0;i<4;++i) if(outputs[i]!=i*16384) throw std::runtime_error("Concurrent direct overlap");
    unsigned rejected=0;try {backing.allocate(0,65536,16384,0,0,reinterpret_cast<std::int64_t*>(1));} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=1) throw std::runtime_error("Direct bad output accepted");
    std::cout<<"{\"scope\":\"owned_direct_allocation_ledger\",\"alignment_nonoverlap_query\":true,\"exhaustion_invalid_no_write\":true,\"invalid_pointer_rejected\":true,\"guest_mapping_supported\":false}\n";return 0;
}
inline int direct_memory_control() {
    constexpr auto capacity=4736ULL*1024*1024;
    DirectMemoryBacking backing(capacity);
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(backing.address(),&info,sizeof(info)) || info.State!=MEM_RESERVE || info.RegionSize!=capacity || backing.size()!=capacity) throw std::runtime_error("Direct reservation extent differs");
    backing.commit(0,16384);backing.commit(capacity-16384,16384);
    backing.address()[0]=0x35;backing.address()[capacity-1]=0x71;
    if(backing.address()[0]!=0x35 || backing.address()[capacity-1]!=0x71) throw std::runtime_error("Direct backing storage failed");
    if(!VirtualQuery(backing.address()+16384,&info,sizeof(info)) || info.State!=MEM_RESERVE) throw std::runtime_error("Direct reservation overcommitted");
    DirectQueryInfo untouched{0x1234,0x5678,9};const auto before=untouched;
    for(auto offset:{0ULL,capacity-1,capacity}) for(auto flags:{0,1}) {
        if(backing.query(offset,flags,&untouched,24)!=static_cast<int>(0x8002000dU) || std::memcmp(&untouched,&before,sizeof(before))) throw std::runtime_error("Empty direct query mutated output or returned success");
    }
    unsigned query_rejected=0;
    try {backing.query(0,2,&untouched,24);} catch(const std::runtime_error&) {++query_rejected;}
    try {backing.query(0,1,&untouched,23);} catch(const std::runtime_error&) {++query_rejected;}
    if(query_rejected!=2 || std::memcmp(&untouched,&before,sizeof(before))) throw std::runtime_error("Query invalid domain accepted/mutated output");
    unsigned rejected=0;
    for(auto range:{std::pair<std::uint64_t,std::uint64_t>{capacity,16384},{1,16384},{0,0},{capacity-16384,32768}}) {
        try {backing.commit(range.first,range.second);} catch(const std::runtime_error&) {++rejected;}
    }
    if(rejected!=4) throw std::runtime_error("Direct range negatives differ");
    std::cout<<"{\"scope\":\"host_direct_backing\",\"reserved_bytes\":"<<capacity<<",\"committed_edge_bytes\":32768,\"interior_reserved\":true,\"range_rejections\":4,\"runtime_bound\":false}\n";
    return 0;
}
// Authored permission fixture: mapped ownership plus independent Win32 protection.
inline int direct_output_permission_control(){
 DirectMemoryBacking backing(32768);std::int64_t physical=-1;void* address=nullptr;
 if(backing.allocate(0,32768,32768,16384,0,&physical) || backing.map(&address,32768,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(address);
 if(backing.readable_mapped_extent(base,32768)!=32768 || backing.writable_mapped_extent(base,32768)!=32768)return 2;
 DWORD old=0;
 if(!VirtualProtect(static_cast<unsigned char*>(address)+16384,16384,PAGE_READONLY,&old))return 3;
 if(backing.readable_mapped_extent(base,32768)!=32768 || backing.writable_mapped_extent(base,32768)!=16384 || backing.writable_mapped_extent(base+16384,16384)!=0)return 4;
 if(backing.writable_mapped_extent(base+32768,4) || backing.writable_mapped_extent(base,0) || backing.writable_mapped_extent(base,16*1024*1024+1))return 5;
 if(!VirtualProtect(address,4096,PAGE_READWRITE|PAGE_GUARD,&old))return 6;
 if(backing.readable_mapped_extent(base,32768) || backing.writable_mapped_extent(base,32768))return 7;
 if(!VirtualProtect(address,4096,PAGE_READWRITE,&old))return 8;
 std::cout<<"{\"scope\":\"direct_output_permission_control\",\"passed\":true,\"readonly_suffix\":true,\"guard_rejected\":true,\"unmapped_and_invalid_rejected\":true,\"synthetic\":true,\"GPU_output_admission\":false}\n";return 0;
}
}
