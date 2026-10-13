// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/contact_availability.h"
namespace ctr {
bool contact_available(HandleRecord*& slot){
 if(!slot)return false;
 // Original bc8ab2 bypasses canonical traversal after a rebind.
 if(slot->flags_and_count.load()&0x10000000U){
  rebind_stable_handle(slot);
  return slot&&slot->payload;
 }
 return inspect_handle_chain(slot)!=nullptr;
}
}
