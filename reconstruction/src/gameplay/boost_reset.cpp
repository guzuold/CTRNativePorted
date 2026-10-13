#include "ctr/gameplay/boost_reset.h"
#include "ctr/core/handle_initializer.h"
#include <array>
#include <cstring>
#include <stdexcept>
namespace ctr {
void reset_boost(std::span<std::byte> timer,std::span<const std::byte> source,
 std::span<const std::byte> data,int now){
 if(timer.size()<60)throw std::invalid_argument("Timer missing");
 std::array<std::byte,8> local{};initialize_handle(local,source,{},0);
 unsigned long long old;std::memcpy(&old,timer.data()+32,8);
 if(old)throw std::runtime_error("Missing old handle binding and terminal lifetime");
 std::memcpy(timer.data()+32,local.data(),8);
 if(!data.empty()){
  if(data.size()<60)throw std::invalid_argument("Boost data missing");
  std::memcpy(timer.data()+40,data.data()+32,4);std::memcpy(timer.data()+44,data.data()+40,4);
  timer[48]=data[59];int finished=-1;std::memcpy(timer.data()+52,&finished,4);std::memcpy(timer.data()+56,&now,4);
 }
}
}
