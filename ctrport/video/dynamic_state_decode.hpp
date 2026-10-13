// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "graphics_key_decode.hpp"
#include "graphics_companion.h"
namespace CtrPm4 {
// Whole public UpdateDepthStencilState/PrimitiveState/RasterizationState/ColorBlendingState
// register policy, pinned e3ce810f3a653f43ac64ebab63023de281a4103a.
// Descriptor feedback loop and pipeline write masks are separate pipeline/resource authority.
inline int DecodeDynamicState(const Registers& captured,bool indexed,bool listRestart,bool patchRestart,
 CtrGraphicsDynamicStateV1& output) {
 AmdGpu::Regs regs{};std::memcpy(regs.reg_array.data(),captured.words.data(),sizeof(captured.words));
 CtrGraphicsDynamicStateV1 r{};r.version=1;r.size=sizeof(r);
 r.depth_test=regs.depth_control.depth_enable && regs.depth_buffer.DepthValid();
 if(r.depth_test){r.depth_write=regs.depth_control.depth_write_enable && !regs.depth_render_control.depth_clear_enable;
  r.depth_compare=uint32_t(regs.depth_control.depth_func);}
 r.depth_bounds_test=regs.depth_control.depth_bounds_enable;
 if(r.depth_bounds_test){r.depth_min=regs.depth_bounds_min;r.depth_max=regs.depth_bounds_max;}
 r.depth_bias=regs.polygon_control.NeedsBias();
 if(r.depth_bias){const bool front=regs.polygon_control.enable_polygon_offset_front;
  r.bias_constant=front?regs.poly_offset.front_offset:regs.poly_offset.back_offset;
  r.bias_clamp=regs.poly_offset.depth_bias;r.bias_slope=(front?regs.poly_offset.front_scale:regs.poly_offset.back_scale)/16.f;}
 r.stencil_test=regs.depth_control.stencil_enable && regs.depth_buffer.StencilValid();
 if(r.stencil_test){const auto& sc=regs.stencil_control;
  const auto face=[](AmdGpu::StencilFunc fail,AmdGpu::StencilFunc pass,AmdGpu::StencilFunc zfail,
    AmdGpu::CompareFunc compare,const AmdGpu::StencilRefMask& ref,bool clear){
   CtrGraphicsStencilFaceV1 value{};value.fail_op=uint32_t(fail);value.pass_op=uint32_t(pass);
   value.depth_fail_op=uint32_t(zfail);value.compare_op=uint32_t(compare);
   const bool op=fail==AmdGpu::StencilFunc::ReplaceOp || pass==AmdGpu::StencilFunc::ReplaceOp || zfail==AmdGpu::StencilFunc::ReplaceOp;
   value.reference=op?ref.stencil_op_val:ref.stencil_test_val;value.write_mask=clear?0:ref.stencil_write_mask;
   value.compare_mask=ref.stencil_mask;return value;
  };
  const bool clear=regs.depth_render_control.stencil_clear_enable;
  r.front=face(sc.stencil_fail_front,sc.stencil_zpass_front,sc.stencil_zfail_front,
    regs.depth_control.stencil_ref_func,regs.stencil_ref_front,clear);
  r.back=regs.depth_control.backface_enable?face(sc.stencil_fail_back,sc.stencil_zpass_back,sc.stencil_zfail_back,
    regs.depth_control.stencil_bf_func,regs.stencil_ref_back,clear):r.front;
 }
 using P=AmdGpu::PrimitiveType;bool list=false,patch=false,culled=false;
 switch(regs.primitive_type){
 case P::PointList:case P::LineList:case P::AdjLineList:list=true;break;
 case P::TriangleList:case P::AdjTriangleList:list=true;culled=true;break;
 case P::PatchPrimitive:case P::QuadList:patch=true;culled=true;break;
 case P::RectList:patch=true;break;
 case P::TriangleFan:case P::TriangleStrip:case P::AdjTriangleStrip:case P::QuadStrip:case P::Polygon:culled=true;break;
 case P::None:case P::LineStrip:case P::AdjLineStrip:case P::LineLoop:break;
 default:return 88;
 }
 r.primitive_restart=(regs.enable_primitive_restart&1) && (listRestart || !list) && (patchRestart || !patch);
 if(indexed && r.primitive_restart && regs.primitive_restart_index!=0xffff && regs.primitive_restart_index!=0xffffffff)return 88;
 r.cull_mode=culled?uint32_t(regs.polygon_control.CullingMode()):0;
 r.front_face=uint32_t(regs.polygon_control.front_face);r.rasterizer_discard=regs.clipper_control.dx_rasterization_kill;
 r.line_width=regs.line_control.Width();
 std::memcpy(r.blend_constants,&regs.blend_constants,sizeof(r.blend_constants));
 output=r;return 0;
}
}
