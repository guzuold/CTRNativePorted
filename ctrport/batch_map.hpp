// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "settings/hot_trace.hpp"
#include "owned_guest_copy.hpp"
#include "direct_memory.hpp"
#include "query_mapping_records.hpp"
#include "video_session_owner.hpp"
#include "video/memory_change.h"
namespace ctrport {
// Public layout/operation from pinned shadPS4 memory.h/memory.cpp,
// e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later.
struct BatchMapEntry {uint64_t start,offset,length;uint8_t protection,type;int16_t reserved;int32_t operation;};
static_assert(sizeof(BatchMapEntry)==32);
struct BatchProtectionRecord {uint64_t bytes;unsigned protection,type;};
inline std::mutex batch_protection_mutex;
inline std::map<uint64_t,BatchProtectionRecord> batch_protections;
inline int runtime_batch_map(uint64_t address,int count,uint64_t output){
 constexpr int invalid=static_cast<int>(0x80020016u);
 const auto memory=make_owned_guest_copy_api();int processed=0;
 if(output && memory.read(memory.context,output,&processed,4))throw std::runtime_error("BatchMap output not owned RW");
 processed=0;
 const auto publish=[&]{if(output && memory.write(memory.context,output,&processed,4))throw std::runtime_error("BatchMap processed count write failed");};
 if(count<=0){publish();return 0;}
 if(count>16)throw std::runtime_error("BatchMap count exceeds supported transport");
 if(!address){publish();return invalid;}
 std::vector<BatchMapEntry> entries(static_cast<size_t>(count));
 if(memory.read(memory.context,address,entries.data(),entries.size()*sizeof(BatchMapEntry)))throw std::runtime_error("BatchMap entries not owned");
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);session=runtime_video_session;}
 auto backing=direct_memory_backing;
 if(guest_cpu_write_hook && backing){
  // GPU library (gpu_library.hpp): no ctrport renderer resources to admit. The
  // protection change reaches the library's caches through guest_cpu_write_hook.
  for(const auto& entry:entries){
   if(!entry.length || entry.operation>4){publish();return invalid;}
   if(entry.operation!=4 || entry.offset || entry.reserved)throw std::runtime_error("BatchMap unobserved operation/fields");
   const int result=backing->type_protect_writeback(entry.start,entry.length,entry.protection,entry.type);
   if(result){publish();return result;}
   record_query_mapping(entry.start,entry.length,entry.protection);
   {std::lock_guard<std::mutex> lock(batch_protection_mutex);batch_protections[entry.start]={entry.length,entry.protection,entry.type};}
   ++processed;
  }
  publish();return 0;
 }
 if(!session || !backing)throw std::runtime_error("BatchMap live mapping/renderer owner unavailable");
 for(const auto& entry:entries){
  if(!entry.length || entry.operation>4){publish();return invalid;}
  if(entry.operation!=4 || entry.offset || entry.reserved)throw std::runtime_error("BatchMap unobserved operation/fields");
  const int status=session->invoke([&](const CtrVideoSessionApiV1* api){
   const int admission=ctr_video_admit_memory_change_v1(api->context,entry.start,entry.length);
   if(admission)throw std::runtime_error("BatchMap renderer retained resource overlap/invalid admission="+std::to_string(admission));
   // Keep SAME owner dispatch while changing Windows and allocation type state.
   const int result=backing->type_protect_writeback(entry.start,entry.length,entry.protection,entry.type);
   if(!result){record_query_mapping(entry.start,entry.length,entry.protection);std::lock_guard<std::mutex> lock(batch_protection_mutex);batch_protections[entry.start]={entry.length,entry.protection,entry.type};}
   return result;
  });
  if(status){publish();return status;}++processed;
 }
 publish();return 0;
}
extern "C" int ctr_batch_map(uint64_t address,int count,uint64_t output,uint64_t caller){
 try{const int result=runtime_batch_map(address,count,output);if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"original_BatchMap_return\",\"caller\":%llu,\"count\":%d,\"result\":%d,\"type_protection_path\":true}\n",static_cast<unsigned long long>(caller),count,result);std::fflush(stdout);}return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=2SKEx6bSq-4 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_batch_map();
