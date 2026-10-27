// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../../retail_map.hpp"
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
namespace ctrport::settings::geometry_controls {
inline constexpr std::size_t shadow_site=0x3576a1;
// Follow the retail scene-disabled branch. It clears all four shadow camera
// visibility flags and invalidates the active cascade count, retaining cleanup.
// Bootstrap-only; never claims to resize or release the shadow atlas.
inline void shadow_verify(unsigned char* base,std::size_t extent,const std::string& sha,bool patched){
 if(!retail_map::bound(sha) || !base || retail_map::at(shadow_site)>extent || 2>extent-retail_map::at(shadow_site))
  throw std::invalid_argument("unsupported shadow override image");
 const unsigned char expected[]{static_cast<unsigned char>(patched?0xeb:0x74),0x5d};
 if(!retail_map::same(base+retail_map::at(shadow_site),shadow_site,expected,2))throw std::invalid_argument("shadow branch signature mismatch");
}
inline void apply_shadow_off(unsigned char* base,std::size_t extent,const std::string& sha){shadow_verify(base,extent,sha,false);base[retail_map::at(shadow_site)]=0xeb;shadow_verify(base,extent,sha,true);}
inline void rollback_shadow(unsigned char* base,std::size_t extent,const std::string& sha){shadow_verify(base,extent,sha,true);base[retail_map::at(shadow_site)]=0x74;shadow_verify(base,extent,sha,false);}
}
