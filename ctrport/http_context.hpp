// SPDX-License-Identifier: GPL-2.0-only
// Independent lifecycle implementation. PS4: flatz/ps4_remote_pkg_installer/http.c.
// Host: Microsoft Learn WinHttpOpen / WinHttpCloseHandle. No connect/request calls.
#pragma once
#include <winhttp.h>
#pragma comment(lib,"winhttp.lib")
namespace ctrport {
class HttpContexts {
 struct Context{void* pool=nullptr;size_t capacity=0;HINTERNET session=nullptr;int net=0,ssl=0;~Context(){if(session)WinHttpCloseHandle(session);if(pool)VirtualFree(pool,0,MEM_RELEASE);}};
 std::mutex mutex;std::map<int,std::unique_ptr<Context>> contexts;int next=1;
public:
 bool contains(int id){std::lock_guard<std::mutex> lock(mutex);return contexts.find(id)!=contexts.end();}
 int create(int net,int ssl,size_t bytes){
  if(!net_pools.contains(net) || !ssl_contexts.contains(ssl) || !bytes)throw std::runtime_error("HTTP requires live owned Net/SSL IDs and nonzero pool");
  auto c=std::make_unique<Context>();c->net=net;c->ssl=ssl;c->capacity=bytes;
  c->pool=VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!c->pool)throw std::runtime_error("HTTP pool allocation failed");
  c->session=WinHttpOpen(L"CTRPort",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
  if(!c->session)throw std::runtime_error("WinHTTP session allocation failed");
  std::lock_guard<std::mutex> lock(mutex);if(next==INT_MAX)throw std::runtime_error("HTTP IDs exhausted");int id=next++;contexts.emplace(id,std::move(c));return id;
 }
 void destroy(int id){std::lock_guard<std::mutex> lock(mutex);if(!contexts.erase(id))throw std::runtime_error("Unknown HTTP context ID");}
 bool control(int id,size_t bytes){std::lock_guard<std::mutex> lock(mutex);auto it=contexts.find(id);if(it==contexts.end() || it->second->capacity!=bytes || !it->second->session)return false;DWORD timeout=0,size=sizeof(timeout);if(!WinHttpQueryOption(it->second->session,WINHTTP_OPTION_CONNECT_TIMEOUT,&timeout,&size))return false;auto* p=static_cast<unsigned char*>(it->second->pool);p[0]=0x41;p[bytes-1]=0x92;return p[0]==0x41 && p[bytes-1]==0x92;}
};
inline HttpContexts http_contexts;
extern "C" int ctr_http_init(int net,int ssl,size_t bytes,uint64_t caller){try{int id=http_contexts.create(net,ssl,bytes);std::printf("{\"scope\":\"original_HttpInit_return\",\"caller\":%llu,\"net_id\":%d,\"ssl_id\":%d,\"pool_size\":%llu,\"result\":%d,\"owned_WinHTTP_session\":true,\"connections\":0}\n",static_cast<unsigned long long>(caller),net,ssl,static_cast<unsigned long long>(bytes),id);std::fflush(stdout);return id;}catch(const std::exception& e){std::fprintf(stderr,"NID=A9cVMUtEp4Y library=libSceHttp caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" int ctr_http_term(int id,uint64_t caller){try{http_contexts.destroy(id);return 0;}catch(const std::exception& e){std::fprintf(stderr,"NID=Ik-KpLTlf7Q library=libSceHttp caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_sysv_http_init();
extern "C" void ctr_sysv_http_term();
inline int http_context_control(){
 int net=net_pools.create("HttpControl",32768,0),ssl=ssl_contexts.create(32768);
 int id=static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_http_init),net,ssl,262144));
 if(id<=0 || !http_contexts.control(id,262144))return 1;
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_http_term),id,0))return 2;
 bool stale=false;try{http_contexts.destroy(id);}catch(...){stale=true;}if(!stale)return 3;
 for(int variant=0;variant<3;++variant){bool rejected=false;try{http_contexts.create(variant==0?-1:net,variant==1?-1:ssl,variant==2?0:4097);}catch(...){rejected=true;}if(!rejected)return 4;}
 int second=http_contexts.create(net,ssl,4097);if(second==id || !http_contexts.control(second,4097))return 5;http_contexts.destroy(second);ssl_contexts.destroy(ssl);net_pools.destroy(net);
 std::cout<<"{\"scope\":\"http_context_control\",\"passed\":true,\"actual_SysV_lifecycle\":true,\"real_WinHTTP_session_query\":true,\"invalid_dependencies_rejected\":true,\"connections\":0}\n";return 0;
}
}
