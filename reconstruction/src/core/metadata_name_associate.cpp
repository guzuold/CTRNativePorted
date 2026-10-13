// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_name_associate.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void metadata_name_associate(std::span<std::byte,24> metadata,
 std::uintptr_t name_context,const char* name,
 const MetadataNameOriginal& intern,const MetadataNameOriginal& release){
 if(!intern)throw std::runtime_error("Missing original name intern14a74c0");
 auto value=intern(name_context,reinterpret_cast<std::uintptr_t>(name));
 std::uintptr_t previous{};std::memcpy(&previous,metadata.data()+16,8);
 if(previous){
  if(!release)throw std::runtime_error("Missing original name release144df00");
  release(name_context,previous-16u);
 }
 std::memcpy(metadata.data()+16,&value,8);
}
}
