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
inline int virtual_query_control(){
 auto previous=direct_memory_backing;auto backing=std::make_shared<DirectMemoryBacking>(65536);direct_memory_backing=backing;
 int64_t physical=-1;void* mapped=nullptr;
 if(backing->allocate(16384,65536,32768,0,3,&physical) || backing->map(&mapped,32768,51,0,physical,0))return 1;
 record_query_mapping(reinterpret_cast<uint64_t>(mapped),32768,51);
 GuestVirtualQueryInfo info{};OriginalRegisterArguments args{};args.integer_count=4;
 args.integers[0]=reinterpret_cast<uint64_t>(mapped)+7;args.integers[1]=0;args.integers[2]=reinterpret_cast<uint64_t>(&info);args.integers[3]=72;
 if(invoke_original_registers(reinterpret_cast<void*>(&ctr_sysv_virtual_query),args,4,0).integer)return 2;
 if(info.start!=reinterpret_cast<uint64_t>(mapped) || info.end!=info.start+32768 || info.offset!=16384 || info.protection!=51 || info.memory_type!=3 || info.flags!=18)return 3;
 if(backing->type_protect_writeback(info.start+16384,16384,50,3))return 4;
 record_query_mapping(reinterpret_cast<uint64_t>(mapped)+16384,16384,50);
 if(runtime_virtual_query(reinterpret_cast<uint64_t>(mapped)+16384,0,&info,72) || info.start!=reinterpret_cast<uint64_t>(mapped)+16384 || info.protection!=50 || info.offset!=32768)return 5;
 unsigned rejected=0;
 for(auto address:{reinterpret_cast<uint64_t>(mapped)-1,reinterpret_cast<uint64_t>(mapped)+32768})try{runtime_virtual_query(address,0,&info,72);}catch(...){++rejected;}
 try{runtime_virtual_query(reinterpret_cast<uint64_t>(mapped),1,&info,72);}catch(...){++rejected;}
 if(rejected!=3)return 6;
 direct_memory_backing=previous;
 std::cout<<"{\"scope\":\"virtual_query_control\",\"passed\":true,\"SysV_layout72\":true,\"actual_owner_ranges\":true,\"split_type_protection\":true,\"unknown_owners_fail\":true}\n";return 0;
}
}
