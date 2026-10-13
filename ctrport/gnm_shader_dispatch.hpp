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
inline int gnm_set_ps_guest_control(){
 auto old=direct_memory_backing;auto owner=std::make_shared<DirectMemoryBacking>(16384);direct_memory_backing=owner;
 int64_t physical=-1;void* mapped=nullptr;if(owner->allocate(0,16384,16384,0,0,&physical) || owner->map(&mapped,16384,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(mapped);auto* words=static_cast<uint32_t*>(mapped);
 const uint32_t source[12]={0x1234,0,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa};
 std::memcpy(words+128,source,sizeof(source));std::fill(words,words+48,0xabcdef01);
 if(ctr_gnm_set_ps_bridge_fixture(base,40,base+512) || words[2]!=source[0] || words[27]!=source[11] || words[28]!=0xc00a1000 || words[29])return 1;
 for(unsigned i=30;i<48;++i)if(words[i]!=0xabcdef01)return 1;
 std::fill(words,words+48,0xabcdef01);
 if(ctr_gnm_set_ps_bridge_fixture(base,40,0) || words[2] || words[3] || words[9]!=15 || words[10]!=0xc01c1000 || words[11])return 1;
 for(unsigned i=12;i<48;++i)if(words[i]!=0xabcdef01)return 1;
 const std::vector<uint32_t> saved(words,words+48);unsigned rejected=0;
 if(runtime_gnm_set_ps(base,39,base+512)==-1)++rejected;
 words[129]=1;if(runtime_gnm_set_ps(base,40,base+512)==-1)++rejected;words[129]=0;
 for(auto destination:{base+16384-4,UINT64_MAX-3})try{runtime_gnm_set_ps(destination,40,base+512);}catch(...){++rejected;}
 try{runtime_gnm_set_ps(base,40,base+4);}catch(...){++rejected;}
 try{runtime_gnm_set_ps(base,40,base+16384-4);}catch(...){++rejected;}
 DWORD protection=0;if(!VirtualProtect(mapped,4096,PAGE_READONLY,&protection))return 1;
 try{runtime_gnm_set_ps(base,40,base+512);}catch(...){++rejected;}
 if(!VirtualProtect(mapped,4096,protection,&protection) || rejected!=7 || !std::equal(saved.begin(),saved.end(),words))return 1;
 direct_memory_backing=old;std::cout<<"{\"scope\":\"gnm_set_ps_guest_control\",\"SysV3\":true,\"null_and_nonnull_branches\":true,\"negative_rejections\":7,\"tail_preserved\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
inline int gnm_set_vs_guest_control(){
 auto old=direct_memory_backing;auto owner=std::make_shared<DirectMemoryBacking>(16384);direct_memory_backing=owner;
 int64_t physical=-1;void* mapped=nullptr;
 if(owner->allocate(0,16384,16384,0,0,&physical) || owner->map(&mapped,16384,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(mapped);auto* words=static_cast<uint32_t*>(mapped);
 const uint32_t source[7]={0x1234,0,0xdeadbeef,0x11,0x22,0x33,0x44};
 std::memcpy(words+128,source,sizeof(source));std::fill(words,words+40,0xabcdef01);
 if(ctr_gnm_set_vs_bridge_fixture(base,29,base+512,0x3000000))return 1;
 if(words[0]!=0xc0027600 || words[2]!=source[0] || words[6]!=((source[2]&0xfcfffc3f)|0x3000000) || words[17]!=0xc00a1000 || words[18])return 1;
 for(unsigned i=19;i<40;++i)if(words[i]!=0xabcdef01)return 1;
 const std::vector<uint32_t> saved(words,words+40);unsigned rejected=0;
 for(const auto size:{0u,28u})if(runtime_gnm_set_vs(base,size,base+512,0)==-1)++rejected;
 if(runtime_gnm_set_vs(base,29,base+512,1)==-1)++rejected;
 words[129]=1;if(runtime_gnm_set_vs(base,29,base+512,0)==-1)++rejected;words[129]=0;
 for(const auto destination:{base+16384-4,UINT64_MAX-3})try{runtime_gnm_set_vs(destination,29,base+512,0);}catch(...){++rejected;}
 try{runtime_gnm_set_vs(base,29,base+16384-4,0);}catch(...){++rejected;}
 try{runtime_gnm_set_vs(base,29,base+4,0);}catch(...){++rejected;}
 DWORD protection=0;if(!VirtualProtect(mapped,4096,PAGE_READONLY,&protection))return 1;
 try{runtime_gnm_set_vs(base,29,base+512,0);}catch(...){++rejected;}
 if(!VirtualProtect(mapped,4096,protection,&protection))return 1;
 if(rejected!=9 || !std::equal(saved.begin(),saved.end(),words))return 1;
 direct_memory_backing=old;
 std::cout<<"{\"scope\":\"gnm_set_vs_guest_control\",\"SysV4\":true,\"negative_rejections\":9,\"preserved_trailing_words\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
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
inline int gnm_draw_auto_guest_control(){
 auto old=direct_memory_backing;auto owner=std::make_shared<DirectMemoryBacking>(4736ULL*1024*1024);direct_memory_backing=owner;
 int64_t physical=-1;void* mapped=nullptr;if(owner->allocate(0,16384,16384,0,0,&physical) || owner->map(&mapped,16384,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(mapped);auto* words=static_cast<uint32_t*>(mapped);
 std::fill(words,words+12,0xabcdef01);
 if(ctr_gnm_draw_auto_bridge_fixture(base,7,3,0) || words[0]!=0xc0012d00 || words[1]!=3 || words[2]!=2 || words[3]!=0xc0021000 || words[4])return 1;
 for(unsigned i=5;i<12;++i)if(words[i]!=0xabcdef01)return 1;
 const std::vector<uint32_t> saved(words,words+12);unsigned rejected=0;
 if(runtime_gnm_draw_auto(base,6,3,0)==-1)++rejected;
 if(runtime_gnm_draw_auto(base,7,3,2)==-1)++rejected;
 for(auto destination:{base+16384-4,UINT64_MAX-3})try{runtime_gnm_draw_auto(destination,7,3,0);}catch(...){++rejected;}
 DWORD protection=0;if(!VirtualProtect(mapped,4096,PAGE_READONLY,&protection))return 1;
 try{runtime_gnm_draw_auto(base,7,3,0);}catch(...){++rejected;}
 if(!VirtualProtect(mapped,4096,protection,&protection) || rejected!=5 || !std::equal(saved.begin(),saved.end(),words))return 1;
 direct_memory_backing=old;std::cout<<"{\"scope\":\"gnm_draw_auto_guest_control\",\"SysV4\":true,\"negative_rejections\":5,\"tail_preserved\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
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
inline int gnm_update_vs_guest_control(){
 auto old=direct_memory_backing;auto owner=std::make_shared<DirectMemoryBacking>(16384);direct_memory_backing=owner;
 int64_t physical=-1;void* mapped=nullptr;
 if(owner->allocate(0,16384,16384,0,0,&physical) || owner->map(&mapped,16384,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(mapped);auto* words=static_cast<uint32_t*>(mapped);
 const uint32_t source[7]={0x1234,0,0xdeadbeef,0x11,0x22,0x33,0x44};
 std::memcpy(words+128,source,sizeof(source));std::fill(words,words+40,0xabcdef01);
 if(ctr_gnm_update_vs_bridge_fixture(base,29,base+512,0x3000080))return 1;
 if(words[0]!=0xc0027600 || words[2]!=source[0] || words[6]!=((source[2]&0xfcfffc3f)|0x3000080) || words[17]!=0xc00a1000 || words[18])return 1;
 if(words[8]!=0xc0011000 || words[9]!=0xc01e0207 || words[12]!=0xc01e01b1 || words[15]!=0xc01e01c3)return 1;
 for(unsigned i=19;i<40;++i)if(words[i]!=0xabcdef01)return 1;
 const std::vector<uint32_t> saved(words,words+40);unsigned rejected=0;
 for(const auto size:{0u,28u})if(runtime_gnm_update_vs(base,size,base+512,0)==-1)++rejected;
 if(runtime_gnm_update_vs(base,29,base+512,1)==-1)++rejected;
 words[129]=1;if(runtime_gnm_update_vs(base,29,base+512,0)==-1)++rejected;words[129]=0;
 for(const auto destination:{base+16384-4,UINT64_MAX-3})try{runtime_gnm_update_vs(destination,29,base+512,0);}catch(...){++rejected;}
 try{runtime_gnm_update_vs(base,29,base+16384-4,0);}catch(...){++rejected;}
 try{runtime_gnm_update_vs(base,29,base+4,0);}catch(...){++rejected;}
 DWORD protection=0;if(!VirtualProtect(mapped,4096,PAGE_READONLY,&protection))return 1;
 try{runtime_gnm_update_vs(base,29,base+512,0);}catch(...){++rejected;}
 if(!VirtualProtect(mapped,4096,protection,&protection))return 1;
 if(rejected!=9 || !std::equal(saved.begin(),saved.end(),words))return 1;
 direct_memory_backing=old;
 std::cout<<"{\"scope\":\"gnm_update_vs_guest_control\",\"SysV4\":true,\"negative_rejections\":9,\"preserved_trailing_words\":true,\"synthetic\":true,\"gpu_execution\":false}\n";return 0;
}
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
inline int gnm_dispatch_guest_control(){
 auto old=direct_memory_backing;auto owner=std::make_shared<DirectMemoryBacking>(16384);direct_memory_backing=owner;
 int64_t physical=-1;void* mapped=nullptr;if(owner->allocate(0,16384,16384,0,0,&physical)||owner->map(&mapped,16384,51,0,physical,0))return 1;
 const auto base=reinterpret_cast<uint64_t>(mapped);auto* w=static_cast<uint32_t*>(mapped);std::fill(w,w+16,0xabcdef01);
 if(ctr_gnm_dispatch_direct_fixture(base,9,240,135,1,0))return 1;
 const uint32_t expected[9]={0xc0031502,240,135,1,1,0xc0021000,0,0xabcdef01,0xabcdef01};
 if(!std::equal(expected,expected+9,w))return 1;for(unsigned i=9;i<16;++i)if(w[i]!=0xabcdef01)return 1;
 if(ctr_gnm_dispatch_direct_fixture(base,9,3,4,5,0x19) || w[0]!=0xc0031503 || w[1]!=3 || w[2]!=4 || w[3]!=5 || w[4]!=25)return 1;
 const std::vector<uint32_t> saved(w,w+16);unsigned rejected=0;
 if(runtime_gnm_dispatch_direct(base,8,1,2,3,0)==-1)++rejected;
 if(runtime_gnm_dispatch_direct(base,9,0x80000000,2,3,0)==-1)++rejected;
 try{runtime_gnm_dispatch_direct(base+16384-4,9,1,2,3,0);}catch(...){++rejected;}
 if(rejected!=3 || !std::equal(saved.begin(),saved.end(),w))return 1;direct_memory_backing=old;
 std::cout<<"{\"scope\":\"gnm_dispatch_guest_control\",\"SysV6\":true,\"all_arguments\":true,\"negative_cases\":3,\"gpu_execution\":false,\"passed\":true}\n";return 0;
}
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
