// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "direct_memory_copy.hpp"
#include "video/gnm_shader_emit.h"
namespace ctrport {
// Runtime transport only. Whole public packet emission belongs to data.
// Keep direct mapping owner and recursive ledger transaction through snapshot,
// emitter validation and final write. Unsupported authorities fail explicitly.
inline int runtime_gnm_set_vs(uint64_t command,uint32_t capacity,uint64_t registers,uint32_t modifier){
 auto owner=direct_memory_backing;
 if(!owner)throw std::runtime_error("SetVsShader mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_set_vs_shader(&memory,command,capacity,registers,modifier,&result);
 if(host)throw std::runtime_error("SetVsShader checked emitter rejected host_status="+std::to_string(host));
 return result;
}
extern "C" int ctr_gnm_set_vs(uint64_t command,uint32_t capacity,uint64_t registers,uint32_t modifier,uint64_t caller){
 try{const int result=runtime_gnm_set_vs(command,capacity,registers,modifier);
  std::printf("{\"scope\":\"original_GnmSetVsShader_return\",\"caller\":%llu,\"capacity_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),capacity,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=gAhCn6UiU4Y library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_set_vs();
extern "C" int ctr_gnm_set_vs_bridge_fixture(uint64_t,uint32_t,uint64_t,uint32_t);
inline int runtime_gnm_set_ps(uint64_t command,uint32_t capacity,uint64_t registers){
 auto owner=direct_memory_backing;if(!owner)throw std::runtime_error("SetPsShader350 mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_set_ps_shader_350(&memory,command,capacity,registers,&result);
 if(host)throw std::runtime_error("SetPsShader350 checked emitter rejected host_status="+std::to_string(host));return result;
}
extern "C" int ctr_gnm_set_ps(uint64_t command,uint32_t capacity,uint64_t registers,uint64_t caller){
 try{const int result=runtime_gnm_set_ps(command,capacity,registers);std::printf("{\"scope\":\"original_GnmSetPsShader350_return\",\"caller\":%llu,\"capacity_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),capacity,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=5uFKckiJYRM library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_set_ps();
extern "C" int ctr_gnm_set_ps_bridge_fixture(uint64_t,uint32_t,uint64_t);
}

namespace ctrport {
inline int runtime_gnm_draw_auto(uint64_t command,uint32_t size,uint32_t count,uint32_t flags){
 auto owner=direct_memory_backing;if(!owner)throw std::runtime_error("DrawIndexAuto mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_draw_index_auto(&memory,command,size,count,flags,process_neo_mode(),&result);
 if(host)throw std::runtime_error("DrawIndexAuto checked emitter rejected host_status="+std::to_string(host));return result;
}
extern "C" int ctr_gnm_draw_auto(uint64_t command,uint32_t size,uint32_t count,uint32_t flags,uint64_t caller){
 try{const int result=runtime_gnm_draw_auto(command,size,count,flags);
 std::printf("{\"scope\":\"original_GnmDrawIndexAuto_return\",\"caller\":%llu,\"size_dwords\":%u,\"index_count\":%u,\"flags\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),size,count,flags,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=GGsn7jMTxw4 library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_draw_auto();
extern "C" int ctr_gnm_draw_auto_bridge_fixture(uint64_t,uint32_t,uint32_t,uint32_t);
}

namespace ctrport {
inline int runtime_gnm_update_vs(uint64_t command,uint32_t capacity,uint64_t registers,uint32_t modifier){
 auto owner=direct_memory_backing;
 if(!owner)throw std::runtime_error("UpdateVsShader mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_update_vs_shader(&memory,command,capacity,registers,modifier,&result);
 if(host)throw std::runtime_error("UpdateVsShader checked emitter rejected host_status="+std::to_string(host));
 return result;
}
extern "C" int ctr_gnm_update_vs(uint64_t command,uint32_t capacity,uint64_t registers,uint32_t modifier,uint64_t caller){
 try{const int result=runtime_gnm_update_vs(command,capacity,registers,modifier);
  std::printf("{\"scope\":\"original_GnmUpdateVsShader_return\",\"caller\":%llu,\"capacity_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),capacity,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=V31V01UiScY library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_update_vs();
extern "C" int ctr_gnm_update_vs_bridge_fixture(uint64_t,uint32_t,uint64_t,uint32_t);
inline int runtime_gnm_set_cs(uint64_t command,uint32_t capacity,uint64_t registers,uint32_t modifier){
 auto owner=direct_memory_backing;
 if(!owner)throw std::runtime_error("SetCsShaderWithModifier mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_set_cs_shader_modifier(&memory,command,capacity,registers,modifier,&result);
 if(host)throw std::runtime_error("SetCsShaderWithModifier checked emitter rejected host_status="+std::to_string(host));
 return result;
}
extern "C" int ctr_gnm_set_cs(uint64_t command,uint32_t capacity,uint64_t registers,uint32_t modifier,uint64_t caller){
 try{const int result=runtime_gnm_set_cs(command,capacity,registers,modifier);
  std::printf("{\"scope\":\"original_GnmSetCsShaderWithModifier_return\",\"caller\":%llu,\"capacity_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),capacity,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=Kx-h-nWQJ8A library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_set_cs();

inline int runtime_gnm_dispatch_direct(uint64_t command,uint32_t size,uint32_t x,uint32_t y,uint32_t z,uint32_t flags){
 auto owner=direct_memory_backing;if(!owner)throw std::runtime_error("DispatchDirect mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);int32_t result=123;
 const int host=ctr_gnm_dispatch_direct(&memory,command,size,x,y,z,flags,&result);
 if(host)throw std::runtime_error("DispatchDirect checked emitter rejected host_status="+std::to_string(host));return result;
}
extern "C" int ctr_gnm_dispatch_direct_bridge(uint64_t command,uint32_t size,uint32_t x,uint32_t y,uint32_t z,uint32_t flags,uint64_t caller){
 try{int rc=runtime_gnm_dispatch_direct(command,size,x,y,z,flags);
 std::printf("{\"scope\":\"original_GnmDispatchDirect_return\",\"caller\":%llu,\"x\":%u,\"y\":%u,\"z\":%u,\"flags\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),x,y,z,flags,rc);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=0BzLGljcwBo library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_dispatch_direct();
extern "C" int ctr_gnm_dispatch_direct_fixture(uint64_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
inline int runtime_gnm_update_ps(uint64_t command,uint32_t capacity,uint64_t registers){
 auto owner=direct_memory_backing;if(!owner)throw std::runtime_error("UpdatePsShader350 mapping owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);
 int32_t result=123;const int host=ctr_gnm_update_ps_shader_350(&memory,command,capacity,registers,&result);
 if(host)throw std::runtime_error("UpdatePsShader350 checked emitter rejected host_status="+std::to_string(host));return result;
}
extern "C" int ctr_gnm_update_ps(uint64_t command,uint32_t capacity,uint64_t registers,uint64_t caller){
 try{const int result=runtime_gnm_update_ps(command,capacity,registers);std::printf("{\"scope\":\"original_GnmUpdatePsShader350_return\",\"caller\":%llu,\"capacity_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),capacity,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=mLVL7N7BVBg library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_update_ps();
}
