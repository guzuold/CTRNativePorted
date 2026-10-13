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
inline int insert_wait_flip_control(){
 GuestStack stack(32768,4096);const auto memory=make_owned_guest_copy_api();const auto address=reinterpret_cast<uint64_t>(stack.bottom());
 std::array<uint32_t,8> result{};result.fill(0x79797979);if(memory.write(memory.context,address,result.data(),32))return 1;
 if(insert_wait_flip(memory,address,7,0x123456780,15) || memory.read(memory.context,address,result.data(),32))return 1;
 if(result!=std::array<uint32_t,8>{0xc0053c00,0x13,0x234567f8,1,0,0xffffffff,10,0x79797979})return 1;
 const auto before=result;
 if(insert_wait_flip(memory,address,6,0x123456780,15)!=-1 || insert_wait_flip(memory,address,7,0x123456780,16)!=88 || insert_wait_flip(memory,UINT64_MAX-4,7,0x123456780,0)!=112)return 1;
 if(memory.read(memory.context,address,result.data(),32) || result!=before)return 1;
 std::cout<<"{\"scope\":\"authored_InsertWaitFlip_packet\",\"passed\":true,\"negative_rejections\":3,\"sentinel_preserved\":true,\"GPU_wait_executed\":false}\n";return 0;
}
}
