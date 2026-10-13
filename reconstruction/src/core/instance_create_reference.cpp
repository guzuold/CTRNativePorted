// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_create_reference.h"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::byte* instance_create_reference(std::span<std::byte,8> output,std::uintptr_t source,
 std::uintptr_t context,std::uint32_t mode,const OriginalIntegerCall3& create,
 const OriginalIntegerCall2& release){
 if(!create)throw std::runtime_error("Missing original instance creation13db7f0");
 auto result=create(source,context,mode);std::memcpy(output.data(),&result,8);
 if(result){
  auto& refs=*reinterpret_cast<std::uint32_t*>(result+8);
  std::atomic_ref<std::uint32_t>(refs).fetch_add(1,std::memory_order_seq_cst);
  if(!release)throw std::runtime_error("Missing original instance release13d8710");
  release(result,0);
 }
 return output.data();
}
}
