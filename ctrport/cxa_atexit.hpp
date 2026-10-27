// SPDX-License-Identifier: GPL-2.0-only
// Independent Itanium C++ ABI 3.3.6 destruction API implementation.
// https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor
#pragma once
#include <vector>
namespace ctrport {
class CxaDestructors {
 struct Entry{void* function;uint64_t argument;void* dso;bool claimed=false;};
 std::mutex mutex;std::recursive_mutex finalization;std::vector<Entry> entries;
 static void validate(void* function){MEMORY_BASIC_INFORMATION info{};
  if(!function || !VirtualQuery(function,&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&PAGE_GUARD) || ((info.Protect&0xff)!=PAGE_EXECUTE_READ && (info.Protect&0xff)!=PAGE_EXECUTE_READWRITE))throw std::runtime_error("C++ destructor is not executable");}
public:
 int add(void* function,uint64_t argument,void* dso){validate(function);std::lock_guard<std::mutex> lock(mutex);try{entries.push_back({function,argument,dso});}catch(const std::bad_alloc&){return -1;}return 0;}
 void finalize(void* dso){
  std::lock_guard<std::recursive_mutex> serial(finalization);
  for(;;){Entry selected{};bool found=false;
   {std::lock_guard<std::mutex> lock(mutex);for(auto it=entries.rbegin();it!=entries.rend();++it)if(!it->claimed && (!dso || it->dso==dso)){it->claimed=true;selected=*it;found=true;break;}}
   if(!found)return;validate(selected.function);
   (void)ctr_call_original_integer3(selected.function,selected.argument,0,0);
  }
 }
};
inline CxaDestructors cxa_destructors;
extern "C" int ctr_cxa_atexit(void* function,uint64_t argument,void* dso,uint64_t caller){
 try{int result=cxa_destructors.add(function,argument,dso);std::printf("{\"scope\":\"original_cxa_atexit_return\",\"caller\":%llu,\"result\":%d,\"destructor_retained\":%s}\n",static_cast<unsigned long long>(caller),result,result==0?"true":"false");std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=tsvEmnenz48 library=libSceLibcInternal caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_cxa_finalize(void* dso,uint64_t caller){try{cxa_destructors.finalize(dso);}catch(const std::exception& e){std::fprintf(stderr,"NID=H2e8t5ScQGc library=libSceLibcInternal caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}}
extern "C" void ctr_sysv_cxa_atexit();
extern "C" void ctr_sysv_cxa_finalize();
extern "C" void ctr_sysv_cxa_destructor_control();
inline std::vector<uint64_t> cxa_destructor_observations;
extern "C" void ctr_cxa_destructor_observe(uint64_t argument){cxa_destructor_observations.push_back(argument);}
}
