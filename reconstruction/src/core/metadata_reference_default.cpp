// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_reference_default.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace {template<class T>T read(const std::byte*p){T v;std::memcpy(&v,p,sizeof(v));return v;}}
namespace ctr {
void metadata_reference_default(std::span<std::byte,88> metadata,std::span<std::byte> storage,std::span<const std::byte,112> manager){
 auto*m=metadata.data();auto alignment=read<std::int32_t>(m+80);
 if(alignment==-1&&read<std::uintptr_t>(m+72))throw std::runtime_error("Missing original type virtual+130 alignment query");
 if(read<unsigned char>(m+34)&16)throw std::runtime_error("Missing original named pool registry/intern/release domain");
 if(storage.size()<12)throw std::runtime_error("Missing storage ownership");
 const auto slot=read<std::uint32_t>(storage.data()+8)>>22;std::uintptr_t pool=0;
 if(read<unsigned char>(manager.data()+108)){
  if(read<std::uintptr_t>(manager.data()+96))throw std::runtime_error("Missing synchronization virtual+f8/e8");
  const auto registry=read<std::uintptr_t>(manager.data()+56);if(!registry)throw std::runtime_error("Missing active pool registry");
  const auto*r=reinterpret_cast<const std::byte*>(registry);const auto*indices=reinterpret_cast<const std::byte*>(read<std::uintptr_t>(r+40));
  if(read<std::int32_t>(indices+12)>static_cast<std::int32_t>(slot)){
   const auto*codes=reinterpret_cast<const std::byte*>(read<std::uintptr_t>(indices+32));
   if(read<std::uint16_t>(codes+slot*2)==(slot|0x8000u)){
    const auto*values=reinterpret_cast<const std::byte*>(read<std::uintptr_t>(r+32));pool=read<std::uintptr_t>(values+slot*8);
   }
  }
 }else {const auto index=slot&1023u;const auto offset=index==1?16:index==2?32:index==3?40:24;pool=read<std::uintptr_t>(manager.data()+offset);}
 const auto offset=read<std::uint16_t>(m+20);if(offset>storage.size()||storage.size()-offset<16)throw std::runtime_error("Missing storage field extent");
 auto*field=storage.data()+offset;auto header=read<std::uint64_t>(field);
 if(!read<std::uintptr_t>(field+8)){const auto token=pool?static_cast<std::uint64_t>(static_cast<std::int64_t>(read<std::int32_t>(reinterpret_cast<const std::byte*>(pool)+28))+1):0;header=(header&0xfff0000000000000ull)|(token&0x000fffffffffffffull);std::memcpy(field,&header,8);}
 if(alignment!=-1){const auto value=static_cast<std::uint32_t>(alignment)>4u?static_cast<std::uint32_t>(alignment):4u;const auto code=(std::bit_width(value)-3u)&15u;header=(header&0xff0fffffffffffffull)|(static_cast<std::uint64_t>(code)<<52);std::memcpy(field,&header,8);}
}
}
