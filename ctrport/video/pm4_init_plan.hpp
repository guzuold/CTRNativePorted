// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "pm4_decode.hpp"
#include "pm4_registers.hpp"
namespace CtrPm4 {
struct InitOperation {
    uint32_t opcode;
    std::vector<uint32_t> payload; // Owned snapshot, never persisted or guessed.
};
// Pure preparation. Execute barriers/EOP only after complete validation, in order.
// No Submit success, guest memory write, IRQ or GPU completion is implied.
inline int PrepareInit(const uint32_t* data,size_t words,std::vector<InitOperation>& output) {
    std::vector<Packet> packets;
    const int status=DecodePackets(data,words,packets);if(status) return status;
    std::vector<InitOperation> prepared;
    for(const auto& packet:packets) {
        const auto* p=data+packet.word;
        if(p[0]&255) return 88; // Predication/compute header unsupported in this route.
        switch(packet.opcode) {
        case 0x10:if(packet.words>2 && (p[1]&0xffff0000)==0x68750000 && !(p[1]==0x68753000 && packet.words==5)) return 88;break; // Ordinary padding; special flip/debug markers remain explicit.
        case 0x12:if(packet.words!=2 || p[1]) return 88;break;
        case 0x28:if(packet.words!=3 || p[1]!=0x80000000 || p[2]!=0x80000000) return 88;break;
        case 0x2f:if(packet.words!=2) return 60;break;
        case 0x3c:
            if(packet.words!=7)return 60;
            if((p[1]&~0x17u) || (p[1]&7u)==7 || p[6]!=10)return 88;
            if(p[1]&16u) {if(p[2]&3u)return 60;}
            else if(p[3] || p[2]>=0xD000)return 60;
            break;
        case 0x47: {
            Eop eop{};const int e=DecodeEop(p,packet.words,eop);if(e) return e;
            if(eop.eventType!=4 || eop.eventIndex!=5 || eop.dataSelect!=2 || eop.interruptSelect!=0) return 88;
            if(eop.address&7) return 60;
            break;
        }
        case 0x58:if(packet.words!=7 || p[3] || p[4] || p[5] || p[6]!=10 ||
            !((p[1]==2294546432U && p[2]==1) || (p[1]==784629696U && p[2]==UINT32_MAX))) return 88;break;
        case 0x69:case 0x76:case 0x79: {
            if(packet.words<3 || p[1]>65535) return 60;
            const uint32_t base=packet.opcode==0x69 ? 0xA000 : packet.opcode==0x76 ? 0x2C00 : 0xC000;
            const uint32_t end=packet.opcode==0x69 ? 0xB000 : packet.opcode==0x76 ? 0x2E00 : 0xD000;
            if(packet.opcode==0x76 && p[1]>=0x200) {
                if(p[1]>=0x250 || packet.words-2>0x250-p[1]) return 88;
            } else if(p[1]>=end-base || packet.words-2>end-base-p[1]) return 88;
            break;
        }
        default:return 88;
        }
        prepared.push_back({packet.opcode,std::vector<uint32_t>(p+1,p+packet.words)});
    }
    output.swap(prepared);return 0;
}
// Applies ONLY validated CPU register operations. Caller must execute other
// ordered operations through genuine GPU/memory authority before Submit returns.
inline int ApplyRegister(const InitOperation& operation,Registers& registers) {
    const auto& p=operation.payload;
    if(operation.opcode==0x12) {if(p.size()!=1 || p[0]) return 60;registers.SetDefaults();return 0;}
    if(operation.opcode==0x2f) {if(p.size()!=1) return 60;registers.words[0xC24D]=p[0];return 0;}
    uint32_t base=0,end=0;
    switch(operation.opcode) {
    case 0x69:base=0xA000;end=0xB000;break;
    case 0x76:base=0x2C00;end=0x2E00;break;
    case 0x79:base=0xC000;end=0xD000;break;
    default:return 88;
    }
    if(p.size()<2) return 60;
    if(operation.opcode==0x76 && p[0]>=0x200) {
        if(p[0]>=0x250 || p.size()-1>0x250-p[0]) return 60;
        std::memcpy(&registers.compute[p[0]-0x200],p.data()+1,(p.size()-1)*sizeof(uint32_t));return 0;
    }
    if(p[0]>=end-base || p.size()-1>end-base-p[0]) return 60;
    std::memcpy(&registers.words[base+p[0]],p.data()+1,(p.size()-1)*sizeof(uint32_t));return 0;
}
}
