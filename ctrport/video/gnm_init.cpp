// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted whole sceGnmDrawInitDefaultHardwareState350 and trivial internal helpers.
// Source revision e3ce810f3a653f43ac64ebab63023de281a4103a.
#include "gnm_init.h"
#include "../gnm_init_transport.h"
#include <type_traits>
static_assert(std::is_same_v<decltype(&ctr_gnm_draw_init_350),CtrGnmInit350Transport>,"infra transport ABI exact");
#include "../../engine/shadPS4/src/core/libraries/gnmdriver/gnmdriver_init.h"
#include <array>
#include <vector>
#include <cstring>
extern "C" int ctr_gnm_draw_init_350(const CtrGuestCopyApi* memory,uint64_t address,
    uint32_t capacity_dwords,int neo_mode,uint32_t* result) {
    if(!result || (neo_mode!=0 && neo_mode!=1)) return 60;
    if(capacity_dwords<Libraries::GnmDriver::HwInitPacketSize) { *result=0;return 0; }
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write) return 60;
    std::array<uint32_t,256> words;
    // Public code leaves trailing NOP payload untouched. Preserve destination bytes.
    if(memory->read(memory->context,address,words.data(),sizeof(words))) return 112;
    constexpr uint32_t clear[]={0xc0012800u,0x80000000u,0x80000000u,0xc0001200u,0u,
        0xc0055800u,0x2ec47fc0u,0xffffffffu,0u,0u,0u,10u};
    std::memcpy(words.data(),clear,sizeof(clear));
    size_t used=12;
    if(neo_mode) {
        const auto& sequence=Libraries::GnmDriver::InitSequence350Neo;
        std::memcpy(words.data()+used,sequence.data()+2,(sequence.size()-2)*4);
        used+=sequence.size()-2;
    } else {
        const auto& sequence=Libraries::GnmDriver::InitSequence350;
        std::memcpy(words.data()+used,sequence.data()+2,(sequence.size()-2)*4);
        used+=sequence.size()-2;
    }
    const auto payload=words.size()-used-1;
    words[used]=0xc0001000u | (static_cast<uint32_t>(payload-1)<<16);
    words[used+1]=0;
    if(memory->write(memory->context,address,words.data(),sizeof(words))) return 112;
    *result=Libraries::GnmDriver::HwInitPacketSize;return 0;
}

