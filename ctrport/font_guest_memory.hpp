// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "font_memory.hpp"
#include "entry_boundary.hpp"
namespace ctrport {
// Synchronous borrowed PS4 API object: caller retains ownership and must not free,
// unmap, re-protect or modify it until return. This is not an owned GPU snapshot.
inline void font_borrow_check(const void* pointer,std::size_t size,bool write){
 auto p=reinterpret_cast<std::uintptr_t>(pointer);
 if(!pointer || size>(std::numeric_limits<std::uintptr_t>::max)()-p)throw std::runtime_error("Font borrowed object range invalid");
 const auto end=p+size;
 while(p<end){MEMORY_BASIC_INFORMATION info{};
  if(!VirtualQuery(reinterpret_cast<const void*>(p),&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))throw std::runtime_error("Font object unmapped");
  const auto mode=info.Protect&0xff;
  const bool writable=mode==PAGE_READWRITE || mode==PAGE_WRITECOPY || mode==PAGE_EXECUTE_READWRITE || mode==PAGE_EXECUTE_WRITECOPY;
  if(!(writable || mode==PAGE_READONLY || mode==PAGE_EXECUTE_READ) || (write && !writable))throw std::runtime_error("Font object permissions rejected");
  const auto base=reinterpret_cast<std::uintptr_t>(info.BaseAddress);
  if(info.RegionSize>(std::numeric_limits<std::uintptr_t>::max)()-base || base+info.RegionSize<=p)throw std::runtime_error("Font region extent overflow");
  p=(std::min)(end,base+info.RegionSize);
 }
}
inline void font_callback_check(void* callback){
 if(!callback)return;
 if(!entry_images || !entry_metadata)throw std::runtime_error("Font original callback owner unavailable");
 const auto address=reinterpret_cast<std::uint64_t>(callback);
 for(std::size_t i=0;i<entry_images->size();++i){const auto base=reinterpret_cast<std::uint64_t>((*entry_images)[i].get());
  for(const auto& span:(*entry_metadata)[i].executable_spans)if(address>=base+span.first && address<base+span.second){
   (void)resolve_original_call(*entry_images,*entry_metadata,entry_metadata->front().sha256,(*entry_metadata)[i].sha256,address-base);return;
  }
 }
 throw std::runtime_error("Font callback outside original executable images");
}
inline int font_guest_memory_init(FontMem* memory,void* region,std::uint32_t size,const FontMemInterface* iface,void* mspace,void* destroy,void* context){
 if(!memory)return font_invalid_parameter;
 font_borrow_check(memory,sizeof(*memory),true);
 if(iface)font_borrow_check(iface,sizeof(*iface),false);
 font_callback_check(destroy);
 return font_memory_init(memory,region,size,iface,mspace,destroy,context);
}
inline int font_guest_memory_term(FontMem* memory){
 if(!memory)return font_invalid_parameter;
 font_borrow_check(memory,sizeof(*memory),true);
 if(memory->mem_kind==0xf00){
  if((memory->attr_bits&0x80)!=0){if(memory->iface){font_borrow_check(memory->iface,sizeof(*memory->iface),false);font_callback_check(memory->iface->mspace_destroy);}}
  else font_callback_check(memory->on_destroy);
 }
 return font_memory_term(memory);
}
extern "C" int ctr_test_font_memory_init(FontMem*,void*,std::uint32_t,const FontMemInterface*,void*,void*,void*);
}
