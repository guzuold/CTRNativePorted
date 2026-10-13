#include "ctr/data/igz_append_dependencies.h"
#include <atomic>
#include <bit>
#include <cstddef>
#include <stdexcept>
namespace ctr::data {
static_assert(sizeof(IgzDependencyRecord)==40);
static_assert(offsetof(IgzDependencyArray,packed_capacity)==8);
static_assert(offsetof(IgzDependencyArray,data)==16);
static void retain(std::uint64_t name) {
    if(name) {
        auto* counter=reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(name)-8);
        std::atomic_ref<std::uint32_t>(*counter).fetch_add(1,std::memory_order_seq_cst);
    }
}
void igz_append_dependencies(IgzDependencyArray& a,const IgzDependencyRecord* begin,
                             const IgzDependencyRecord* end) {
    const auto distance=reinterpret_cast<std::uintptr_t>(end)-reinterpret_cast<std::uintptr_t>(begin);
    const auto added=static_cast<std::uint32_t>(distance>>3)*0xcccccccdu;
    const auto total=a.count+added;
    const auto capacity=a.data?static_cast<std::uint32_t>((a.packed_capacity&0xfffffffffffffull)/40):0u;
    if(std::bit_cast<std::int32_t>(total)>std::bit_cast<std::int32_t>(capacity)) {
        const auto bytes=total*40u;
        const auto old=a.data?static_cast<std::uint32_t>(a.packed_capacity):0u;
        if(std::bit_cast<std::int32_t>(bytes)>std::bit_cast<std::int32_t>(old))
            throw std::runtime_error("missing original growth014f4d80");
    }
    if(begin!=end) {
        auto* destination=a.data+std::bit_cast<std::int32_t>(a.count);
        do {
            destination->first_name=begin->first_name;
            retain(destination->first_name);
            destination->first_hash=begin->first_hash;
            destination->second_name=begin->second_name;
            retain(destination->second_name);
            destination->second_hash=begin->second_hash;
            destination->value=begin->value;
            ++begin;++destination;
        } while(begin!=end);
    }
    a.count+=added;
}
}
