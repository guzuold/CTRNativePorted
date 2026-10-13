#include "ctr/mspace_free.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
void free_mspace_top(std::span<std::byte> backing,
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
    const auto next=chunk+size,top_size=get64(h+0x10);
    extent(next,top_size);
    if (c.base_token+chunk<get64(h+0x18) || (marker&3)!=3 || get32(h) || get32(h+4) || get64(h+8) ||
        get64(h+0x20) || (flags&0x10) || get64(h+0x390) || (get32(h+0x398)&3)==1 ||
        get64(h+0x28)!=c.base_token+next || (get64(next+8)&3)!=1 ||
        (get64(next+8)&0xff00fffffffffff8ull)!=top_size || get64(h+0x360)<size)
        throw std::invalid_argument("Non-top free, coalescing bins, threshold or backing release not reconstructed");
    extent(chunk,size+top_size);
    put(h+0x360,get64(h+0x360)-size);
    put(h+0x10,size+top_size);put(h+0x28,c.base_token+chunk);put(chunk+8,(size+top_size)|1);
    lock.release();
}
}
