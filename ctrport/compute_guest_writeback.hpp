// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion_client.hpp"
#include "guest_copy_api.h"
namespace ctrport {
// Provider must finish the real GPU writer, download, and retile using the
// public format/layout path, preserving initial padding. Returning success is
// forbidden while any of those operations remains unsupported.
using ComputeTiledDownload=int(*)(void*,const unsigned char*,uint64_t,unsigned char*,uint64_t);
// Image generations can exceed the guest-copy ABI's 16 MiB call bound
// (retail1426 sampled Z32 MS2 spans17694720B). Bound the whole image at32MiB,
// then use8MiB calls; the transaction still validates the whole generation.
inline int image_guest_copy(const CtrGuestCopyApi& memory,uint64_t address,void* buffer,uint64_t bytes,bool write){
 if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !memory.write || !address || !buffer || !bytes || bytes>32*1024*1024 || address>UINT64_MAX-bytes)return 60;
 const auto host=reinterpret_cast<uintptr_t>(buffer);
 // Reject whole-span aliasing before the first chunk. Per-call checks alone
 // could publish an earlier chunk before a later shifted overlap is detected.
 if(host>UINTPTR_MAX-bytes || (host<address+bytes && address<host+bytes))return 60;
 auto* data=static_cast<unsigned char*>(buffer);
 for(uint64_t offset=0;offset<bytes;){const uint64_t count=std::min<uint64_t>(bytes-offset,8*1024*1024);const int status=write?memory.write(memory.context,address+offset,data+offset,count):memory.read(memory.context,address+offset,data+offset,count);if(status)return status;offset+=count;}
 return 0;
}
struct ComputeGuestWriteback {
 uint64_t address=0;std::vector<unsigned char> initial;bool completed=false,poisoned=false;
 int commit(const CtrGuestCopyApi& memory,ComputeTiledDownload download,void* context){
  if(completed || poisoned)return 91;
  if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !memory.write || !address || initial.empty() || initial.size()>32*1024*1024 || address>UINT64_MAX-initial.size())return 60;
  if(!download)return 88;
  try{
   std::vector<unsigned char> current(initial.size()),result(initial.size());
   int status=image_guest_copy(memory,address,current.data(),current.size(),false);if(status)return status;
   if(current!=initial)return 88; // Concurrent CPU change: do not overwrite it.
   status=download(context,initial.data(),initial.size(),result.data(),result.size());if(status)return status;
   status=image_guest_copy(memory,address,current.data(),current.size(),false);if(status)return status;
   if(current!=initial)return 88;
   poisoned=true; // Includes an exception after publication begins.
   status=image_guest_copy(memory,address,result.data(),result.size(),true);
   if(status){poisoned=true;return status;}
   status=image_guest_copy(memory,address,current.data(),current.size(),false);
   if(status || current!=result){poisoned=true;return status?status:112;}
   initial.swap(result);completed=true;poisoned=false;return 0;
  }catch(...){return 112;}
 }
};

}
