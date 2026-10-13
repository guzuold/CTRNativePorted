// SPDX-License-Identifier: GPL-2.0-only
// Independent offline connection query. Public shadPS4 netctl.cpp342-349,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Runtime owns no NetCtl interface/session; host connectivity is not guest connectivity.
#pragma once
namespace ctrport {
inline int net_connection_state(int* output){
 if(!output)return int(0x80412107U);
 require_time_output(output,4);*output=0;return 0; // DISCONNECTED; all connect APIs remain missing.
}
extern "C" int ctr_net_connection_state(int* output,std::uint64_t caller){
 try{const int result=net_connection_state(output);std::printf("{\"scope\":\"owned_netctl_connection_state\",\"nid\":\"uBPlr0lbuiI\",\"library\":\"libSceNetCtl\",\"caller\":%llu,\"result\":%d,\"state\":%d,\"guest_connections\":0}\n",static_cast<unsigned long long>(caller),result,result?-1:*output);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=uBPlr0lbuiI library=libSceNetCtl caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_net_connection_state();
inline int net_connection_state_control(){
 struct {int before=123,state=91,after=456;} frame;
 auto call=[&](int* output){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_net_connection_state),reinterpret_cast<std::uint64_t>(output),0));};
 if(call(&frame.state) || frame.state!=0 || frame.before!=123 || frame.after!=456 || call(nullptr)!=int(0x80412107U))return 1;
 void* blocked=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!blocked)return 1;bool rejected=false;
 try{net_connection_state(static_cast<int*>(blocked));}catch(const std::exception&){rejected=true;}VirtualFree(blocked,0,MEM_RELEASE);if(!rejected)return 1;
 std::cout<<"{\"scope\":\"netctl_state_control\",\"SysV1\":true,\"disconnected\":true,\"sentinels\":true,\"null_and_noaccess_rejected\":true,\"connections\":0}\n";return 0;
}
}
