// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/vertical_shutdown.h"
#include <stdexcept>
namespace ctr {
void shutdown_vertical_state(bool& active,VerticalResetHandle*& slot,
 VerticalTerminalRelease terminal_release,void* context){
 if(!active)return;
 reset_vertical_payload(slot);
 if(slot){
  const auto before=slot->flags;
  const auto threshold=(before&0x90000000u)?3u:2u;
  if((before&0xffffu)==threshold&&!terminal_release)
   throw std::invalid_argument("Missing genuine terminal vertical handle release014da170");
  --slot->flags; // Borrowed single-thread view of original LOCK XADD(-1).
  if((before&0xffffu)==threshold)terminal_release(slot,context);
  slot=nullptr;
 }
 slot=nullptr;
 active=false;
}
}
