#include "ctr/mspace_dv.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
MspaceAllocation allocate_mspace_dv(std::span<std::byte> backing,
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
    if (get32(h) || get32(h+4) || (flags&0x10) || (get32(h+0x3f0)&1))
        throw std::invalid_argument("Mspace free-list, dv, threshold or tracked allocation path not reconstructed");
    if (request>std::numeric_limits<std::uint64_t>::max()-0x80)
        throw std::invalid_argument("Mspace oversized allocation path not reconstructed");
    const auto size=request<15?0x20ull:(request+0x2f)&~31ull;
    const auto dv_token=get64(h+0x20),available=get64(h+8);
    if (!available || available<size || dv_token<c.base_token || (available&31))
        throw std::invalid_argument("Missing/insufficient dv; fallback not reconstructed");
    const auto top=dv_token-c.base_token;
    if (top>backing.size() || available>backing.size()-top || backing.size()-top-available<16)
        throw std::invalid_argument("Invalid dv extent");
    const auto end=top+available;
    if ((get64(top+8)&0xff00fffffffffff8ull)!=available || (get64(top+8)&3)!=1 ||
        get64(end)!=available || (get64(end+8)&1))
        throw std::invalid_argument("Invalid dv boundary");
    const auto remaining=available-size,allocated=remaining>=32?size:available,next=top+allocated;
    if (remaining>=32) {
        put(h+8,remaining);put(h+0x20,c.base_token+next);
        put(next+8,remaining|1);put(end,remaining);
    } else {
        put(h+8,0);put(h+0x20,0);backing[end+8]|=std::byte{1};
    }
    put(top+8,allocated|3);put(next,c.header_token^poison);
    const auto low=(request+16)&31;
    if (low) {
        const auto count=31-low;
        std::memset(backing.data()+next-(32-low),0xaf,static_cast<std::size_t>(count));
        backing[next-1]=static_cast<std::byte>((~count)&255);backing[top+8]|=std::byte{4};
    }
    const auto total=get64(h+0x360)+allocated;
    put(h+0x360,total);if (get64(h+0x368)<total) put(h+0x368,total);
    lock.release();
    return {c.base_token+top+16,backing.subspan(static_cast<std::size_t>(top+16),static_cast<std::size_t>(request))};
}
}
