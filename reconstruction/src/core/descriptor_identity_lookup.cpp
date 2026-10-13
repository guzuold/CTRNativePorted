// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/descriptor_identity_lookup.h"
#include <cstring>
#include <stdexcept>
namespace {template<class T>T read(const std::byte*p){T v{};std::memcpy(&v,p,sizeof(v));return v;}}
namespace ctr {
bool descriptor_identity_lookup(std::span<const std::byte,48> table,const std::uint64_t& query,std::uint32_t hash,std::uintptr_t& output,const std::uint64_t& sentinel,const std::uint8_t& initialized){
 if(!initialized)throw std::runtime_error("Missing libc lazy initialization:3GPpjQdAMTw/tsvEmnenz48/9rAeANT2tyE caller37afaf");
 if(query==sentinel)return false;
 const auto keys=read<std::uintptr_t>(table.data()+0x28);if(!keys)return false;
 const auto slots=static_cast<std::uint32_t>(read<std::uint64_t>(table.data()+0x20)>>3);if(!slots)return false;
 if(!initialized)throw std::runtime_error("Missing libc lazy initialization caller37aff5");
 std::uint32_t index=hash%slots,visited=0;
 for(;;){const auto key=read<std::uint64_t>(reinterpret_cast<const std::byte*>(keys)+static_cast<std::size_t>(index)*8);
  if(key==query||key==sentinel)break;
  ++index;if(index==slots)index=0;if(++visited>=slots)return false;
 }
 if(index==0xffffffff)return false;
 const auto key=read<std::uint64_t>(reinterpret_cast<const std::byte*>(keys)+static_cast<std::size_t>(index)*8);
 if(key!=query)return false;
 output=read<std::uintptr_t>(table.data()+0x18)+static_cast<std::uintptr_t>(index)*8;return true;
}
}
