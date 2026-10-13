// SPDX-License-Identifier: GPL-2.0-only
// Independent empty library context lifecycle, informed by pinned shadPS4
// np_web_api.cpp initialize/terminate and np_web_api_internal.cpp createLibraryContext.
// Owned local user contexts and unsent request metadata; no PSN identity or network operations.
#pragma once
namespace ctrport {
class NpWebContexts {
 struct Request{std::string group,path,type;std::uint64_t length=0,id=0;int method=0;bool sent=false;};
 struct UserContext{int user=0,id=0;std::map<std::uint64_t,std::unique_ptr<Request>> requests;};
 struct Context{std::map<int,std::unique_ptr<UserContext>> users;int http=0;size_t capacity=0;void* pool=nullptr;~Context(){if(pool)VirtualFree(pool,0,MEM_RELEASE);}};
 std::mutex mutex;std::map<int,std::unique_ptr<Context>> contexts;int next=1,next_user=0;std::uint64_t next_request=0;
public:
 int create(int http,size_t bytes){
  if(!http_contexts.contains(http) || !bytes)throw std::runtime_error("NP WebApi requires live owned HTTP context and positive pool size");
  auto c=std::make_unique<Context>();c->http=http;c->capacity=bytes;c->pool=VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!c->pool)throw std::runtime_error("NP WebApi pool allocation failed");
  std::lock_guard<std::mutex> lock(mutex);if(next>=0x8000)throw std::runtime_error("NP WebApi context ID wrap unsupported");int id=next++;contexts.emplace(id,std::move(c));return id;
 }
 int create_user(int library,int user){
  if(library>=0x8000)return int(0x80552903U);
  if(user==-1)return int(0x80552902U);
  std::lock_guard<std::mutex> lock(mutex);const auto found=contexts.find(library);
  if(found==contexts.end())return int(0x80552904U);
  for(const auto& entry:found->second->users)if(entry.second->user==user)return int(0x8055290aU);
  if(next_user>=0xffff)throw std::runtime_error("NP user context counter wrap unsupported");
  auto object=std::make_unique<UserContext>();object->user=user;object->id=(library<<16)|(++next_user);const int id=object->id;
  found->second->users.emplace(id,std::move(object));return id;
 }
 bool owns_user(int library,int id,int user){std::lock_guard<std::mutex> lock(mutex);const auto parent=contexts.find(library);if(parent==contexts.end())return false;const auto child=parent->second->users.find(id);return child!=parent->second->users.end() && child->second->user==user;}
 static std::string read_text(const char* source){std::string result;for(size_t i=0;i<4096;++i){char c=0;SIZE_T read=0;if(!ReadProcessMemory(GetCurrentProcess(),source+i,&c,1,&read) || read!=1)throw std::runtime_error("NP request string inaccessible");if(!c)return result;result.push_back(c);}throw std::runtime_error("NP request string exceeds supported4096-byte bound");}
 struct Content{std::uint64_t length;const char* type;unsigned char reserved[16];};
 int create_request(int user,const char* group,const char* path,int method,const Content* content,std::uint64_t* output){
  if(!group || !path || method<0 || method>3)return int(0x80552902U);
  Content snapshot{};if(content){SIZE_T bytes=0;if(!ReadProcessMemory(GetCurrentProcess(),content,&snapshot,sizeof(snapshot),&bytes) || bytes!=sizeof(snapshot))throw std::runtime_error("NP request content inaccessible");if(snapshot.length && !snapshot.type)return int(0x80552908U);}
  const auto group_text=read_text(group),path_text=read_text(path);const auto type_text=snapshot.type?read_text(snapshot.type):std::string{};
  if(output)require_time_output(output,8);
  std::lock_guard<std::mutex> lock(mutex);const auto parent=contexts.find(user>>16);if(parent==contexts.end())return int(0x80552904U);
  const auto child=parent->second->users.find(user);if(child==parent->second->users.end())return int(0x80552905U);
  if(next_request>=0xffffffffULL)throw std::runtime_error("NP request counter wrap unsupported");
  auto object=std::make_unique<Request>();object->group=group_text;object->path=path_text;object->type=type_text;object->length=snapshot.length;object->method=method;object->id=(static_cast<std::uint64_t>(user)<<32)|(++next_request);const auto id=object->id;
  child->second->requests.emplace(id,std::move(object));if(output)*output=id;return 0;
 }
 bool owns_request(int user,std::uint64_t id,const char* path){std::lock_guard<std::mutex> lock(mutex);const auto p=contexts.find(user>>16);if(p==contexts.end())return false;const auto c=p->second->users.find(user);if(c==p->second->users.end())return false;const auto r=c->second->requests.find(id);return r!=c->second->requests.end() && r->second->path==path;}
 int send_offline(std::uint64_t request){
  std::lock_guard<std::mutex> lock(mutex);const auto parent=contexts.find(static_cast<int>(request>>48));if(parent==contexts.end())return int(0x80552904U);
  const auto child=parent->second->users.find(static_cast<int>(request>>32));if(child==parent->second->users.end())return int(0x80552905U);
  const auto found=child->second->requests.find(request);if(found==child->second->requests.end())return int(0x80552906U);
  // Public sendRequest670-776: initial timeout is0; mark sent, then fail absent sign-in.
  found->second->sent=true;return int(0x80552907U);
 }
 // Independent offline-owned branch of pinned np_web_api_internal.cpp1016-1070.
 // Send is synchronous and never creates HTTP resources: no outstanding worker to abort.
 int delete_request(std::uint64_t request){
  std::lock_guard<std::mutex> lock(mutex);const auto parent=contexts.find(static_cast<int>(request>>48));if(parent==contexts.end())return int(0x80552904U);
  const auto child=parent->second->users.find(static_cast<int>(request>>32));if(child==parent->second->users.end())return int(0x80552905U);
  if(!child->second->requests.erase(request))return int(0x80552906U);return 0;
 }
 bool request_attempted(int user,std::uint64_t request){std::lock_guard<std::mutex> lock(mutex);const auto p=contexts.find(user>>16);if(p==contexts.end())return false;const auto c=p->second->users.find(user);if(c==p->second->users.end())return false;const auto r=c->second->requests.find(request);return r!=c->second->requests.end() && r->second->sent;}
 void destroy(int id){std::lock_guard<std::mutex> lock(mutex);if(!contexts.erase(id))throw std::runtime_error("NP WebApi unknown context ID");}
 bool control(int id,size_t bytes){std::lock_guard<std::mutex> lock(mutex);auto it=contexts.find(id);if(it==contexts.end() || it->second->capacity!=bytes)return false;auto* p=static_cast<unsigned char*>(it->second->pool);p[0]=0x25;p[bytes-1]=0x81;return p[0]==0x25 && p[bytes-1]==0x81;}
};
inline NpWebContexts np_web_contexts;
extern "C" int ctr_np_web_init(int http,size_t bytes,uint64_t caller){try{int id=np_web_contexts.create(http,bytes);std::printf("{\"scope\":\"original_NpWebApiInitialize_return\",\"caller\":%llu,\"http_id\":%d,\"pool_size\":%llu,\"result\":%d,\"owned_empty_library_context\":true,\"connections\":0}\n",static_cast<unsigned long long>(caller),http,static_cast<unsigned long long>(bytes),id);std::fflush(stdout);return id;}catch(const std::exception& e){std::fprintf(stderr,"NID=G3AnLNdRBjE library=libSceNpWebApi caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" int ctr_np_web_term(int id,uint64_t caller){try{np_web_contexts.destroy(id);return 0;}catch(const std::exception& e){std::fprintf(stderr,"NID=asz3TtIqGF8 library=libSceNpWebApi caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" int ctr_np_web_create_user(int library,int user,std::uint64_t caller){
 try{const int result=np_web_contexts.create_user(library,user);std::printf("{\"scope\":\"owned_np_web_user_context\",\"nid\":\"zk6c65xoyO0\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"parent_id\":%d,\"user_id\":%d,\"result\":%d,\"connections\":0}\n",static_cast<unsigned long long>(caller),library,user,result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=zk6c65xoyO0 library=libSceNpWebApi caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_np_web_create_user();
inline int np_web_user_control(){
 int net=net_pools.create("NpUserControl",32768,0),ssl=ssl_contexts.create(32768),http=http_contexts.create(net,ssl,32768),parent=np_web_contexts.create(http,32768);
 auto call=[&](int p,int u){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_web_create_user),p,u));};
 const int first=call(parent,1),second=call(parent,2);
 if(first<=0 || second<=0 || first==second || (first>>16)!=parent || !np_web_contexts.owns_user(parent,first,1) || !np_web_contexts.owns_user(parent,second,2))return 1;
 if(call(parent,1)!=int(0x8055290aU) || call(parent,-1)!=int(0x80552902U) || call(0x8000,1)!=int(0x80552903U) || call(-1,1)!=int(0x80552904U))return 1;
 np_web_contexts.destroy(parent);if(np_web_contexts.owns_user(parent,first,1) || call(parent,1)!=int(0x80552904U))return 1;
 http_contexts.destroy(http);ssl_contexts.destroy(ssl);net_pools.destroy(net);
 std::cout<<"{\"scope\":\"np_web_user_control\",\"SysV2\":true,\"owned_children\":2,\"duplicate_invalid_stale\":true,\"parent_destroys_children\":true,\"connections\":0}\n";return 0;
}
extern "C" int ctr_np_web_create_request(int user,const char* group,const char* path,int method,const NpWebContexts::Content* content,std::uint64_t* output,std::uint64_t caller){
 try{const int result=np_web_contexts.create_request(user,group,path,method,content,output);std::printf("{\"scope\":\"owned_np_web_request\",\"nid\":\"rdgs5Z1MyFw\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"user_context\":%d,\"method\":%d,\"result\":%d,\"request_id\":%llu,\"connections\":0}\n",static_cast<unsigned long long>(caller),user,method,result,static_cast<unsigned long long>(!result && output?*output:0));std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=rdgs5Z1MyFw library=libSceNpWebApi caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_np_web_create_request();
extern "C" void ctr_sysv_np_request_fixture();
inline int np_web_request_control(){
 int net=net_pools.create("NpRequestControl",32768,0),ssl=ssl_contexts.create(32768),http=http_contexts.create(net,ssl,32768),parent=np_web_contexts.create(http,32768),user=np_web_contexts.create_user(parent,1);
 char group[]="authored",path[]="/offline";std::uint64_t output=123;
 std::array<std::uint64_t,6> args{static_cast<std::uint64_t>(user),reinterpret_cast<std::uint64_t>(group),reinterpret_cast<std::uint64_t>(path),0,0,reinterpret_cast<std::uint64_t>(&output)};
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_request_fixture),reinterpret_cast<std::uint64_t>(args.data()),0) || output>>32!=static_cast<std::uint64_t>(user) || !np_web_contexts.owns_request(user,output,"/offline"))return 1;
 const auto first=output;path[1]='X';if(!np_web_contexts.owns_request(user,first,"/offline"))return 1;
 NpWebContexts::Content invalid{1,nullptr,{}};output=123;
 if(np_web_contexts.create_request(user,group,path,0,&invalid,&output)!=int(0x80552908U) || output!=123 || np_web_contexts.create_request(user,nullptr,path,0,nullptr,&output)!=int(0x80552902U))return 1;
 np_web_contexts.destroy(parent);if(np_web_contexts.owns_request(user,first,"/offline") || np_web_contexts.create_request(user,group,path,0,nullptr,&output)!=int(0x80552904U) || output!=123)return 1;
 http_contexts.destroy(http);ssl_contexts.destroy(ssl);net_pools.destroy(net);
 std::cout<<"{\"scope\":\"np_request_control\",\"SysV6\":true,\"owned_request_snapshot\":true,\"negative_output_untouched\":true,\"parent_teardown\":true,\"connections\":0}\n";return 0;
}
extern "C" int ctr_np_web_send_offline(std::uint64_t request,const void*,std::uint64_t,void*,std::uint64_t caller){
 try{const auto result=np_web_contexts.send_offline(request);std::printf("{\"scope\":\"owned_np_web_offline_send\",\"nid\":\"KjNeZ-29ysQ\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"request_id\":%llu,\"result\":%d,\"signed_in\":false,\"connections\":0}\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(request),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=KjNeZ-29ysQ library=libSceNpWebApi caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_np_web_send_offline();
extern "C" void ctr_sysv_np_send_fixture();
inline int np_web_send_control(){
 int net=net_pools.create("NpSendControl",32768,0),ssl=ssl_contexts.create(32768),http=http_contexts.create(net,ssl,32768),parent=np_web_contexts.create(http,32768),user=np_web_contexts.create_user(parent,1);std::uint64_t request=0;
 if(np_web_contexts.create_request(user,"authored","/offline",0,nullptr,&request))return 1;
 std::array<std::uint64_t,4> args{request,1,16,1}; // deliberately inaccessible body/response: offline gate never reads/writes them.
 if(static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_send_fixture),reinterpret_cast<std::uint64_t>(args.data()),0))!=int(0x80552907U) || !np_web_contexts.request_attempted(user,request) || np_web_contexts.send_offline(request)!=int(0x80552907U))return 1;
 if(np_web_contexts.send_offline(request+1)!=int(0x80552906U) || np_web_contexts.send_offline((static_cast<std::uint64_t>(parent)<<48)|(2ULL<<32)|1)!=int(0x80552905U))return 1;
 np_web_contexts.destroy(parent);if(np_web_contexts.send_offline(request)!=int(0x80552904U))return 1;
 http_contexts.destroy(http);ssl_contexts.destroy(ssl);net_pools.destroy(net);
 std::cout<<"{\"scope\":\"np_send_control\",\"SysV4\":true,\"not_signed_in\":true,\"request_sent_flag\":true,\"invalid_owner_errors\":true,\"body_response_unaccessed\":true,\"connections\":0}\n";return 0;
}
extern "C" int ctr_np_web_delete_request(std::uint64_t request,std::uint64_t caller){
 try{const int result=np_web_contexts.delete_request(request);std::printf("{\"scope\":\"owned_np_web_delete_request\",\"nid\":\"noQgleu+KLE\",\"library\":\"libSceNpWebApi\",\"caller\":%llu,\"request_id\":%llu,\"result\":%d,\"connections\":0}\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(request),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=noQgleu+KLE library=libSceNpWebApi caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_np_web_delete_request();
inline int np_web_delete_control(){
 int net=net_pools.create("NpDeleteControl",32768,0),ssl=ssl_contexts.create(32768),http=http_contexts.create(net,ssl,32768),parent=np_web_contexts.create(http,32768),user=np_web_contexts.create_user(parent,1);std::uint64_t first=0,second=0;
 if(np_web_contexts.create_request(user,"authored","/first",0,nullptr,&first) || np_web_contexts.create_request(user,"authored","/second",0,nullptr,&second))return 1;
 auto call=[&](std::uint64_t id){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_web_delete_request),id,0));};
 if(np_web_contexts.send_offline(first)!=int(0x80552907U) || call(first) || np_web_contexts.owns_request(user,first,"/first") || !np_web_contexts.owns_request(user,second,"/second"))return 1;
 if(call(first)!=int(0x80552906U) || np_web_contexts.send_offline(first)!=int(0x80552906U) || call((static_cast<std::uint64_t>(parent)<<48)|(2ULL<<32)|1)!=int(0x80552905U))return 1;
 if(call(second))return 1;np_web_contexts.destroy(parent);if(call(second)!=int(0x80552904U))return 1;
 http_contexts.destroy(http);ssl_contexts.destroy(ssl);net_pools.destroy(net);
 std::cout<<"{\"scope\":\"np_delete_control\",\"SysV1\":true,\"sent_and_unsent_release\":true,\"sibling_preserved\":true,\"double_delete_and_stale_rejected\":true,\"connections\":0}\n";return 0;
}
extern "C" void ctr_sysv_np_web_init();
extern "C" void ctr_sysv_np_web_term();
inline int np_web_context_control(){
 int net=net_pools.create("NpWebControl",32768,0),ssl=ssl_contexts.create(32768),http=http_contexts.create(net,ssl,32768);
 int id=static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_web_init),http,1048576));if(id<=0 || !np_web_contexts.control(id,1048576))return 1;
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_web_term),id,0))return 2;
 bool stale=false;try{np_web_contexts.destroy(id);}catch(...){stale=true;}if(!stale)return 3;
 for(int variant=0;variant<2;++variant){bool rejected=false;try{np_web_contexts.create(variant==0?-1:http,variant==1?0:4097);}catch(...){rejected=true;}if(!rejected)return 4;}
 int second=np_web_contexts.create(http,4097);if(second==id || !np_web_contexts.control(second,4097))return 5;np_web_contexts.destroy(second);http_contexts.destroy(http);ssl_contexts.destroy(ssl);net_pools.destroy(net);
 std::cout<<"{\"scope\":\"np_web_context_control\",\"passed\":true,\"actual_SysV_lifecycle\":true,\"owned_pool\":true,\"stale_and_invalid_dependencies_rejected\":true,\"connections\":0,\"user_identity_or_requests_supported\":false}\n";return 0;
}
}
