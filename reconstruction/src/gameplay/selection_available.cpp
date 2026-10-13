// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/selection_available.h"
#include <stdexcept>
namespace ctr {
bool selection_available(const GameplayHandle* handle,std::int32_t selection,
                         std::uint32_t input,SelectionOwnerQuery query,void* context){
 bool available=true;
 if(gameplay_handle_payload(handle)){
  const auto* p=static_cast<const SelectionAvailablePayload*>(gameplay_handle_payload(handle));
  if(!p)throw std::invalid_argument("Missing second selection payload");
  available=p->blocked==0;
 }
 if(selection!=-1)return false;
 if((input&255u)==0){
  if(!query)throw std::invalid_argument("Missing original owner virtual+b0 query");
  if(query(context))return false;
 }
 return available;
}
}
