// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <mutex>
#include <iostream>
namespace ctrport {
// Own transaction design; public contract: shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a
// gnmdriver.cpp2313, liverpool.h73 / liverpool.cpp139 (GPL-2.0-or-later).
// A receipt is NOT GPU idle. Only a genuine owner-thread provider may finish it.
class GnmFrameEnd {
 std::mutex mutex_; uint64_t count_=0,token_=0,thread_=0,frames_=0,argument_=0; bool pending_=false,running_=false;
public:
 int publish(uint64_t count,uint64_t token,uint64_t thread,bool commands,bool resources,bool present,uint64_t argument=0){
  std::lock_guard<std::mutex> lock(mutex_);
  if(!count || !token || !thread || !commands || !resources || !present)return 60;
  if(pending_ || running_)return 91;
  if(count<=count_)return 88;
  count_=count;token_=token;thread_=thread;argument_=argument;pending_=true;return 0;
 }
 template<class Provider> int finish(uint64_t thread,Provider&& provider){
  uint64_t count=0,token=0;
  {std::lock_guard<std::mutex> lock(mutex_);if(!pending_)return 62;if(running_)return 91;if(thread!=thread_)return 116;running_=true;count=count_;token=token_;}
  int status=112;try{status=provider(count,token);}catch(...){std::lock_guard<std::mutex> lock(mutex_);running_=false;throw;}
  std::lock_guard<std::mutex> lock(mutex_);running_=false;
  if(status)return status;
  pending_=false;++frames_;return 0;
 }
 struct Snapshot {uint64_t count,token,thread,frames,argument;bool pending,running;};
 Snapshot snapshot(){std::lock_guard<std::mutex> lock(mutex_);return {count_,token_,thread_,frames_,argument_,pending_,running_};}
 uint64_t frames(){std::lock_guard<std::mutex> lock(mutex_);return frames_;}
};
inline GnmFrameEnd original_gnm_frame_end;
inline int gnm_frame_end_control(){
 GnmFrameEnd state;unsigned calls=0,rejected=0;
 auto success=[&](uint64_t count,uint64_t token){++calls;return count==1 && token==7?0:88;};
 if(state.finish(4,success)==62)++rejected;
 if(state.publish(1,7,4,false,true,true)==60)++rejected;
 if(state.publish(1,7,4,true,true,true))return 1;
 if(state.publish(2,8,4,true,true,true)==91)++rejected;
 if(state.finish(5,success)==116)++rejected;
 if(state.finish(4,[&](uint64_t,uint64_t){++calls;return 88;})!=88 || state.frames())return 1;
 if(state.finish(4,[&](uint64_t,uint64_t){if(state.finish(4,success)!=91)return 112;return 116;})!=116 || state.frames())return 1;
 try{state.finish(4,[](uint64_t,uint64_t)->int{throw 1;});return 1;}catch(int){}
 if(state.finish(4,success) || state.frames()!=1 || calls!=2)return 1;
 if(state.finish(4,success)==62)++rejected;
 if(state.publish(1,9,4,true,true,true)==88)++rejected;
 if(state.publish(2,8,4,true,true,true) || state.finish(4,[](uint64_t count,uint64_t token){return count==2 && token==8?0:88;}) || state.frames()!=2 || rejected!=6)return 1;
 std::cout<<"{\"scope\":\"authored_Gnm_frame_end_transaction\",\"passed\":true,\"negative_rejections\":6,\"provider_failure_atomic\":true,\"reentrant_rejected\":true,\"exception_atomic\":true,\"frames\":2,\"real_GPU_idle\":false,\"guest_success_bound\":false}\n";return 0;
}
}
