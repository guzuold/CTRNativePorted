// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
namespace ctrport::settings::quality_controls {
inline constexpr std::size_t mesh_site=0xeadfce;
inline constexpr unsigned char mesh_original[]{0x0f,0x85,0x09,0x01,0x00,0x00};
inline constexpr unsigned char mesh_highest[]{0xe9,0x0a,0x01,0x00,0x00,0x90};
// Bootstrap-only: original authored highest-detail branch, including original
// multiplayer camera aggregation and resource-switch scheduling. No asset index
// is invented and no runtime ownership or scene visibility is changed.
inline void verify_mesh(unsigned char* base,std::size_t extent,const std::string& sha,bool patched){
 if(sha!="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5" || !base || mesh_site>extent || sizeof(mesh_original)>extent-mesh_site)
  throw std::invalid_argument("unsupported mesh quality image");
 if(std::memcmp(base+mesh_site,patched?mesh_highest:mesh_original,sizeof(mesh_original)))throw std::invalid_argument("mesh quality branch signature mismatch");
}
inline void apply_mesh_highest(unsigned char* base,std::size_t extent,const std::string& sha){verify_mesh(base,extent,sha,false);std::memcpy(base+mesh_site,mesh_highest,sizeof(mesh_highest));}
inline void rollback_mesh(unsigned char* base,std::size_t extent,const std::string& sha){verify_mesh(base,extent,sha,true);std::memcpy(base+mesh_site,mesh_original,sizeof(mesh_original));}
}
