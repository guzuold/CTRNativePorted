// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video/pm4_registers.hpp"
#include <sstream>
namespace ctrport {
// Public shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a:
// regs_depth.h, regs.cpp. GPL-2.0-or-later. Metadata only, no GPU admission.
// Public StencilControl / StencilRefMask bit fields in regs_depth.h at the
// same pinned revision. Preserve distinct test-reference and operation values.
inline std::string stencil_face_metadata(uint32_t operations,uint32_t reference,uint32_t comparison){
 std::ostringstream out;out<<"{\"fail\":"<<(operations&15)<<",\"zpass\":"<<((operations>>4)&15)<<",\"zfail\":"<<((operations>>8)&15)
 <<",\"reference\":"<<(reference&255)<<",\"compare_mask\":"<<((reference>>8)&255)<<",\"write_mask\":"<<((reference>>16)&255)
 <<",\"operation_value\":"<<(reference>>24)<<",\"compare\":"<<comparison<<'}';return out.str();
}
inline std::string depth_attachment_metadata(const CtrPm4::Registers& registers,bool hint_observed,uint32_t hint) {
 const auto& r=registers.words;const uint32_t z=r[0xa010],s=r[0xa011],view=r[0xa002],ctl=r[0xa200],size=r[0xa016];
 std::ostringstream out;
 out<<"{\"depth_enable\":"<<((ctl>>1)&1)<<",\"depth_write_enable\":"<<((ctl>>2)&1)<<",\"stencil_enable\":"<<(ctl&1)
 <<",\"depth_compare\":"<<((ctl>>4)&7)<<",\"z_format\":"<<(z&3)<<",\"stencil_format\":"<<(s&1)
 <<",\"samples\":"<<(1u<<((z>>2)&3))<<",\"tile_mode\":"<<((z>>20)&7)<<",\"htile_enabled\":"<<((z>>29)&1)
 <<",\"z_read_address\":"<<(uint64_t(r[0xa012])<<8)<<",\"stencil_read_address\":"<<(uint64_t(r[0xa013])<<8)
 <<",\"z_write_address\":"<<(uint64_t(r[0xa014])<<8)<<",\"stencil_write_address\":"<<(uint64_t(r[0xa015])<<8)
 <<",\"z_read_valid\":"<<((r[0xa012] && (z&3))?"true":"false")<<",\"z_write_valid\":"<<((r[0xa014] && (z&3))?"true":"false")
 <<",\"stencil_read_valid\":"<<((r[0xa013] && (s&1))?"true":"false")<<",\"stencil_write_valid\":"<<((r[0xa015] && (s&1))?"true":"false")
 <<",\"pitch\":"<<(((size&2047)+1)<<3)<<",\"height\":"<<((((size>>11)&2047)+1)<<3)
 <<",\"slice_tile_max\":"<<(r[0xa017]&0x3fffff)<<",\"slice_start\":"<<(view&2047)<<",\"slice_max\":"<<((view>>13)&2047)
 <<",\"z_read_only\":"<<((view>>24)&1)<<",\"stencil_read_only\":"<<((view>>25)&1)
 <<",\"htile_address\":"<<(uint64_t(r[0xa005])<<8)<<",\"render_control_raw\":"<<r[0xa000]
 <<",\"depth_clear_float_bits\":"<<r[0xa00b]<<",\"stencil_clear_raw\":"<<r[0xa00a]
 <<",\"depth_clear_enable\":"<<(r[0xa000]&1)<<",\"stencil_clear_enable\":"<<((r[0xa000]>>1)&1)
 <<",\"stencil_front_compare\":"<<((ctl>>8)&7)<<",\"stencil_back_compare\":"<<((ctl>>20)&7)<<",\"stencil_backface_enable\":"<<((ctl>>7)&1)
 <<",\"stencil_control_raw\":"<<r[0xa10b]<<",\"stencil_front_ref_mask_raw\":"<<r[0xa10c]<<",\"stencil_back_ref_mask_raw\":"<<r[0xa10d]
 <<",\"stencil_front\":"<<stencil_face_metadata(r[0xa10b],r[0xa10c],(ctl>>8)&7)
 <<",\"stencil_back\":"<<stencil_face_metadata(r[0xa10b]>>12,r[0xa10d],(ctl>>20)&7)
 <<",\"extent_hint_observed\":"<<(hint_observed?"true":"false")<<",\"extent_hint_valid\":"<<((hint_observed && hint)?"true":"false")
 <<",\"extent_hint_width\":"<<(hint&65535)<<",\"extent_hint_height\":"<<(hint>>16)
 <<",\"allocation_extent_validated\":false,\"gpu_admission\":false}";
 return out.str();
}
}
