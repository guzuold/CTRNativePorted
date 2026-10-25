// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../ao_controls/shader_override.hpp"
namespace ctrport::settings::hero_shadows {
inline constexpr std::uint64_t shader_hash=0x1d8043adULL;
inline std::vector<std::uint32_t> override_code(std::uint64_t hash,std::span<const std::uint32_t> code,bool off){
 if(!off||hash!=shader_hash)return {};
 if(code.size()!=161||ao::digest(code)!="db6b98beb4971c0af89b4eb70fa8a81bd0dfc1f41b42e32ee99b6b306ede6090")throw std::runtime_error("hero shadows: shader identity mismatch");
 if(code[154]!=0xd2820000||code[155]!=0x03ca0008||code[156]!=0x5e000100||code[157]!=0xbefe040e||code[158]!=0xf8001c0f||code[159]!=0||code[160]!=0xbf810000)throw std::runtime_error("hero shadows: export contract mismatch");
 std::vector<std::uint32_t> result(code.begin(),code.end());
 // The original result is 1 + fade * (visibility-1). Force its neutral output
 // directly: 0*NaN is not neutral. Same VGPR, EXEC and packed MRT0 export.
 result[154]=0x7e0002f2; // V_MOV_B32 v0, inline1.0
 result[155]=0xbf800000; // S_NOP 0
 return result;
}
}
