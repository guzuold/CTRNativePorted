// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_release_nonterminal.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void instance_release_nonterminal(std::span<std::byte> object){
 if(object.size()<12)throw std::runtime_error("Missing bounded reference word");
 auto* p=reinterpret_cast<std::uint32_t*>(object.data()+8);
 if(reinterpret_cast<std::uintptr_t>(p)%std::atomic_ref<std::uint32_t>::required_alignment)throw std::runtime_error("Missing aligned reference word");
 std::uint32_t refs;std::memcpy(&refs,p,4);
 if((refs&0x7ffff)<=1||(refs&0x300000))throw std::runtime_error("Missing terminal/deferred original lifetime services");
 std::atomic_ref<std::uint32_t>(*p).fetch_sub(1,std::memory_order_seq_cst);
}
}
