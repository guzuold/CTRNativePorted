// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/field_alignment.h"
#include <cstring>
namespace ctr {
std::uint32_t field_alignment_default8(std::span<std::byte,36> field) {
 std::uint32_t flags{};
 std::memcpy(&flags,field.data()+32,4);
 if ((flags&0x3fc00u)==0) {
  flags=(flags&0xffbc03ffu)|0x402000u;
  std::memcpy(field.data()+32,&flags,4);
 }
 return (flags>>10)&255u;
}
}
