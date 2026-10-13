#include "ctr/mspace_chunks.h"
#include <cstring>
#include <atomic>
#include <bit>
#include <limits>
#include <stdexcept>
namespace ctr {
void initialize_mspace_chunks(std::span<std::byte> backing,const MspaceChunksParameters& p) {
    const std::uint64_t h=p.header_offset;
    if (p.capacity>backing.size() || p.capacity<0x60 || h<8 || h>p.capacity || p.capacity-h<0x520 ||
        p.base_token>std::numeric_limits<std::uint64_t>::max()-p.capacity)
        throw std::invalid_argument("Invalid mspace backing or header");
    auto read64=[&](std::uint64_t offset) {std::uint64_t value;std::memcpy(&value,backing.data()+offset,8);return value;};
    std::uint32_t flags;std::memcpy(&flags,backing.data()+h+0x370,4);
    if ((flags&2) && !p.permit_mutex_initialization) throw std::invalid_argument("Mspace mutex requires explicit initialization");
    const auto chunk_size=read64(h-8)&0xff00fffffffffff8ull;
    if (chunk_size<0x520 || chunk_size>p.capacity-h)
        throw std::invalid_argument("Invalid prepared mspace chunk size");
    const auto next=h+chunk_size;
    const auto padding=(0ull-(p.base_token+next))&31ull;
    if (next+padding>p.capacity-0x50)
        throw std::invalid_argument("Missing mspace top chunk capacity");
    const auto top=next+padding-0x10;
    const auto available=p.capacity-top-0x60;
    auto put=[&](std::uint64_t offset,std::uint64_t value) {std::memcpy(backing.data()+offset,&value,8);};
    flags|=4;std::memcpy(backing.data()+h+0x370,&flags,4);
    for (std::uint64_t i=0;i<32;++i) {
        const auto sentinel=h+0x40+16*i;
        put(sentinel+0x10,p.base_token+sentinel);put(sentinel+0x18,p.base_token+sentinel);
    }
    put(h+0x28,p.base_token+top);put(h+0x10,available);
    put(top+8,available|1);put(p.capacity-0x58,0x60);
    put(h+0x30,0ull-static_cast<std::uint64_t>(p.global_lock_policy&1));
}
void register_mspace(std::span<std::byte> backing,std::uint64_t base_token,
    std::uint32_t header_offset,std::uint32_t initial_padding,
    std::uint64_t* bitmap,std::span<std::uint64_t> table) {
    const std::uint64_t h=header_offset,pad=initial_padding;
    if (h>backing.size() || backing.size()-h<0x3f8 || pad>backing.size() || backing.size()-pad<16 ||
        base_token>std::numeric_limits<std::uint64_t>::max()-backing.size())
        throw std::invalid_argument("Invalid mspace registration backing");
    if (!bitmap) return;
    if (table.size()<65 || reinterpret_cast<std::uintptr_t>(bitmap)%std::atomic_ref<std::uint64_t>::required_alignment)
        throw std::invalid_argument("Missing or unaligned mspace registry");
    std::atomic_ref<std::uint64_t> state(*bitmap);
    auto old=state.load();std::uint64_t bit;
    do {bit=(~old)&(old+1);} while (!state.compare_exchange_weak(old,old^bit));
    if (!bit) {const std::uint64_t zero=0;std::memcpy(backing.data()+h+0x3f0,&zero,8);return;}
    const auto index=static_cast<std::uint32_t>(std::countr_zero(bit)+1);
    std::memcpy(backing.data()+h+0x3f4,&index,4);
    std::uint32_t flags;std::memcpy(&flags,backing.data()+h+0x3f0,4);
    if (flags&1) {flags|=3;std::memcpy(backing.data()+h+0x3f0,&flags,4);}
    table[index]=base_token+h;
    if (flags&2) {
        std::uint64_t marker;std::memcpy(&marker,backing.data()+pad+8,8);
        marker|=(static_cast<std::uint64_t>(index)<<48)|0x80000000000000ull;
        std::memcpy(backing.data()+pad+8,&marker,8);
    }
}
}
