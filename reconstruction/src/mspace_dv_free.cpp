#include "ctr/mspace_dv.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
void free_mspace_dv_adjacent(std::span<std::byte> backing,
    const MspaceTopAllocationContext& c,std::uint64_t payload) {
    if (!payload) return;
    auto extent=[&](std::uint64_t at,std::uint64_t n) {
        if (at>backing.size() || n>backing.size()-at)
            throw std::invalid_argument("Invalid free chunk extent");
    };
    if (c.base_token>std::numeric_limits<std::uint64_t>::max()-backing.size() ||
        c.header_token<c.base_token || payload<c.base_token || payload-c.base_token<16)
        throw std::invalid_argument("Invalid heap or payload address");
    const auto h=c.header_token-c.base_token,chunk=payload-c.base_token-16;
    extent(h,0x520);extent(chunk,16);
    auto get64=[&](std::uint64_t at) {extent(at,8);std::uint64_t v;std::memcpy(&v,backing.data()+at,8);return v;};
    auto get32=[&](std::uint64_t at) {extent(at,4);std::uint32_t v;std::memcpy(&v,backing.data()+at,4);return v;};
    auto put=[&](std::uint64_t at,std::uint64_t v) {std::memcpy(backing.data()+at,&v,8);};
    const auto marker=get64(chunk+8),size=marker&0xff00fffffffffff8ull;
    if (size<32) throw std::invalid_argument("Invalid allocated chunk size");
    extent(chunk,size);extent(chunk+size,16);
    if (marker&4) {
        const auto last=std::to_integer<unsigned>(backing[chunk+size-1]);
        if (last<0xe1) throw std::invalid_argument("Original free padding validation failed");
        const auto count=last^255u;
        for (unsigned i=0;i<count;++i)
            if (backing[chunk+size-2-i]!=std::byte{0xaf})
                throw std::invalid_argument("Original free padding validation failed");
    }
    if ((marker&(1ull<<55)) || (get32(h+0x3f0)&1) ||
        (get64(chunk+size)^(c.poison?0x58585858ull:0ull))!=c.header_token)
        throw std::invalid_argument("Tracked chunk or invalid free owner");
    const auto flags=get32(h+0x370);
    struct Lock {
        NativeMutex* value{};
        ~Lock() {if (value) value->unlock();}
        void release() {auto* p=value;value=nullptr;if (p&&p->unlock()) throw std::runtime_error("Free mutex unlock failed");}
    } lock;
    if (flags&2) {
        if (!c.mutex || get64(h+0x378)!=reinterpret_cast<std::uint64_t>(&c.mutex->prefix))
            throw std::invalid_argument("Missing actual free mutex");
        if (c.mutex->lock()) throw std::runtime_error("Actual free mutex lock failed");
        lock.value=c.mutex;
    }
    const auto next=chunk+size,dv_token=get64(h+0x20),dv_size=get64(h+8);
    if (!(marker&2) || get32(h) || get32(h+4) || !dv_size || (dv_size&31) ||
        (flags&0x10) || get64(h+0x390) || (get32(h+0x398)&3)==1 || get64(h+0x360)<size)
        throw std::invalid_argument("Unsupported dv free context");
    std::uint64_t merged_chunk=chunk,combined=size+dv_size;
    const bool forward=(marker&1)&&dv_token==c.base_token+next;
    const bool previous=!(marker&1)&&dv_size<=chunk&&dv_token==c.base_token+chunk-dv_size;
    if (!forward&&!previous) throw std::invalid_argument("Non-adjacent dv free not reconstructed");
    if (previous) {
        if (get64(chunk)!=dv_size) throw std::invalid_argument("Invalid previous dv footer");
        merged_chunk=chunk-dv_size;
    }
    const auto dv=dv_token-c.base_token;extent(dv,dv_size);extent(dv+dv_size,16);
    if ((get64(dv+8)&0xff00fffffffffff8ull)!=dv_size || (get64(dv+8)&3)!=1)
        throw std::invalid_argument("Invalid dv free neighbor");
    extent(merged_chunk,combined);extent(merged_chunk+combined,16);
    const bool top_merge=previous&&get64(h+0x28)==c.base_token+next;
    if (top_merge) {
        const auto top_size=get64(h+0x10);extent(next,top_size);
        if ((get64(next+8)&3)!=1 || (get64(next+8)&0xff00fffffffffff8ull)!=top_size)
            throw std::invalid_argument("Invalid dv/top merge boundary");
        put(h+0x360,get64(h+0x360)-size);put(h+8,0);put(h+0x20,0);
        put(h+0x10,combined+top_size);put(h+0x28,c.base_token+merged_chunk);put(merged_chunk+8,(combined+top_size)|1);
    } else {
        const auto after=merged_chunk+combined;
        if (!(get64(after+8)&2) || (previous&&!(get64(next+8)&1)))
            throw std::invalid_argument("Dv free/bin coalescing not reconstructed");
        put(h+0x360,get64(h+0x360)-size);
        if (previous) {put(chunk+8,marker&~2ull);backing[next+8]&=std::byte{0xfe};}
        put(h+8,combined);put(h+0x20,c.base_token+merged_chunk);put(merged_chunk+8,combined|1);put(after,combined);
    }
    lock.release();
}
}
