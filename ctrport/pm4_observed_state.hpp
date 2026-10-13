// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video/pm4_init_plan.hpp"
namespace ctrport {
// Diagnostic register observation only. Whole public SetConfigReg and IndexType
// semantics from shadPS4 liverpool.cpp/regs.cpp at
// e3ce810f3a653f43ac64ebab63023de281a4103a; no queue or draw execution.
inline int observe_pm4_register(const CtrPm4::InitOperation& op,CtrPm4::Registers& registers) {
 const auto& p=op.payload;
 if(op.opcode==0x68){
  if(p.size()<2 || p[0]>0xffff || p[0]>=0xb000 || p.size()-1>0xb000-p[0])return 60;
  std::memcpy(registers.words.data()+0x2000+p[0],p.data()+1,(p.size()-1)*sizeof(uint32_t));return 0;
 }
 if(op.opcode==0x2a){if(p.size()!=1)return 60;registers.words[0xa29f]=p[0];return 0;}
 return CtrPm4::ApplyRegister(op,registers);
}
inline int observed_pm4_control(){
 auto registers=std::make_unique<CtrPm4::Registers>();registers->words.fill(0x5a5a5a5a);
 if(observe_pm4_register({0x68,{0x240,0x12345678,0xabcdef01}},*registers) ||
 registers->words[0x2240]!=0x12345678 || registers->words[0x2241]!=0xabcdef01 ||
 registers->words[0x223f]!=0x5a5a5a5a || registers->words[0x2242]!=0x5a5a5a5a)return 1;
 if(observe_pm4_register({0x68,{0xa242,4}},*registers) || registers->words[0xc242]!=4)return 1;
 if(observe_pm4_register({0x2a,{0x80000003}},*registers) || registers->words[0xa29f]!=0x80000003)return 1;
 if(observe_pm4_register({0x2f,{7}},*registers) || registers->words[0xc24d]!=7 || registers->words[0xa29f]!=0x80000003)return 1;
 auto before=std::make_unique<CtrPm4::Registers>(*registers);unsigned rejected=0;
 for(const auto& op:std::vector<CtrPm4::InitOperation>{{0x68,{}},{0x68,{0}},{0x68,{0xb000,1}},{0x68,{0xafff,1,2}},{0x68,{0x10000000,1}},{0x2a,{}},{0x2a,{1,2}}}) {
  if(observe_pm4_register(op,*registers))++rejected;
  if(registers->words!=before->words || registers->compute!=before->compute)return 1;
 }
 if(rejected!=7)return 1;
 std::cout<<"{\"scope\":\"observed_pm4_register_control\",\"passed\":true,\"negative_atomic\":7,\"IndexType_distinct_NumInstances\":true,\"synthetic\":true,\"GPU_execution\":false}\n";return 0;
}
}
