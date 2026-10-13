// SPDX-License-Identifier: GPL-2.0-only
// Public create/destroy lifecycle: flatz/ps4_remote_pkg_installer net.c.
// ABI/NIDs: pinned shadPS4 net.cpp; upstream dummy not reused.
#pragma once
namespace ctrport {
class NetPools {
 struct Pool{std::string name;size_t bytes;void* memory;~Pool(){VirtualFree(memory,0,MEM_RELEASE);}};
 std::mutex mutex;std::map<int,std::unique_ptr<Pool>> pools;int next=1;
public:
 bool contains(int id){std::lock_guard<std::mutex> lock(mutex);return pools.find(id)!=pools.end();}
 int create(const char* name,int bytes,int flags){
  if(!name || bytes<=0 || flags)throw std::runtime_error("Network pool requires readable name, positive size and flags0");
  std::string copied;for(size_t i=0;i<256;++i){char c;SIZE_T read=0;if(!ReadProcessMemory(GetCurrentProcess(),name+i,&c,1,&read) || read!=1)throw std::runtime_error("Network pool name unreadable");if(!c)break;copied.push_back(c);}
  if(copied.empty() || copied.size()==256)throw std::runtime_error("Network pool name empty or unsupported length");
  std::lock_guard<std::mutex> lock(mutex);if(next==INT_MAX)throw std::runtime_error("Network pool IDs exhausted");
  auto pool=std::make_unique<Pool>();pool->name=std::move(copied);pool->bytes=static_cast<size_t>(bytes);pool->memory=VirtualAlloc(nullptr,pool->bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!pool->memory)throw std::runtime_error("Network pool backing allocation failed");
  int id=next++;pools.emplace(id,std::move(pool));return id;
 }
 void destroy(int id){std::lock_guard<std::mutex> lock(mutex);if(!pools.erase(id))throw std::runtime_error("Unknown network pool ID");}
 bool backing_control(int id,size_t bytes){std::lock_guard<std::mutex> lock(mutex);auto it=pools.find(id);if(it==pools.end() || it->second->bytes!=bytes)return false;auto* p=static_cast<unsigned char*>(it->second->memory);p[0]=0x71;p[bytes-1]=0x93;return p[0]==0x71 && p[bytes-1]==0x93;}
};
inline NetPools net_pools;
extern "C" int ctr_net_pool_create(const char* name,int bytes,int flags,uint64_t caller){try{int result=net_pools.create(name,bytes,flags);std::printf("{\"scope\":\"original_NetPoolCreate_return\",\"caller\":%llu,\"size\":%d,\"flags\":%d,\"result\":%d,\"real_owned_backing\":true}\n",static_cast<unsigned long long>(caller),bytes,flags,result);std::fflush(stdout);return result;}catch(const std::exception& e){std::fprintf(stderr,"NID=dgJBaeJnGpo library=libSceNet caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" int ctr_net_pool_destroy(int id,uint64_t caller){try{net_pools.destroy(id);return 0;}catch(const std::exception& e){std::fprintf(stderr,"NID=K7RlrTkI-mw library=libSceNet caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_sysv_net_pool_create();
extern "C" void ctr_sysv_net_pool_destroy();
inline int net_pool_control(){
 constexpr char name[]="CTRPoolControl";int id=static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_net_pool_create),reinterpret_cast<uint64_t>(name),32768,0));
 if(id<=0 || !net_pools.backing_control(id,32768))return 1;
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_net_pool_destroy),id,0))return 2;
 bool stale=false;try{net_pools.destroy(id);}catch(...){stale=true;}if(!stale)return 3;
 int second=net_pools.create(name,4097,0);if(second==id || !net_pools.backing_control(second,4097))return 4;net_pools.destroy(second);
 for(int variant=0;variant<3;++variant){bool invalid=false;try{net_pools.create(variant==0?nullptr:name,variant==1?0:8,variant==2?1:0);}catch(...){invalid=true;}if(!invalid)return 5;}
 std::cout<<"{\"scope\":\"net_pool_control\",\"passed\":true,\"actual_SysV_lifecycle\":true,\"real_capacity_backing\":true,\"stale_ID_rejected\":true,\"network_transport_supported\":false}\n";return 0;
}
}
