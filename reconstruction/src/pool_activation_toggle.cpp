#include "ctr/pool_activation_toggle.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void toggle_nonthreaded_pool_activation(std::span<std::byte> s,std::uint64_t base,
 std::int32_t flag,std::uint8_t global_selector){
 if(s.size()!=608)throw std::invalid_argument("Resolved guarded608 layout required");
 if(!flag){s[208+0x28]=std::byte{0};return;}
 if(s[64+0x6c]!=std::byte{0})throw std::invalid_argument("Threaded pool handles missing");
 auto resolve=[&](std::size_t manager_offset){
  std::uint64_t token;std::memcpy(&token,s.data()+64+manager_offset,8);
  if(token<base||token-base> s.size()||64>s.size()-(token-base))throw std::invalid_argument("Resolved pool64 token required");
  const auto offset=static_cast<std::size_t>(token-base);
  if(offset!=288&&offset!=352&&offset!=416&&offset!=480)throw std::invalid_argument("Pool outside resolved fixture layout");
  return offset;
 };
 const auto first=resolve(global_selector==1?0x28:0x18);
 std::uint32_t refs;std::memcpy(&refs,s.data()+208+8,4);const auto type=refs>>22;
 const auto second=resolve(type==1?0x10:type==2?0x20:type==3?0x28:0x18);
 const auto byte=std::byte{static_cast<std::uint8_t>(flag)};
 s[first+0x38]=byte;s[second+0x38]=std::byte{1};s[208+0x28]=byte;
}
}
