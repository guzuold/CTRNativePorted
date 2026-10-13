// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video_label_memory.hpp"
#include "owned_guest_copy.hpp"
namespace ctrport {
// Pinned public PatchFlipRequest writes a 32-bit lock1 at label_base+slot*8.
// This bounded CPU publisher is not a fence or a notification mechanism.
class FlipSurfaceLock {
 CtrGuestCopyApi memory_{};uint64_t address_=0,argument_=0,completion_count_=0,completion_token_=0;bool locked_=false,completed_=false;
public:
 int acquire(const CtrGuestCopyApi& memory,uint64_t base,uint32_t slot,uint64_t argument){
  if(locked_ || completed_ || !base || (base&7) || slot>=16 || base>UINT64_MAX-128 || !memory.read || !memory.write)return 60;
  uint64_t old=UINT64_MAX;const auto address=base+uint64_t(slot)*8;
  if(memory.read(memory.context,address,&old,8))return 60;if(old)return 91;
  const uint32_t one=1;if(memory.write(memory.context,address,&one,4))return 60;
  memory_=memory;address_=address;argument_=argument;locked_=true;return 0;
 }
 // Completion identity comes from a successful real present_next on this session.
 // Failed/stale completion must retain lock1, preventing premature reuse.
 int complete(int present_status,uint64_t count,uint64_t token,uint64_t argument){
  if(!locked_ || completed_)return 91;
  if(present_status || !count || !token || argument!=argument_)return 88;
  uint64_t current=0;if(memory_.read(memory_.context,address_,&current,8))return 60;if(current!=1)return 91;
  // driver.cpp ProcessFlip271: current frame remains locked. A later
  // successful frame retires this PREVIOUS slot, not its own current slot.
  completion_count_=count;completion_token_=token;completed_=true;return 0;
 }
 int retire_previous(int status,uint64_t later_count,uint64_t later_token){
  if(!locked_ || !completed_)return 91;
  if(status || later_count<=completion_count_ || !later_token || later_token==completion_token_)return 88;
  uint64_t current=0;if(memory_.read(memory_.context,address_,&current,8))return 60;if(current!=1)return 91;
  const uint64_t zero=0;if(memory_.write(memory_.context,address_,&zero,8))return 60;
  locked_=false;return 0;
 }
};
inline int flip_surface_lock_control(){
 auto owner=std::make_shared<VideoLabelMemory>();const auto memory=make_owned_guest_copy_api();const auto base=owner->address();
 FlipSurfaceLock lock;uint64_t values[16]{};if(lock.acquire(memory,base,15,77))return 1;
 if(memory.read(memory.context,base,values,128) || values[15]!=1)return 2;for(unsigned i=0;i<15;++i)if(values[i])return 2;
 FlipSurfaceLock duplicate;if(duplicate.acquire(memory,base,15,77)!=91)return 3;
 for(auto entry:std::array<std::array<uint64_t,4>,4>{{{{88,1,1,77}},{{0,0,1,77}},{{0,1,0,77}},{{0,1,1,78}}}}){
  if(lock.complete(static_cast<int>(entry[0]),entry[1],entry[2],entry[3])!=88 || memory.read(memory.context,base+120,&values[15],8) || values[15]!=1)return 4;
 }
 if(lock.complete(0,1,1,77) || lock.complete(0,1,1,77)!=91 || memory.read(memory.context,base+120,&values[15],8) || values[15]!=1)return 5;
 if(lock.retire_previous(0,1,2)!=88 || lock.retire_previous(0,2,1)!=88 || lock.retire_previous(88,2,2)!=88)return 8;
 FlipSurfaceLock next;if(next.acquire(memory,base,14,78) || next.complete(0,2,2,78) || lock.retire_previous(0,2,2))return 9;
 if(memory.read(memory.context,base,values,128) || values[15] || values[14]!=1)return 10;
 FlipSurfaceLock malformed;if(malformed.acquire(memory,base,16,0)!=60 || malformed.acquire(memory,base+1,0,0)!=60)return 6;
 owner.reset();if(malformed.acquire(memory,base,0,0)!=60)return 7;
 std::cout<<"{\"scope\":\"flip_surface_lock_control\",\"passed\":true,\"actual_owned_memory\":true,\"failed_completion_retains_lock\":true,\"first_completion_keeps_current_locked\":true,\"later_completion_retires_previous\":true,\"neighbors_unchanged\":true,\"revoked_owner_rejected\":true,\"GPU_completion\":false,\"synthetic\":true}\n";return 0;
}
}
