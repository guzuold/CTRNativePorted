// SPDX-License-Identifier: GPL-2.0-only
// Independent implementation. PS4 lifecycle: flatz/ps4_remote_pkg_installer/http.c.
// Host lifecycle: Microsoft Learn Getting Schannel Credentials. No socket is opened.
#pragma once
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <security.h>
#include <schannel.h>
#pragma comment(lib,"secur32.lib")
namespace ctrport {
class SslContexts {
 struct Context {
  void* pool=nullptr; size_t capacity=0; CredHandle credential{}; bool acquired=false;
  ~Context(){if(acquired)FreeCredentialsHandle(&credential);if(pool)VirtualFree(pool,0,MEM_RELEASE);}
 };
 std::mutex mutex;std::map<int,std::unique_ptr<Context>> contexts;int next=1;
public:
 bool contains(int id){std::lock_guard<std::mutex> lock(mutex);return contexts.find(id)!=contexts.end();}
 int create(size_t bytes){
  if(!bytes)throw std::runtime_error("SSL pool size zero unsupported");
  auto context=std::make_unique<Context>();context->capacity=bytes;
  context->pool=VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
  if(!context->pool)throw std::runtime_error("SSL pool allocation failed");
  SCHANNEL_CRED config{};config.dwVersion=SCHANNEL_CRED_VERSION;
  config.dwFlags=SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_AUTO_CRED_VALIDATION;
  TimeStamp expiry{};
  auto status=AcquireCredentialsHandleA(nullptr,const_cast<char*>(UNISP_NAME_A),SECPKG_CRED_OUTBOUND,nullptr,&config,nullptr,nullptr,&context->credential,&expiry);
  if(status!=SEC_E_OK)throw std::runtime_error("Schannel credential acquisition failed status="+std::to_string(static_cast<long>(status)));
  context->acquired=true;
  std::lock_guard<std::mutex> lock(mutex);
  if(next==INT_MAX)throw std::runtime_error("SSL context IDs exhausted");
  int id=next++;contexts.emplace(id,std::move(context));return id;
 }
 void destroy(int id){std::lock_guard<std::mutex> lock(mutex);if(!contexts.erase(id))throw std::runtime_error("Unknown SSL context ID");}
 bool control(int id,size_t bytes){std::lock_guard<std::mutex> lock(mutex);auto it=contexts.find(id);if(it==contexts.end() || it->second->capacity!=bytes || !it->second->acquired)return false;auto* p=static_cast<unsigned char*>(it->second->pool);p[0]=0x27;p[bytes-1]=0x91;return p[0]==0x27 && p[bytes-1]==0x91;}
};
inline SslContexts ssl_contexts;
extern "C" int ctr_ssl_init(size_t bytes,uint64_t caller){try{int id=ssl_contexts.create(bytes);std::printf("{\"scope\":\"original_SslInit_return\",\"caller\":%llu,\"pool_size\":%llu,\"result\":%d,\"owned_Schannel_credential\":true,\"socket_opened\":false}\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(bytes),id);std::fflush(stdout);return id;}catch(const std::exception& e){std::fprintf(stderr,"NID=hdpVEUDFW3s library=libSceSsl caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" int ctr_ssl_term(int id,uint64_t caller){try{ssl_contexts.destroy(id);return 0;}catch(const std::exception& e){std::fprintf(stderr,"NID=0K1yQ6Lv-Yc library=libSceSsl caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_sysv_ssl_init();
extern "C" void ctr_sysv_ssl_term();
inline int ssl_context_control(){
 int id=static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_ssl_init),32768,0));
 if(id<=0 || !ssl_contexts.control(id,32768))return 1;
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_ssl_term),id,0))return 2;
 bool stale=false;try{ssl_contexts.destroy(id);}catch(...){stale=true;}if(!stale)return 3;
 int second=ssl_contexts.create(4097);if(second==id || !ssl_contexts.control(second,4097))return 4;ssl_contexts.destroy(second);
 bool invalid=false;try{ssl_contexts.create(0);}catch(...){invalid=true;}if(!invalid)return 5;
 std::cout<<"{\"scope\":\"ssl_context_control\",\"passed\":true,\"actual_SysV_lifecycle\":true,\"owned_Schannel_credential\":true,\"owned_pool\":true,\"stale_ID_rejected\":true,\"network_connections\":0,\"TLS_handshake_verified\":false}\n";return 0;
}
}
