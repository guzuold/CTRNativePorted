// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <iostream>
#include <future>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "owned_guest_copy.hpp"
namespace ctrport {
// Own checked-memory adapter; packet behavior from shadPS4 v0.18.0
// revision e3ce810f3a653f43ac64ebab63023de281a4103a
// src/core/libraries/gnmdriver/gnmdriver.cpp:1150 sceGnmInsertWaitFlipDone
// GPL-2.0-or-later. WAIT_REG_MEM waits for the actual VideoOut slot label zero.
inline int insert_wait_flip(const CtrGuestCopyApi& memory,uint64_t destination,uint32_t size,uint64_t labels,uint32_t slot){
 if(size!=7)return -1;
 if(!labels || slot>=16 || labels>UINT64_MAX-128)return 88;
 const auto address=(labels+slot*8)&~uint64_t(3);
 const std::array<uint32_t,7> packet{0xc0053c00,0x13,static_cast<uint32_t>(address),static_cast<uint32_t>(address>>32),0,0xffffffff,10};
 return memory.write(memory.context,destination,packet.data(),sizeof(packet))?112:0;
}
}
