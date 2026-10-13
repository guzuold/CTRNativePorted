// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_instance_create.h"
#include <stdexcept>
namespace ctr {
std::uintptr_t metadata_instance_create(std::uintptr_t source,std::uintptr_t context,
 std::uint32_t mode,const OriginalIntegerCall2& type_query,
 const OriginalIntegerCall3& factory,const OriginalIntegerCall2& initialize,
 const OriginalIntegerCall2& initialize_alternate){
 if(!type_query||!factory)throw std::runtime_error("Missing original type query or instance factory");
 auto type=type_query(source,0);
 auto result=factory(type,context,0);
 if(result&&mode==0){
  if(!initialize)throw std::runtime_error("Missing original instance initialize13dc290");
  initialize(result,source);
 }else if(result&&mode==1){
  if(!initialize_alternate)throw std::runtime_error("Missing original instance initialize13dc100");
  initialize_alternate(result,source);
 }
 return result;
}
}
