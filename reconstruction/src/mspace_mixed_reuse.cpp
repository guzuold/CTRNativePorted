#include "ctr/mspace_small_allocate.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
MspaceAllocation allocate_mspace_exact_with_dv(std::span<std::byte> backing,
    const MspaceTopAllocationContext& c,std::uint64_t request) {
    if (c.header_token<c.base_token || c.header_token-c.base_token>backing.size() ||
        backing.size()-(c.header_token-c.base_token)<0x520 ||
        c.base_token>std::numeric_limits<std::uint64_t>::max()-backing.size() ||
        ((reinterpret_cast<std::uintptr_t>(backing.data())^c.base_token)&31))
        throw std::invalid_argument("Missing or misaligned actual mspace backing");
    const auto h=c.header_token-c.base_token;
    auto get64=[&](std::uint64_t offset) {std::uint64_t v;std::memcpy(&v,backing.data()+offset,8);return v;};
    auto get32=[&](std::uint64_t offset) {std::uint32_t v;std::memcpy(&v,backing.data()+offset,4);return v;};
    auto put=[&](std::uint64_t offset,std::uint64_t v) {std::memcpy(backing.data()+offset,&v,8);};
    const auto poison=c.poison?0x58585858ull:0ull;
    if (get64(h+0x38)!=poison) return {};
    const auto flags=get32(h+0x370);
    struct Lock {
        NativeMutex* value{};
        ~Lock() {if (value) value->unlock();}
        void release() {auto* p=value;value=nullptr;if (p&&p->unlock()) throw std::runtime_error("Actual mspace mutex unlock failed");}
    } lock;
    if (flags&2) {
        if (!c.mutex || get64(h+0x378)!=reinterpret_cast<std::uint64_t>(&c.mutex->prefix))
            throw std::invalid_argument("Missing actual mspace mutex");
        if (c.mutex->lock()) throw std::runtime_error("Actual mspace mutex lock failed");
        lock.value=c.mutex;
    }
    if (get32(h+4) || (flags&0x10) || (get32(h+0x3f0)&1))
        throw std::invalid_argument("Mspace free-list, dv, threshold or tracked allocation path not reconstructed");
    if (request>std::numeric_limits<std::uint64_t>::max()-0x80)
        throw std::invalid_argument("Mspace oversized allocation path not reconstructed");
    const auto dv_size=get64(h+8),dv_token=get64(h+0x20);
    if (dv_size!=64 || dv_token<c.base_token || dv_token-c.base_token>backing.size() ||
        backing.size()-(dv_token-c.base_token)<80)
        throw std::invalid_argument("Declared 64-byte dv required for mixed exact-bin allocation");
    const auto dv=dv_token-c.base_token;
    if ((get64(dv+8)&0xff00fffffffffff8ull)!=64 || (get64(dv+8)&3)!=1 ||
        get64(dv+64)!=64 || (get64(dv+72)&1))
        throw std::invalid_argument("Invalid declared dv boundary");
    const auto size=request<15?0x20ull:(request+0x2f)&~31ull;
    if (size!=64) throw std::invalid_argument("Mixed exact-bin request size outside verified contract");
    if (request>208) throw std::invalid_argument("Large-bin allocation not reconstructed");
    const auto index=size>>3;const auto bit=1u<<index;
    const auto map=get32(h);const auto sentinel=h+0x40+size*2;
    if (!(map&bit)) throw std::invalid_argument("No exact small-bin chunk; fallback/split not reconstructed");
    auto offset=[&](std::uint64_t token,std::uint64_t extent) {
        if (token<c.base_token || token-c.base_token>backing.size() || extent>backing.size()-(token-c.base_token))
            throw std::invalid_argument("Invalid small-bin chunk extent");
        return token-c.base_token;
    };
    const auto chunk_token=get64(sentinel+0x10),top=offset(chunk_token,size+16),next=top+size;
    if ((get64(top+8)&0xff00fffffffffff8ull)!=size || (get64(top+8)&3)!=1 ||
        get64(top+0x18)!=c.base_token+sentinel || (get64(next+8)&1))
        throw std::invalid_argument("Invalid exact-bin head or neighbor");
    const auto following=get64(top+0x10);
    const auto f=offset(following,32);
    if (get64(f+0x18)!=chunk_token)
        throw std::invalid_argument("Broken exact-bin forward link");
    if (following==c.base_token+sentinel) {
        const auto remaining=map&~bit;std::memcpy(backing.data()+h,&remaining,4);
    } else {
        put(sentinel+0x10,following);put(f+0x18,c.base_token+sentinel);
    }
    put(top+8,size|3);backing[next+8]|=std::byte{1};put(next,c.header_token^poison);
    const auto low=(request+16)&31;
    if (low) {
        const auto count=31-low;
        std::memset(backing.data()+next-(32-low),0xaf,static_cast<std::size_t>(count));
        backing[next-1]=static_cast<std::byte>((~count)&255);backing[top+8]|=std::byte{4};
    }
    const auto total=get64(h+0x360)+size;
    put(h+0x360,total);if (get64(h+0x368)<total) put(h+0x368,total);
    lock.release();
    return {c.base_token+top+16,backing.subspan(static_cast<std::size_t>(top+16),static_cast<std::size_t>(request))};
}
}
