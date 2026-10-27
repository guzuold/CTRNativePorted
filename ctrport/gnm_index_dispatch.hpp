// SPDX-License-Identifier: GPL-2.0-only
#pragma once
namespace ctrport {
inline thread_local bool index_abi_control_active=false;
inline thread_local std::array<uint64_t,7> index_abi_capture{};
extern "C" int ctr_gnm_draw_index_bridge(uint64_t command,uint32_t size,uint32_t count,uint64_t indices,uint32_t flags,uint32_t type,uint64_t caller){
 if(index_abi_control_active){index_abi_capture={command,size,count,indices,flags,type,caller};return 42;}
 try{auto owner=direct_memory_backing;if(!owner)throw std::runtime_error("DrawIndex mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_draw_index(&memory,command,size,count,indices,flags,type,process_neo_mode(),&result);
 if(host)throw std::runtime_error("DrawIndex checked emitter host_status="+std::to_string(host));
 std::printf("{\"scope\":\"original_GnmDrawIndex_return\",\"caller\":%llu,\"index_count\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),count,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=HlTPoZ-oY7Y library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_draw_index();
extern "C" int ctr_gnm_draw_index_fixture(uint64_t,uint32_t,uint32_t,uint64_t,uint32_t,uint32_t);
}
