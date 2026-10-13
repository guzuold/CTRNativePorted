// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_construct_empty.h"
#include <cstring>
#include <stdexcept>
#include <atomic>
namespace ctr {
void instance_construct_empty(std::span<std::byte> m,std::uintptr_t id,std::size_t object,std::size_t meta,std::uintptr_t pool,std::size_t manager,std::size_t base,std::uintptr_t verified_image_vtable){
 if(verified_image_vtable&&verified_image_vtable!=0x4025ba3c0ull)throw std::runtime_error("Missing verified retail vtable identity");
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing constructor extent");};
 auto q=[&](std::size_t o){check(o,8);std::uint64_t v;std::memcpy(&v,m.data()+o,8);return v;};
 auto i=[&](std::size_t o){check(o,4);std::uint32_t v;std::memcpy(&v,m.data()+o,4);return v;};
 auto off=[&](std::uintptr_t p){if(p<id||p-id>=m.size())throw std::runtime_error("Missing constructor pointer view");return static_cast<std::size_t>(p-id);};
 check(meta+0x5c,1);if((std::to_integer<unsigned>(m[meta+0x5c])&4)==0||i(meta+0x18)||i(base+0x18))throw std::runtime_error("Missing field construction branch");
 auto header=static_cast<std::int32_t>(i(manager+0xa8));if(header<0)throw std::runtime_error("Missing negative header offset");
 auto validate_vtable=[&](std::uintptr_t table){if(verified_image_vtable&&table==verified_image_vtable)return;auto vt=off(table);if(q(vt+0x20)!=0x4013d76f0ull||q(vt+0x50)!=0x4013d7790ull||q(vt+0x40)!=0x4013d7730ull)throw std::runtime_error("Missing constructor virtual dispatch");};
 validate_vtable(q(meta+0x38));
 auto value=q(meta+0x38);check(object+static_cast<std::size_t>(header),8);std::memcpy(m.data()+object+header,&value,8);
 std::uint32_t refs=pool?((i(off(pool)+0x1c)<<22)|1):1;check(object+8,4);std::memcpy(m.data()+object+8,&refs,4);
 if(header!=0){validate_vtable(q(object));}
 check(meta+0x30,4);auto p=reinterpret_cast<std::uint32_t*>(m.data()+meta+0x30);if(reinterpret_cast<std::uintptr_t>(p)%std::atomic_ref<std::uint32_t>::required_alignment)throw std::runtime_error("Missing aligned metadata counter");std::atomic_ref<std::uint32_t>(*p).fetch_add(1,std::memory_order_seq_cst);
}
}
