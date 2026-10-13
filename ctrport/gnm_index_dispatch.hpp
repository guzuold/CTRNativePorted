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
inline int index_abi_control(){
 index_abi_control_active=true;
 const std::array<uint64_t,6> input{0x123456789abcdef0ULL,10,102,0xfedcba9876543210ULL,0x60000001,0xdeadbeef};
 const int result=ctr_gnm_draw_index_fixture(input[0],static_cast<uint32_t>(input[1]),static_cast<uint32_t>(input[2]),input[3],static_cast<uint32_t>(input[4]),static_cast<uint32_t>(input[5]));
 index_abi_control_active=false;
 if(result!=42 || !std::equal(input.begin(),input.end(),index_abi_capture.begin()) || !index_abi_capture[6])return 1;
 auto old=direct_memory_backing;auto owner=std::make_shared<DirectMemoryBacking>(4736ULL*1024*1024);direct_memory_backing=owner;
 int64_t physical=-1;void* mapped=nullptr;if(owner->allocate(0,16384,16384,0,0,&physical)||owner->map(&mapped,16384,51,0,physical,0))return 1;
 auto* words=static_cast<uint32_t*>(mapped);const auto base=reinterpret_cast<uint64_t>(mapped);std::fill(words,words+12,0xabcdef01);
 if(ctr_gnm_draw_index_fixture(base,10,102,0x123456789abcdef0ULL,0,1))return 1;
 const uint32_t expected[]={0xc0042700,102,0x9abcdef0,0x12345678,102,0,0xc0021000,0,0xabcdef01,0xabcdef01,0xabcdef01,0xabcdef01};
 if(!std::equal(std::begin(expected),std::end(expected),words))return 1;
 if(ctr_gnm_draw_index_fixture(base,9,102,2,0,1)!=-1 || ctr_gnm_draw_index_fixture(base,10,102,3,0,1)!=-1 || !std::equal(std::begin(expected),std::end(expected),words))return 1;
 direct_memory_backing=old;
 std::cout<<"{\"scope\":\"draw_index_SysV6_ABI_control\",\"passed\":true,\"fourth_argument_full64bits\":true,\"synthetic\":true,\"runtime_bound\":true}\n";return 0;
}
}