extern "C" int ctr_batch_map_bridge_fixture(void*,int,void*);
inline int batch_map_guest_control(){
 if(runtime_video_open(255,0,0,0)!=1)return 1;
 auto previous=direct_memory_backing;auto backing=std::make_shared<DirectMemoryBacking>(65536);direct_memory_backing=backing;
 int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,65536,65536,0,0,&physical) || backing->map(&mapped,65536,51,0,physical,0))return 1;
 GuestStack storage(32768,4096);auto* entry=static_cast<BatchMapEntry*>(storage.bottom());
 *entry={reinterpret_cast<uint64_t>(mapped)+16384,0,32768,50,3,0,4};
 auto* output=reinterpret_cast<int*>(static_cast<unsigned char*>(storage.bottom())+128);*output=123;
 if(ctr_batch_map_bridge_fixture(entry,1,output) || *output!=1)return 1;
 DirectQueryInfo info{};if(backing->query(16384,0,&info,24) || info.memory_type!=3)return 1;
 unsigned rejected=0;const auto address=reinterpret_cast<uint64_t>(entry);
 if(runtime_batch_map(0,1,reinterpret_cast<uint64_t>(output))==static_cast<int>(0x80020016u) && *output==0)++rejected;
 entry->operation=5;if(runtime_batch_map(address,1,0)==static_cast<int>(0x80020016u))++rejected;
 entry->operation=4;entry->type=7;try{runtime_batch_map(address,1,0);}catch(...){++rejected;}
 if(rejected!=3)return 1;
 direct_memory_backing=previous;if(runtime_video_close(1))return 1;
 std::cout<<"{\"scope\":\"batch_map_guest_control\",\"SysV3\":true,\"processed_count\":1,\"negative_rejections\":3,\"synthetic\":true}\n";return 0;
}
inline int batch_map_memory_control(){
 auto backing=std::make_shared<DirectMemoryBacking>(65536);int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(0,65536,65536,0,0,&physical) || backing->map(&mapped,65536,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(mapped);
 if(backing->type_protect_writeback(base+16384,32768,50,3))return 1;
 DirectQueryInfo first{},middle{},last{};
 if(backing->query(0,0,&first,24) || backing->query(16384,0,&middle,24) || backing->query(49152,0,&last,24) ||
    first.memory_type!=0 || middle.memory_type!=3 || last.memory_type!=0 || middle.start!=16384 || middle.end!=49152)return 1;
 MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(mapped,&info,sizeof(info)) || info.Protect!=PAGE_READWRITE)return 1;
 *reinterpret_cast<uint64_t*>(base+16384)=UINT64_MAX;
 if(*reinterpret_cast<uint64_t*>(base+16384)!=UINT64_MAX)return 1;
 unsigned rejected=0;
 if(backing->type_protect_writeback(base+1,16384,50,3)==static_cast<int>(0x80020016u))++rejected;
 if(backing->type_protect_writeback(base,0,50,3)==static_cast<int>(0x80020016u))++rejected;
 for(const auto address:{UINT64_MAX-16383,base+65536})try{backing->type_protect_writeback(address,16384,50,3);}catch(...){++rejected;}
 try{backing->type_protect_writeback(base,16384,1,3);}catch(...){++rejected;}
 try{backing->type_protect_writeback(base,16384,50,7);}catch(...){++rejected;}
 if(rejected!=6)return 1;
 std::cout<<"{\"scope\":\"batch_map_memory_control\",\"negative_rejections\":6,\"real_Windows_RW\":true,\"type_interval_split\":true,\"renderer_invalidation_tested\":false,\"guest_service_bound\":false}\n";return 0;
}
}
