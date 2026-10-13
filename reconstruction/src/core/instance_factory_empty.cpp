// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_factory_empty.h"
#include "ctr/core/instance_construct_empty.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
namespace ctr {
std::uintptr_t instance_factory_empty(std::span<std::byte> m,std::uintptr_t id,std::size_t meta,std::size_t pool,std::size_t manager,std::size_t base,std::uint32_t& counter,const PoolFreshOriginal& allocate,std::uintptr_t verified_image_vtable){
 auto h=[&](std::size_t o){if(o>m.size()||2>m.size()-o)throw std::runtime_error("Missing factory metadata");std::uint16_t v;std::memcpy(&v,m.data()+o,2);return v;};
 auto flags=h(meta+0x5c);if(flags&32)throw std::runtime_error("Missing substitution registry");if(flags&16)return 0;
 auto size=h(meta+0x5a);if(!size||pool>m.size()||0x34>m.size()-pool)throw std::runtime_error("Missing factory pool/size");std::uint32_t alignment;std::memcpy(&alignment,m.data()+pool+0x30,4);alignment=std::max(alignment,static_cast<std::uint32_t>(h(meta+0x5e)));
 auto result=allocation_dispatch_fresh(m,id,pool,0,size,alignment,0,0,0x4013d7fe0,0,allocate,0x4013db610);if(result<id||result-id>m.size()||size>m.size()-(result-id))throw std::runtime_error("Missing factory allocated extent");
 std::memset(m.data()+result-id,0,size);++counter;instance_construct_empty(m,id,static_cast<std::size_t>(result-id),meta,id+pool,manager,base,verified_image_vtable);return result;
}
}
