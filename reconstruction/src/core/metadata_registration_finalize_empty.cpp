// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registration_finalize_empty.h"
#include "ctr/metaobject_zero_field_finalize.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t metadata_registration_finalize_empty(std::span<std::byte> m,std::uintptr_t base,std::uintptr_t slot){
 auto off=[&](std::uintptr_t p,std::size_t size){if(p<base||p-base>m.size()||size>m.size()-(p-base))throw std::runtime_error("Missing bounded registration metadata");return static_cast<std::size_t>(p-base);};
 std::uintptr_t meta;std::memcpy(&meta,m.data()+off(slot,8),8);auto at=off(meta,152);std::uint16_t flags;std::memcpy(&flags,m.data()+at+0x5c,2);
 if((flags&14)!=2)throw std::runtime_error("Missing registration stage callbacks");
 std::int32_t fields,secondary;std::memcpy(&fields,m.data()+at+0x18,4);std::memcpy(&secondary,m.data()+at+0x68,4);if(fields>0||secondary>0)throw std::runtime_error("Missing field registration callbacks");
 flags|=8;std::memcpy(m.data()+at+0x5c,&flags,2);finalize_zero_field_metaobject_storage(m.subspan(at,152));
 std::memcpy(&flags,m.data()+at+0x5c,2);flags=static_cast<std::uint16_t>((flags|4)&~8);std::memcpy(m.data()+at+0x5c,&flags,2);return 0;
}
}
