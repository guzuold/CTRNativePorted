// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_nested_capture.hpp"
#include <array>
inline bool TestOwnedNestedCapture() {
    struct Owner {
        std::array<uint32_t,8> data{1,2,3,4,5,6,7,8};
        int phase=0, releases=0, ends=0, mode=0;
    } owner;
    CtrShaderOwnedRangesV1 ranges{1,sizeof(CtrShaderOwnedRangesV1),&owner,
        [](void* p,uint64_t address,uint64_t,CtrShaderRangeLeaseV1* out)->int {
            auto& o=*static_cast<Owner*>(p);if(o.phase)return 116;
            if(o.mode==1)return 88;
            o.phase=1;*out={address, o.mode==2?3ULL:31ULL,1};return 0;
        },[](void* p,const CtrShaderRangeLeaseV1*)->int {
            auto& o=*static_cast<Owner*>(p);if(o.phase!=1)return 116;
            o.phase=0;++o.ends;return o.mode==6?116:0;
        }};
    CtrShaderInputsV1 inputs{1,sizeof(CtrShaderInputsV1),&owner,
        [](void* p,uint64_t address,uint64_t bytes,CtrShaderSnapshot* out)->int {
            auto& o=*static_cast<Owner*>(p);if(o.phase!=1)return 116;
            if(o.mode==3)return 88;
            o.phase=2;*out={o.data.data(),bytes,address+(o.mode==4?4:0),2};return 0;
        },[](void* p,const CtrShaderSnapshot*)->int {
            auto& o=*static_cast<Owner*>(p);if(o.phase!=2)return 116;
            o.phase=1;++o.releases;return o.mode==5?73:0;
        }};
    std::vector<uint32_t> result{99};
    if(CtrPm4::CaptureOwnedNestedRange(&inputs,&ranges,0x1000,32,0,result) ||
        result.size()!=7 || result.back()!=7 || owner.releases!=1 || owner.ends!=1)return false;
    owner.data.fill(42);if(result[0]!=1)return false; // copy survives release
    if(CtrPm4::CaptureOwnedNestedRange(&inputs,&ranges,0x1000,32,16,result) || result.size()!=4)return false;
    for(int mode=1;mode<=6;++mode) {
        owner.mode=mode; result={99};const int oldEnds=owner.ends,oldRelease=owner.releases;
        const int status=CtrPm4::CaptureOwnedNestedRange(&inputs,&ranges,0x1000,32,0,result);
        const int expected=mode==5?73:mode==6?116:88;
        if(status!=expected || result!=std::vector<uint32_t>{99} || owner.phase ||
            owner.ends!=oldEnds+(mode!=1) || owner.releases!=oldRelease+(mode>=4))return false;
    }
    owner.mode=0;result={99};
    if(CtrPm4::CaptureOwnedNestedRange(&inputs,&ranges,0x1000,32,32,result)!=88 || result[0]!=99)return false;
    const int oldEnds=owner.ends;
    if(CtrPm4::CaptureOwnedNestedRange(&inputs,&ranges,UINT64_MAX-3,32,0,result)!=112 ||
        CtrPm4::CaptureOwnedNestedRange(&inputs,&ranges,0x1001,32,0,result)!=60 || owner.ends!=oldEnds)return false;
    std::printf("owned_nested_capture=2_positive_9_negative_cleanup_atomic\n");
    // Descriptor-sized owner range, compiler-selected root pair only.
    auto exactRanges=ranges;
    exactRanges.begin=[](void* p,uint64_t address,uint64_t cap,CtrShaderRangeLeaseV1* out)->int {
        auto& o=*static_cast<Owner*>(p);if(o.phase)return 116;
        if(address!=0x1010 || cap!=16)return 88;
        o.phase=1;*out={address,16,3};return 0;
    };
    std::array<uint32_t,16> ud{};ud[4]=0x1000;ud[5]=0xffff0000;
    std::array<uint32_t,4> descriptor{99,99,99,99};
    if(CtrPm4::CaptureOwnedFetchDescriptor(&inputs,&exactRanges,ud.data(),6,4,4,descriptor.data()) || descriptor[3]!=42)return false;
    for(auto args:std::array<std::array<uint32_t,3>,4>{{{5,4,4},{6,6,4},{6,4,5},{17,4,4}}}) {
        descriptor.fill(99);
        if(!CtrPm4::CaptureOwnedFetchDescriptor(&inputs,&exactRanges,ud.data(),args[0],args[1],args[2],descriptor.data()) || descriptor[0]!=99)return false;
    }
    ud[4]=0;ud[5]=0;
    if(CtrPm4::CaptureOwnedFetchDescriptor(&inputs,&exactRanges,ud.data(),6,4,0,descriptor.data())!=88)return false;
    ud[4]=0x1001;
    if(CtrPm4::CaptureOwnedFetchDescriptor(&inputs,&exactRanges,ud.data(),6,4,0,descriptor.data())!=88)return false;
    std::printf("owned_fetch_descriptor=1_positive_6_negative_mask48_checked_pair_exact16\n");return true;
}
