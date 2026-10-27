// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted table ABI/selector behavior from pinned shadPS4 fontft.cpp,
// fontft_internal.cpp and font_internal.h revision e3ce810f3a653f43ac64ebab63023de281a4103a.
#pragma once
#include <array>
#include <cstdint>
#include "original_call.hpp"
namespace ctrport {
extern "C" void ctr_sysv_font_pixel_resolution();
extern "C" void ctr_sysv_font_library_callback_missing();
extern "C" void ctr_sysv_font_renderer_callback_missing();
struct FontRendererSelection{std::uint32_t magic,size;void* create;void* destroy;void* query;};
static_assert(sizeof(FontRendererSelection)==32);
inline const std::array<std::uint64_t,36>& font_library_selection(){
 static const auto table=[](){std::array<std::uint64_t,36> t{};
  t[2]=reinterpret_cast<std::uint64_t>(&ctr_sysv_font_pixel_resolution);
  for(const auto offset:{0x18,0x20,0x28,0x38,0x40,0x50,0x60,0x78,0x80,0x88,0xa0,0xa8,0xb8,0xe0,0x108,0x118})t[static_cast<std::size_t>(offset)/8]=reinterpret_cast<std::uint64_t>(&ctr_sysv_font_library_callback_missing);
  return t;}();return table;
}
inline const FontRendererSelection& font_renderer_selection(){
 static const FontRendererSelection table{0,0x168,reinterpret_cast<void*>(&ctr_sysv_font_renderer_callback_missing),reinterpret_cast<void*>(&ctr_sysv_font_renderer_callback_missing),reinterpret_cast<void*>(&ctr_sysv_font_renderer_callback_missing)};return table;
}
inline const void* font_select_library(int value){return value==0?font_library_selection().data():nullptr;}
inline const void* font_select_renderer(int value){return value==0?&font_renderer_selection():nullptr;}
}
