// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "status_layout.h"
#include "session_api.h"
#include <cstring>
namespace CtrVideo {
// Host-owned bounded output only. Runtime must validate/copy guest memory itself.
inline int PackVblank(const CtrVideoVblankV1& in,void* output,size_t bytes) {
    if(!output || bytes<sizeof(CtrVideoPublicVblankStatus)) return 60;
    CtrVideoPublicVblankStatus value{};
    value.count=in.count; value.process_time=in.process_microseconds; value.tsc=in.native_tsc;
    std::memcpy(output,&value,sizeof(value)); return 0;
}
// CPU flip domain only: this session has no Gnm/EOP requests, hence gc queue0.
inline int PackCpuFlip(const CtrVideoProgressV1& in,void* output,size_t bytes) {
    if(!output || bytes<sizeof(CtrVideoPublicFlipStatus)) return 60;
    if(in.pending>16 || in.current_slot < -1 || in.current_slot>15 || in.reserved ||
       (in.timing_valid & ~3U) || (in.completed && (in.timing_valid & 2U)==0) ||
       ((in.pending || in.completed) && (in.timing_valid & 1U)==0)) return 91;
    CtrVideoPublicFlipStatus value{};
    value.count=in.completed; value.process_time=in.completion_microseconds;
    value.tsc=in.completion_tsc; value.submit_tsc=in.submit_tsc;
    if(in.completed) std::memcpy(&value.flip_arg,&in.argument,sizeof(value.flip_arg));
    else value.flip_arg=-1;
    value.current_buffer=static_cast<int32_t>(in.current_slot);
    value.flip_pending_num=static_cast<int32_t>(in.pending);
    std::memcpy(output,&value,sizeof(value)); return 0;
}
}
