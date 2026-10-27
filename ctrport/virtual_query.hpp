// SPDX-License-Identifier: GPL-2.0-only
// Independent owner query. Public contract: shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a,
// memory.h OrbisVirtualQueryInfo / core/memory.cpp VirtualQuery, GPL-2.0-or-later.
#pragma once
#include "direct_memory.hpp"
#include "query_mapping_records.hpp"
namespace ctrport {
inline int runtime_virtual_query(uint64_t address,int flags,GuestVirtualQueryInfo* output,uint64_t size){
 if(flags!=0 || size!=72)throw std::runtime_error("VirtualQuery supports observed flags0/size72 only");
 GuestVirtualQueryInfo info{};
 auto owner=direct_memory_backing;
 if(!owner || !owner->virtual_query(address,info))throw std::runtime_error("VirtualQuery address has no supported direct mapping owner");
 {
 std::lock_guard<std::mutex> lock(query_mapping_mutex);auto at=query_mapping_records.upper_bound(address);
 if(at==query_mapping_records.begin())throw std::runtime_error("VirtualQuery no retained guest protection");--at;
 if(address>=at->second.end)throw std::runtime_error("VirtualQuery protection gap");
 const auto start=(std::max)(info.start,at->first);info.offset+=start-info.start;info.start=start;info.end=(std::min)(info.end,at->second.end);info.protection=at->second.protection;
 }
 if(!output)return static_cast<int>(0x8002000eu);
 require_time_output(output,sizeof(info));*output=info;return 0;
}
extern "C" int ctr_virtual_query(uint64_t address,int flags,GuestVirtualQueryInfo* output,uint64_t size,uint64_t caller){
 try{const int result=runtime_virtual_query(address,flags,output,size);
 std::printf("{\"scope\":\"original_VirtualQuery_return\",\"caller\":%llu,\"address\":%llu,\"result\":%d,\"owner\":\"direct\",\"start\":%llu,\"end\":%llu,\"protection\":%d,\"memory_type\":%d}\n",(unsigned long long)caller,(unsigned long long)address,result,(unsigned long long)(result?0:output->start),(unsigned long long)(result?0:output->end),result?0:output->protection,result?0:output->memory_type);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=rVjRvHJ0X6c library=libkernel caller=0x%llx unsupported: %s\n",(unsigned long long)caller,e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_virtual_query();
}
