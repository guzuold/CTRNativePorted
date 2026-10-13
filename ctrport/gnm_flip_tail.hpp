// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <iostream>
namespace ctrport {
// Packet construction portion of the whole public PatchFlipRequest function,
// engine/shadPS4/src/core/libraries/gnmdriver/gnmdriver.cpp:2088-2174,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// This is a plan on an owned copy, NOT flip admission or GPU completion.
// The caller must admit a real EOP flip before publishing this plan and must
// preserve ordered WriteData / PatchedFlip / EOP execution. No runtime binding.
inline int plan_flip_tail(const uint32_t* source,size_t words,uint64_t labels,
 uint32_t slot,std::array<uint32_t,64>& output){
 if(!source || words<64 || !labels || (labels&3u) || slot==UINT32_MAX)return 60;
 if(labels>std::numeric_limits<uint64_t>::max()-uint64_t(slot)*8)return 60;
 source+=words-64;
 if(source[0]!=0xc03e1000u)return 60;
 const auto marker=source[1];
 if(marker!=0x68750777u && marker!=0x68750778u && marker!=0x68750780u && marker!=0x68750781u)return 88;
 if(marker==0x68750778u && (source[2]&3u))return 60;
 std::array<uint32_t,64> plan;std::copy_n(source,64,plan.begin());
 const auto address=labels+uint64_t(slot)*8;
 plan[0]=0xc0033700u;plan[1]=0x500;plan[2]=uint32_t(address);plan[3]=uint32_t(address>>32);plan[4]=1;
 plan[6]=0x68750776u;
 if(marker==0x68750777u)plan[5]=0xc0391000u;
 else if(marker==0x68750778u){
  plan[5]=0xc0341000u;plan[59]=0xc0033700u;plan[60]=0x500;
  plan[61]=source[2]&~3u;plan[62]=source[3];plan[63]=source[4];
 }else{
  plan[5]=0xc0331000u;plan[58]=0xc0044700u;
  plan[59]=(source[5]&0x3fu)+0x500u+(source[6]&0x3fu)*0x1000u;
  const bool label=marker==0x68750781u;
  plan[60]=label?(source[2]&~3u):0;
  plan[61]=label?((source[3]&0xffffu)|0x22000000u):0x01000000u;
  plan[62]=label?source[4]:0;plan[63]=0;
 }
 output=plan;return 0;
}
inline int gnm_flip_tail_control(){
 std::array<uint32_t,68> source{};source.fill(0xabcdef01u);
 source[4]=0xc03e1000u;source[6]=0x12345678;source[7]=0x1234;source[8]=0x98765432;
 source[9]=0x7f;source[10]=0x43;
 const uint32_t markers[]={0x68750777u,0x68750778u,0x68750780u,0x68750781u};
 for(unsigned i=0;i<4;++i){
  source[5]=markers[i];const auto original=source;std::array<uint32_t,64> plan{};
  if(plan_flip_tail(source.data(),source.size(),0x123456780000ull,3,plan) || source!=original)return 1;
  if(plan[0]!=0xc0033700u || plan[1]!=0x500 || plan[2]!=0x56780018 || plan[3]!=0x1234 || plan[4]!=1 || plan[6]!=0x68750776u)return 2;
  if(i==0 && (plan[5]!=0xc0391000u || plan[63]!=0xabcdef01u))return 3;
  if(i==1 && (plan[5]!=0xc0341000u || plan[59]!=0xc0033700u || plan[60]!=0x500 || plan[61]!=0x12345678 || plan[62]!=0x1234 || plan[63]!=0x98765432))return 4;
  if(i>=2 && (plan[5]!=0xc0331000u || plan[58]!=0xc0044700u || plan[59]!=0x353f || plan[63]))return 5;
  if(i==2 && (plan[60] || plan[61]!=0x01000000u || plan[62]))return 6;
  if(i==3 && (plan[60]!=0x12345678 || plan[61]!=0x22001234 || plan[62]!=0x98765432))return 7;
 }
 std::array<uint32_t,64> result{};result.fill(0x55);const auto unchanged=result;
 source[5]=0x68750778u;source[6]|=1;
 if(plan_flip_tail(source.data(),68,0x1000,0,result)!=60 || result!=unchanged)return 8;
 source[6]&=~3u;source[5]=0x68750779u;
 if(plan_flip_tail(source.data(),68,0x1000,0,result)!=88 || result!=unchanged)return 9;
 source[5]=0x68750777u;
 if(plan_flip_tail(source.data(),63,0x1000,0,result)!=60 || plan_flip_tail(source.data(),68,UINT64_MAX-3,1,result)!=60 || plan_flip_tail(source.data(),68,0x1000,UINT32_MAX,result)!=60 || result!=unchanged)return 10;
 std::cout<<"{\"scope\":\"public_prepare_flip_tail_plan\",\"passed\":true,\"four_marker_branches\":true,\"input_immutable\":true,\"failure_output_unchanged\":true,\"synthetic\":true,\"runtime_bound\":false,\"GPU_execution\":false,\"flip_admission\":false}\n";return 0;
}
}
