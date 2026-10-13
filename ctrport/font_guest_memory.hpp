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
inline int font_guest_memory_control(){
 auto* region=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 if(!region)throw std::runtime_error("Font borrow control allocation failed");
 struct Cleanup{void* p;~Cleanup(){VirtualFree(p,0,MEM_RELEASE);}} cleanup{region};
 auto* mem=reinterpret_cast<FontMem*>(region);unsigned rejected=0;
 if(ctr_test_font_memory_init(mem,region+256,128,nullptr,reinterpret_cast<void*>(0x1234),nullptr,reinterpret_cast<void*>(0x5678))!=0 || mem->mspace_handle!=reinterpret_cast<void*>(0x1234) || mem->destroy_ctx!=reinterpret_cast<void*>(0x5678) || font_guest_memory_term(mem)!=0)throw std::runtime_error("Font borrowed lifecycle differs");
 std::memset(region,0x5a,8192);DWORD old=0;
 if(!VirtualProtect(region+4096,4096,PAGE_READONLY,&old))throw std::runtime_error("Font control protect failed");
 try{font_guest_memory_init(reinterpret_cast<FontMem*>(region+4096-32),region,8,nullptr,nullptr,nullptr,nullptr);}catch(const std::runtime_error&){++rejected;}
 for(unsigned i=4096-32;i<4096;++i)if(region[i]!=0x5a)throw std::runtime_error("Font failed output partially written");
 try{font_borrow_check(reinterpret_cast<void*>((std::numeric_limits<std::uintptr_t>::max)()-8),64,true);}catch(const std::runtime_error&){++rejected;}
 try{font_guest_memory_init(mem,region,8,reinterpret_cast<FontMemInterface*>(1),nullptr,nullptr,nullptr);}catch(const std::runtime_error&){++rejected;}
 try{font_guest_memory_init(mem,region,8,nullptr,nullptr,region,nullptr);}catch(const std::runtime_error&){++rejected;}
 if(rejected!=4)throw std::runtime_error("Font borrow negatives differ");
 std::cout<<"{\"scope\":\"font_synchronous_borrow_control\",\"negative_rejections\":4,\"failure_before_write\":true,\"retail_execution\":false}\n";return 0;
}
}
