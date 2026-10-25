// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
namespace ctrport::settings::vfx {
inline constexpr std::size_t site=0xaf056e;
inline constexpr std::array<unsigned char,9> original{0xc4,0xc1,0x7a,0x10,0x87,0xf4,0x04,0,0};
inline constexpr std::array<unsigned char,9> low{0xc5,0xf8,0x57,0xc0,0x90,0x90,0x90,0x90,0x90};
inline constexpr std::array<unsigned char,9> high{0xb8,0,0,0x80,0x3f,0xc5,0xf9,0x6e,0xc0};
enum class Quality { original, low, high };
inline const std::array<unsigned char,9>& bytes(Quality q) {
 if(q==Quality::original)return original;if(q==Quality::low)return low;if(q==Quality::high)return high;
 throw std::invalid_argument("invalid VFX quality");
}
inline void verify(const unsigned char* p,std::size_t n,const std::string& sha,Quality q) {
 const auto& expected=bytes(q);
 if(sha!="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5" || !p || site>n || expected.size()>n-site)
  throw std::invalid_argument("VFX unsupported image");
 if(std::memcmp(p+site,expected.data(),expected.size()))throw std::invalid_argument("VFX producer signature mismatch");
}
// Loader barrier only. Publishes quality and its original dependent enable flags;
// never changes particle spawn/lifetime, simulation, or scene-owned parameters.
inline void apply(unsigned char* p,std::size_t n,const std::string& sha,Quality q) {
 const auto& replacement=bytes(q); if(q==Quality::original)return;
 verify(p,n,sha,Quality::original);std::memcpy(p+site,replacement.data(),replacement.size());verify(p,n,sha,q);
}
inline void rollback(unsigned char* p,std::size_t n,const std::string& sha,Quality q) {
 if(q==Quality::original)return;verify(p,n,sha,q);std::memcpy(p+site,original.data(),original.size());verify(p,n,sha,Quality::original);
}
}
