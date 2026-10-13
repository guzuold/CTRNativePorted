// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Original image reads require retained ownership and immutable Windows protection.
namespace ctrport {
struct MarkerCopyOwner {
 std::shared_ptr<DirectMemoryBacking> direct;
 std::shared_ptr<std::vector<MappedImage>> images;
 std::vector<ImageMetadata> metadata;
};
inline int marker_read(void* context,uint64_t address,void* output,uint64_t size) noexcept {
 auto& owner=*static_cast<MarkerCopyOwner*>(context);
 if(!direct_copy_read(owner.direct.get(),address,output,size))return 0;
 if(!size || size>65536 || address>UINT64_MAX-size || !owner.images)return 1;
 for(size_t i=0;i<owner.images->size() && i<owner.metadata.size();++i){
  const auto base=reinterpret_cast<uint64_t>((*owner.images)[i].get());
  if(address<base)continue;const auto offset=address-base;
  for(const auto& span:owner.metadata[i].spans){
   if(offset<span.first || offset>=span.second || size>span.second-offset)continue;
   MEMORY_BASIC_INFORMATION region{};
   if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)))return 1;
   const auto start=reinterpret_cast<uint64_t>(region.BaseAddress);
   if(region.State!=MEM_COMMIT || (region.Protect!=PAGE_READONLY && region.Protect!=PAGE_EXECUTE_READ) || address-start>region.RegionSize || size>region.RegionSize-(address-start))return 1;
   SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,static_cast<SIZE_T>(size),&copied) && copied==size ? 0:1;
  }
 }
 return 1;
}
inline int marker_write(void* context,uint64_t address,const void* input,uint64_t size) noexcept {
 return direct_copy_write(static_cast<MarkerCopyOwner*>(context)->direct.get(),address,input,size);
}
inline int runtime_gnm_push_marker(uint64_t command,uint32_t size,uint64_t marker){
 MarkerCopyOwner owner{direct_memory_backing,startup_worker_images,startup_worker_metadata};
 if(!owner.direct)throw std::runtime_error("PushMarker direct mapping owner missing");
 auto transaction=owner.direct->copy_transaction();
 const CtrGuestCopyApi memory{sizeof(CtrGuestCopyApi),1,&owner,marker_read,marker_write};
 int32_t result=123;const int host=ctr_gnm_insert_push_marker(&memory,command,size,marker,&result);
 if(host)throw std::runtime_error("PushMarker checked emitter host_status="+std::to_string(host));return result;
}
extern "C" int ctr_gnm_push_marker_bridge(uint64_t command,uint32_t size,uint64_t marker,uint64_t caller){
 try{const int result=runtime_gnm_push_marker(command,size,marker);
 std::printf("{\"scope\":\"original_GnmInsertPushMarker_return\",\"caller\":%llu,\"size_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),size,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=W1Etj-jlW7Y library=libSceGnmDriver caller=0x%llx marker=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(marker),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_push_marker();
inline int runtime_gnm_pop_marker(uint64_t command,uint32_t size){
 auto owner=direct_memory_backing;if(!owner)throw std::runtime_error("PopMarker owner missing");
 auto transaction=owner->copy_transaction();const auto memory=make_direct_copy_api(*owner);int32_t result=123;
 const int host=ctr_gnm_insert_pop_marker(&memory,command,size,&result);
 if(host)throw std::runtime_error("PopMarker checked emitter host_status="+std::to_string(host));return result;
}
extern "C" int ctr_gnm_pop_marker_bridge(uint64_t command,uint32_t size,uint64_t caller){
 try{const int result=runtime_gnm_pop_marker(command,size);
 std::printf("{\"scope\":\"original_GnmInsertPopMarker_return\",\"caller\":%llu,\"size_dwords\":%u,\"result\":%d,\"gpu_execution\":false}\n",static_cast<unsigned long long>(caller),size,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=7qZVNgEu+SY library=libSceGnmDriver caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_gnm_pop_marker();
inline int gnm_push_marker_control(){
 auto previous=direct_memory_backing;auto images_previous=startup_worker_images;auto meta_previous=startup_worker_metadata;
 auto direct=std::make_shared<DirectMemoryBacking>(16384);direct_memory_backing=direct;
 int64_t physical=-1;void* mapped=nullptr;if(direct->allocate(0,16384,16384,0,0,&physical)||direct->map(&mapped,16384,51,0,physical,0))return 1;
 auto images=std::make_shared<std::vector<MappedImage>>();images->emplace_back(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
 if(!images->front())return 1;std::memcpy(images->front().get(),"abc",4);DWORD old=0;if(!VirtualProtect(images->front().get(),4096,PAGE_READONLY,&old))return 1;
 ImageMetadata metadata;metadata.spans.emplace_back(0,4096);startup_worker_images=images;startup_worker_metadata={metadata};
 auto* words=static_cast<uint32_t*>(mapped);std::fill(words,words+16,0xabcdef01);
 const auto base=reinterpret_cast<uint64_t>(mapped),marker=reinterpret_cast<uint64_t>(images->front().get());
 if(runtime_gnm_push_marker(base,6,marker) || words[1]!=0x68750001 || std::memcmp(words+2,"abc",4))return 1;
 for(unsigned i=3;i<6;++i)if(words[i])return 1;for(unsigned i=6;i<16;++i)if(words[i]!=0xabcdef01)return 1;
 if(runtime_gnm_pop_marker(base,6))return 1;
 const uint32_t pop[6]={0xc0041000,0x68750002,0,0,0,0};if(!std::equal(pop,pop+6,words))return 1;
 unsigned rejected=0;try{runtime_gnm_push_marker(base,6,marker+4096);}catch(...){++rejected;}
 if(!VirtualProtect(images->front().get(),4096,PAGE_READWRITE,&old))return 1;
 try{runtime_gnm_push_marker(base,6,marker);}catch(...){++rejected;}
 try{runtime_gnm_push_marker(base+16384-4,6,base+512);}catch(...){++rejected;}
 direct_memory_backing=previous;startup_worker_images=images_previous;startup_worker_metadata=meta_previous;
 if(rejected!=3)return 1;
 std::cout<<"{\"scope\":\"push_marker_owned_source_control\",\"passed\":true,\"negative_rejections\":3,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
