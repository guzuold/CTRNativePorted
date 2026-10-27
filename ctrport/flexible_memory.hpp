// SPDX-License-Identifier: GPL-2.0-only
// Independent Win32 mapping owner. PS4 validation: pinned kernel/memory.cpp
// sceKernelMapNamedFlexibleMemory; CPU protection bits from core/memory.h.
#pragma once
#include "original_register_call.hpp"
#include "gpu_hooks.hpp"
namespace ctrport {
class FlexibleMemory {
 struct Mapping{void* base=nullptr;size_t bytes=0;int protection=0;std::string name;~Mapping(){if(base)VirtualFree(base,0,MEM_RELEASE);}};
 std::mutex mutex;std::map<uint64_t,std::unique_ptr<Mapping>> mappings;
public:
 int map(void** output,uint64_t bytes,int prot,int flags,const char* name){
  constexpr int invalid=static_cast<int>(0x80020016u),fault=static_cast<int>(0x8002000eu),long_name=static_cast<int>(0x8002003fu);
  if(!bytes || bytes%16384)return invalid;
  if(!output || !name)return fault;
  require_time_output(output,sizeof(void*));
  std::string copied;for(size_t i=0;i<32;++i){char c=0;SIZE_T read=0;if(!ReadProcessMemory(GetCurrentProcess(),name+i,&c,1,&read) || read!=1)return fault;if(!c)break;copied.push_back(c);}if(copied.size()==32)return long_name;
  void* requested=nullptr;SIZE_T read=0;if(!ReadProcessMemory(GetCurrentProcess(),output,&requested,sizeof(requested),&read) || read!=sizeof(requested))return fault;
  if(requested || flags || prot<0 || prot>3)throw std::runtime_error("Flexible mapping supports observed null hint/flags0/CPU-only protections0..3");
  if(bytes>0x100000000ULL)throw std::runtime_error("Flexible mapping size outside bounded guest address search");
  DWORD protection=prot==0?PAGE_NOACCESS:prot==1?PAGE_READONLY:PAGE_READWRITE;
  auto mapping=std::make_unique<Mapping>();mapping->bytes=static_cast<size_t>(bytes);mapping->protection=prot;mapping->name=std::move(copied);
  std::lock_guard<std::mutex> lock(mutex);
  for(uint64_t candidate=0x2000000000ULL;candidate+bytes<=0xfbffffffffULL;candidate+=0x10000000ULL){
   mapping->base=VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(candidate)),static_cast<SIZE_T>(bytes),MEM_RESERVE|MEM_COMMIT|guest_write_watch_flag(),protection);if(mapping->base)break;
  }
  if(!mapping->base)throw std::runtime_error("Flexible mapping VirtualAlloc failed");
  auto* base=mapping->base;mappings.emplace(reinterpret_cast<uint64_t>(base),std::move(mapping));*output=base;return 0;
 }
 void release_control(void* base){std::lock_guard<std::mutex> lock(mutex);if(!mappings.erase(reinterpret_cast<uint64_t>(base)))throw std::runtime_error("Unknown flexible mapping control owner");}
};
inline FlexibleMemory flexible_memory;
extern "C" int ctr_map_flexible(void** output,uint64_t bytes,int prot,int flags,const char* name,uint64_t caller){try{int result=flexible_memory.map(output,bytes,prot,flags,name);std::printf("{\"scope\":\"original_MapNamedFlexibleMemory_return\",\"caller\":%llu,\"bytes\":%llu,\"protection\":%d,\"flags\":%d,\"result\":%d,\"address\":%llu,\"owned_Windows_mapping\":true}\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(bytes),prot,flags,result,static_cast<unsigned long long>(result?0:reinterpret_cast<uint64_t>(*output)));std::fflush(stdout);return result;}catch(const std::exception& e){std::fprintf(stderr,"NID=mL8NDH86iQI library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_sysv_map_flexible();
}
