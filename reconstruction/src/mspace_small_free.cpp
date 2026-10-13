#include "ctr/mspace_small_free.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
void free_mspace_small(std::span<std::byte> backing,
    const MspaceTopAllocationContext& c,std::uint64_t payload) {
    if (!payload) return;
    auto extent=[&](std::uint64_t at,std::uint64_t n) {
        if (at>backing.size() || n>backing.size()-at)
            throw std::invalid_argument("Invalid free chunk extent");
    };
    if (c.base_token>std::numeric_limits<std::uint64_t>::max()-backing.size() ||
        c.header_token<c.base_token || payload<c.base_token || payload-c.base_token<16)
        throw std::invalid_argument("Invalid heap or payload address");
    const auto h=c.header_token-c.base_token,original_chunk=payload-c.base_token-16;
    auto chunk=original_chunk;
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
    auto put32=[&](std::uint64_t at,std::uint32_t v) {std::memcpy(backing.data()+at,&v,4);};
    auto offset=[&](std::uint64_t token) {
        if (token<c.base_token) throw std::invalid_argument("Invalid small-bin pointer");
        const auto at=token-c.base_token;extent(at,32);return at;
    };
    const auto next=chunk+size,top_size=get64(h+0x10),next_marker=get64(next+8);
    if (c.base_token+chunk<get64(h+0x18) || !(marker&2) || get32(h+4) || get64(h+8) ||
        get64(h+0x20) || (flags&0x10) || get64(h+0x390) || (get32(h+0x398)&3)==1 ||
        !(next_marker&1) || get64(h+0x360)<size)
        throw std::invalid_argument("Tree/dv, threshold, corrupt chunk or backing release not reconstructed");
    auto combined=size;
    std::uint64_t previous_size=0,forward=0,backward=0;
    std::uint32_t map=get32(h);
    if (!(marker&1)) {
        previous_size=get64(chunk);
        if (previous_size<32 || previous_size>255 || (previous_size&31) || previous_size>chunk)
            throw std::invalid_argument("Backward tree coalescing not reconstructed");
        chunk-=previous_size;extent(chunk,previous_size);
        if ((get64(chunk+8)&0xff00fffffffffff8ull)!=previous_size || (get64(chunk+8)&3)!=1 ||
            !(map&(1u<<(previous_size>>3))))
            throw std::invalid_argument("Invalid backward small-bin chunk");
        forward=get64(chunk+0x10);backward=get64(chunk+0x18);
        const auto f=offset(forward),back=offset(backward);
        if (get64(f+0x18)!=c.base_token+chunk || get64(back+0x10)!=c.base_token+chunk)
            throw std::invalid_argument("Broken small-bin links");
        combined+=previous_size;
    }
    const bool merge_top=c.base_token+next==get64(h+0x28);
    if (merge_top) {
        if ((next_marker&3)!=1 || (next_marker&0xff00fffffffffff8ull)!=top_size)
            throw std::invalid_argument("Invalid small-free top");
        extent(next,top_size);
    } else {
        if (!(next_marker&2) || combined>255)
            throw std::invalid_argument("Forward free coalescing or tree insertion not reconstructed");
        const auto sentinel=h+0x40+combined*2;extent(sentinel,32);
        auto prospective=map;
        if (previous_size && forward==backward) prospective&=~(1u<<(previous_size>>3));
        if (prospective&(1u<<(combined>>3))) {
            const auto first=offset(get64(sentinel+0x10));
            if (get64(first+0x18)!=c.base_token+sentinel)
                throw std::invalid_argument("Broken insertion bin head");
        }
    }
    // All supported-path validation precedes mutation, including link writes.
    put(h+0x360,get64(h+0x360)-size);
    if (previous_size) {
        put(original_chunk+8,marker&~2ull);
        if (forward==backward) map&=~(1u<<(previous_size>>3));
        else {put(offset(forward)+0x18,backward);put(offset(backward)+0x10,forward);}
        put32(h,map);
    }
    if (merge_top) {
        put(h+0x10,combined+top_size);put(h+0x28,c.base_token+chunk);put(chunk+8,(combined+top_size)|1);
    } else {
        put(next+8,next_marker&~1ull);put(chunk+8,combined|1);put(chunk+combined,combined);
        const auto sentinel=h+0x40+combined*2;const auto bit=1u<<(combined>>3);
        auto first=c.base_token+sentinel;
        if (map&bit) first=get64(sentinel+0x10);
        else {map|=bit;put32(h,map);}
        put(sentinel+0x10,c.base_token+chunk);put(offset(first)+0x18,c.base_token+chunk);
        put(chunk+0x10,first);put(chunk+0x18,c.base_token+sentinel);
    }
    lock.release();
}
}

