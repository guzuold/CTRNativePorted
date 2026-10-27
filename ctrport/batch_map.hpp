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
}