// Whole public sceGnmSetVsShader, same pinned gnmdriver.cpp revision/license.
#include "gnm_shader_emit.h"
extern "C" int ctr_gnm_set_vs_shader(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t capacity,uint64_t registers,uint32_t modifier,int32_t* result) {
    if(!result)return 60;
    if(!command || capacity<=28 || !registers || (modifier&0xfcfffc3fU)) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-116 || registers>UINT64_MAX-28)return 112;
    if(command<registers+28 && registers<command+116)return 88; // Alias domain not observed; never substitute snapshot semantics.
    std::array<uint32_t,7> source{};
    if(memory->read(memory->context,registers,source.data(),sizeof(source)))return 112;
    if(source[1]) {*result=-1;return 0;}
    std::array<uint32_t,29> destination;
    if(memory->read(memory->context,command,destination.data(),sizeof(destination)))return 112;
    const uint32_t modified=modifier ? (source[2]&0xfcfffc3fU)|modifier : source[2];
    const uint32_t emitted[]={0xc0027600,0x48,source[0],0,
        0xc0027600,0x4a,modified,source[3],
        0xc0016900,0x207,source[6],0xc0016900,0x1b1,source[4],
        0xc0016900,0x1c3,source[5],0xc00a1000,0};
    std::memcpy(destination.data(),emitted,sizeof(emitted));
    // Original initializes first NOP payload only; remaining10DWORD untouched.
    if(memory->write(memory->context,command,destination.data(),sizeof(destination)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmSetPsShader350, same pinned revision/license.
extern "C" int ctr_gnm_set_ps_shader_350(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t capacity,uint64_t registers,int32_t* result) {
    if(!result)return 60;
    if(!command || capacity<=39) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-160 || registers>UINT64_MAX-48)return 112;
    if(registers && command<registers+48 && registers<command+160)return 88;
    std::array<uint32_t,12> s{};
    if(registers && memory->read(memory->context,registers,s.data(),sizeof(s)))return 112;
    if(registers && s[1]) {*result=-1;return 0;}
    std::array<uint32_t,40> d;
    if(memory->read(memory->context,command,d.data(),sizeof(d)))return 112;
    if(registers) {
        const uint32_t words[]={0xc0027600,8,s[0],0,0xc0027600,10,s[2],s[3],
            0xc0026900,0x1c4,s[4],s[5],0xc0026900,0x1b3,s[6],s[7],
            0xc0016900,0x1b6,s[8],0xc0016900,0x1b8,s[9],
            0xc0016900,0x203,s[10],0xc0016900,0x8f,s[11],0xc00a1000,0};
        std::memcpy(d.data(),words,sizeof(words));
    } else {
        const uint32_t words[]={0xc0027600,8,0,0,0xc0016900,0x203,0,
            0xc0016900,0x8f,15,0xc01c1000,0};
        std::memcpy(d.data(),words,sizeof(words));
    }
    if(memory->write(memory->context,command,d.data(),sizeof(d)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmDrawIndexAuto, same pinned revision/license. Emission only.
extern "C" int ctr_gnm_draw_index_auto(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint32_t count,uint32_t flags,int neo,int32_t* result) {
    if(!result || (neo!=0 && neo!=1))return 60;
    if(!command || size!=7 || (flags&0x1ffffffeU)) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-28)return 112;
    std::array<uint32_t,7> d;
    if(memory->read(memory->context,command,d.data(),sizeof(d)))return 112;
    d[0]=0xc0012d00;d[1]=count;d[2]=neo ? (flags&0xe0000000U)|2U : 2U;
    d[3]=0xc0021000;d[4]=0; // Remaining2NOP payload DWORD preserved.
    if(memory->write(memory->context,command,d.data(),sizeof(d)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmUpdateVsShader, same pinned revision/license.
extern "C" int ctr_gnm_update_vs_shader(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t capacity,uint64_t registers,uint32_t modifier,int32_t* result) {
    if(!result)return 60;
    if(!command || capacity<=28 || !registers || (modifier&0xfcfffc3fU)) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-116 || registers>UINT64_MAX-28)return 112;
    if(command<registers+28 && registers<command+116)return 88; // Alias domain not observed; never substitute snapshot semantics.
    std::array<uint32_t,7> source{};
    if(memory->read(memory->context,registers,source.data(),sizeof(source)))return 112;
    if(source[1]) {*result=-1;return 0;}
    std::array<uint32_t,29> destination;
    if(memory->read(memory->context,command,destination.data(),sizeof(destination)))return 112;
    const uint32_t modified=modifier ? (source[2]&0xfcfffc3fU)|modifier : source[2];
    const uint32_t emitted[]={0xc0027600,0x48,source[0],0,
        0xc0027600,0x4a,modified,source[3],
        0xc0011000,0xc01e0207,source[6],0xc0011000,0xc01e01b1,source[4],
        0xc0011000,0xc01e01c3,source[5],0xc00a1000,0};
    std::memcpy(destination.data(),emitted,sizeof(emitted));
    // Original initializes first NOP payload only; remaining10DWORD untouched.
    if(memory->write(memory->context,command,destination.data(),sizeof(destination)))return 112;
    *result=0;return 0;
}


// Whole public sceGnmUpdatePsShader350, same pinned revision/license.
extern "C" int ctr_gnm_update_ps_shader_350(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t capacity,uint64_t registers,int32_t* result) {
    if(!result)return 60;
    if(!command || capacity<=39) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-160 || registers>UINT64_MAX-48)return 112;
    if(registers && command<registers+48 && registers<command+160)return 88;
    std::array<uint32_t,12> s{};
    if(registers && memory->read(memory->context,registers,s.data(),sizeof(s)))return 112;
    if(registers && s[1]) {*result=-1;return 0;}
    std::array<uint32_t,40> d;
    if(memory->read(memory->context,command,d.data(),sizeof(d)))return 112;
    if(registers) {
        const uint32_t words[]={0xc0027600,8,s[0],0,0xc0027600,10,s[2],s[3],
            0xc0021000,0xc01e01c4,s[4],s[5],0xc0021000,0xc01e01b3,s[6],s[7],
            0xc0011000,0xc01e01b6,s[8],0xc0011000,0xc01e01b8,s[9],
            0xc0011000,0xc01e0203,s[10],0xc0011000,0xc01e008f,s[11],0xc00a1000,0};
        std::memcpy(d.data(),words,sizeof(words));
    } else {
        const uint32_t words[]={0xc0027600,8,0,0,0xc0011000,0xc01e0203,0,
            0xc0016900,0x8f,15,0xc01c1000,0};
        std::memcpy(d.data(),words,sizeof(words));
    }
    if(memory->write(memory->context,command,d.data(),sizeof(d)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmSetVgtControl, same pinned revision/license.
extern "C" int ctr_gnm_set_vgt_control(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint32_t group,uint32_t partial,uint32_t eop,int neo,int compat,int32_t* result) {
    if(!result || (neo!=0 && neo!=1) || (compat!=0 && compat!=1))return 60;
    if(!command || size!=3 || group>=256 || (eop|partial)>=2) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    const uint32_t value=neo ? (eop ? ((partial&1)<<16)|group|0x140000U :
        (group&0x1cffffU)|0xd0000U) : ((partial&1)<<16)|(group&0xffffU);
    const uint32_t words[]={neo && !compat ? 0xc0017900U : 0xc0016900U,
        neo ? (compat ? 0x100002aaU : 0x40000258U) : 0x2aaU,
        neo && !compat ? value|0x600000U : value};
    if(memory->write(memory->context,command,words,sizeof(words)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmSetCsShaderWithModifier, same pinned revision/license.
extern "C" int ctr_gnm_set_cs_shader_modifier(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t capacity,uint64_t registers,uint32_t modifier,int32_t* result) {
    if(!result)return 60;
    if(!command || capacity<=24 || !registers || (modifier&0xfffffc3fU)) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-100 || registers>UINT64_MAX-28)return 112;
    if(command<registers+28 && registers<command+100)return 88; // Alias domain not observed; never substitute snapshot semantics.
    std::array<uint32_t,7> source{};
    if(memory->read(memory->context,registers,source.data(),sizeof(source)))return 112;
    if(source[1]) {*result=-1;return 0;}
    std::array<uint32_t,25> destination;
    if(memory->read(memory->context,command,destination.data(),sizeof(destination)))return 112;
    const uint32_t modified=modifier ? (source[2]&0xfffffc3fU)|modifier : source[2];
    const uint32_t emitted[]={0xc0027600,0x20c,source[0],0,
        0xc0027600,0x212,modified,source[3],
        0xc0037600,0x207,source[4],source[5],source[6],0xc00a1000,0};
    std::memcpy(destination.data(),emitted,sizeof(emitted));
    // Original initializes first NOP payload only; remaining10DWORD untouched.
    if(memory->write(memory->context,command,destination.data(),sizeof(destination)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmDispatchDirect, same pinned revision/license. Emission only.
extern "C" int ctr_gnm_dispatch_direct(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint32_t x,uint32_t y,uint32_t z,uint32_t flags,int32_t* result) {
    if(!result)return 60;
    if(!command || size!=9 || ((x|y|z)&0x80000000U)) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-36)return 112;
    std::array<uint32_t,9> d;
    if(memory->read(memory->context,command,d.data(),sizeof(d)))return 112;
    d[0]=0xc0031502U|(flags&1);d[1]=x;d[2]=y;d[3]=z;d[4]=(flags&0x18)+1;
    d[5]=0xc0021000;d[6]=0;
    if(memory->write(memory->context,command,d.data(),sizeof(d)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmInsertPushMarker, same pinned revision/license.
static int insert_text_marker(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint64_t marker,uint32_t selector,int32_t* result) {
    if(!result)return 60;
    if(!command || !marker) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(size<2 || size>0x4000)return 60; // Type3 bounded domain; never truncate packet count.
    const size_t bytes=size_t(size)*4;
    if(command>UINT64_MAX-bytes || marker>UINT64_MAX-bytes)return 112;
    std::vector<unsigned char> text;
    bool terminated=false;
    for(size_t i=0;i<bytes;++i) {
        unsigned char c=0;
        if(memory->read(memory->context,marker+i,&c,1))return 112;
        text.push_back(c);if(!c){terminated=true;break;}
    }
    if(!terminated)return 88; // Bounded authority exhausted; never synthesize terminator.
    const size_t len=text.size()-1;
    const uint32_t packet=uint32_t(((len+8)>>2)+((len+12)>>3)*2);
    if(packet+2!=size) {*result=-1;return 0;}
    if(command<marker+text.size() && marker<command+bytes)return 88;
    std::vector<uint32_t> words(size,0);
    words[0]=0xc0001000U|(packet<<16);words[1]=selector;
    std::memcpy(words.data()+2,text.data(),text.size());
    // Public memset starts at data_block[1] and spans packet*4 bytes,
    // exactly filling the declared packet after header and selector.
    if(memory->write(memory->context,command,words.data(),bytes))return 112;
    *result=0;return 0;
}

extern "C" int ctr_gnm_insert_push_marker(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint64_t marker,int32_t* result) {
    return insert_text_marker(memory,command,size,marker,0x68750001U,result);
}
// Whole public sceGnmInsertSetMarker uses identical packet/text formula.
extern "C" int ctr_gnm_insert_set_marker(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint64_t marker,int32_t* result) {
    return insert_text_marker(memory,command,size,marker,0x68750003U,result);
}
// Whole public sceGnmInsertPopMarker, emission only.
extern "C" int ctr_gnm_insert_pop_marker(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,int32_t* result) {
    if(!result)return 60;
    if(!command || size!=6) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-24)return 112;
    const uint32_t words[]={0xc0041000,0x68750002,0,0,0,0};
    if(memory->write(memory->context,command,words,sizeof(words)))return 112;
    *result=0;return 0;
}

// Whole public sceGnmDrawIndex. Public type argument is unused; index memory is
// not read by the emitter. Later draw execution requires genuine index authority.
extern "C" int ctr_gnm_draw_index(const CtrGuestCopyApi* memory,uint64_t command,
    uint32_t size,uint32_t count,uint64_t indices,uint32_t flags,uint32_t type,int neo,int32_t* result) {
    (void)type;
    if(!result || (neo!=0 && neo!=1))return 60;
    if(!command || size!=10 || !indices || (indices&1) || (flags&0x1ffffffeU)) {*result=-1;return 0;}
    if(!memory || memory->size!=sizeof(*memory) || memory->version!=1 ||
       !memory->context || !memory->read || !memory->write)return 60;
    if(command>UINT64_MAX-40)return 112;
    std::array<uint32_t,10> words;
    if(memory->read(memory->context,command,words.data(),sizeof(words)))return 112;
    const uint32_t emitted[]={0xc0042700,count,uint32_t(indices),uint32_t(indices>>32),count,
        neo ? flags&0xe0000000U : 0,0xc0021000,0};
    std::memcpy(words.data(),emitted,sizeof(emitted));
    if(memory->write(memory->context,command,words.data(),sizeof(words)))return 112;
    *result=0;return 0;
}
