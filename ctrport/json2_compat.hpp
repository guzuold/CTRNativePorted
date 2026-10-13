// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <iostream>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <stdexcept>
inline constexpr const char* ctr_declared_compat_nids[]={"Cxwy7wHq4J0","PR5k1penBLM","cK6bYHf-Q5E","RujUxbr3haM","i-XwZjw0OOY","-hJRce8wn1U","OcAgPxcq5Vk","g8cM39EUZ6o","Nlev7Lg8k3A","gky0+oaNM4k","g8cM39EUZ6o","g8cM39EUZ6o","g8cM39EUZ6o","g8cM39EUZ6o","isruqthpYcw","g8cM39EUZ6o","kvYEw2lBndk","g8cM39EUZ6o","g8cM39EUZ6o","+MCXJlWdi+s","co2NCj--pnc"};
inline constexpr unsigned ctr_declared_compat_count=sizeof(ctr_declared_compat_nids)/sizeof(ctr_declared_compat_nids[0]);
inline int ctr_json2_compat(unsigned index,std::uint64_t caller) {
 const auto& nids=ctr_declared_compat_nids;
 if(index>=ctr_declared_compat_count) throw std::runtime_error("Unknown compatibility stub");
 static std::mutex log_mutex;
 std::lock_guard<std::mutex> guard(log_mutex);
 const char* library=(index==7 || index==10 || index==11 || index==12 || index==13 || index==15 || index==17 || index==18)?"libSceSysmodule":index==8?"libSceNet":index==9?"libSceNetCtl":(index==14 || index==19 || index==20)?"libSceSharePlay":index==16?"libSceGameLiveStreaming":"libSceJson2";
 char line[768];
 const int length=std::snprintf(line,sizeof(line),"{\"scope\":\"declared_compatibility_stub\",\"tag\":\"R8\",\"nid\":\"%s\",\"library\":\"%s\",\"caller\":%llu,\"registry_index\":%u,\"returned_value\":0}\n",nids[index],library,static_cast<unsigned long long>(caller),index);
 struct CanonicalLog {
  FILE* file=nullptr;
  CanonicalLog(){if(fopen_s(&file,"runtime/user/logs/compat_calls.jsonl","wb") || !file){std::fprintf(stderr,"Compatibility canonical log unavailable\n");ExitProcess(4);}}
  ~CanonicalLog(){if(file)std::fclose(file);}
 };
 static CanonicalLog canonical;
 if(length<=0 || static_cast<size_t>(length)>=sizeof(line) || std::fwrite(line,1,static_cast<size_t>(length),canonical.file)!=static_cast<size_t>(length) || std::fflush(canonical.file)){
  std::fprintf(stderr,"Compatibility call logging failed NID=%s library=%s caller=0x%llx\n",nids[index],library,static_cast<unsigned long long>(caller));ExitProcess(4);
 }
 std::fwrite(line,1,static_cast<size_t>(length),stdout);std::fflush(stdout);return 0;
}
