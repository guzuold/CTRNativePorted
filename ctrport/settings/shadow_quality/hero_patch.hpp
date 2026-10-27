// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../../retail_map.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string_view>
namespace ctrport::settings::hero_shadows {
inline constexpr std::size_t site=0x950781;
inline constexpr std::array<unsigned char,5> original{{0x45,0x0f,0xb6,0x67,0x18}};
inline constexpr std::array<unsigned char,5> disabled{{0x45,0x31,0xe4,0x90,0x90}};
inline constexpr std::size_t serialize_site=0x950503;
inline constexpr std::array<unsigned char,4> serialize_original{{0x41,0x8a,0x56,0x18}};
inline constexpr std::array<unsigned char,4> serialize_disabled{{0x30,0xd2,0x90,0x90}};
inline void verify(unsigned char* image,std::size_t extent,std::string_view sha,bool patched=false){
 if(!retail_map::bound(std::string(sha))||!image||!retail_map::has(site)||!retail_map::has(serialize_site)||retail_map::at(site)>extent||original.size()>extent-retail_map::at(site))throw std::runtime_error("hero shadows: unsupported image");
 auto* const publisher=image+retail_map::at(site);auto* const serializer=image+retail_map::at(serialize_site);
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery(publisher,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY)))throw std::runtime_error("hero shadows: bootstrap image not writable");
 MEMORY_BASIC_INFORMATION serial_info{};
 if(!VirtualQuery(serializer,&serial_info,sizeof(serial_info))||serial_info.State!=MEM_COMMIT||(serial_info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(serial_info.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY)))throw std::runtime_error("hero shadows: serializer image not writable");
 if(!retail_map::same(serializer,serialize_site,(patched?serialize_disabled:serialize_original).data(),serialize_original.size()))throw std::runtime_error("hero shadows: serializer signature mismatch");
 if(!retail_map::same(publisher,site,(patched?disabled:original).data(),original.size()))throw std::runtime_error("hero shadows: publisher signature mismatch");
}
inline void apply(unsigned char* image,std::size_t extent,std::string_view sha){verify(image,extent,sha);auto* s=image+retail_map::at(serialize_site);auto* p=image+retail_map::at(site);std::memcpy(s,serialize_disabled.data(),serialize_disabled.size());std::memcpy(p,disabled.data(),disabled.size());FlushInstructionCache(GetCurrentProcess(),s,serialize_disabled.size());FlushInstructionCache(GetCurrentProcess(),p,disabled.size());}
inline void rollback(unsigned char* image,std::size_t extent,std::string_view sha){verify(image,extent,sha,true);auto* s=image+retail_map::at(serialize_site);auto* p=image+retail_map::at(site);std::memcpy(s,serialize_original.data(),serialize_original.size());std::memcpy(p,original.data(),original.size());FlushInstructionCache(GetCurrentProcess(),s,serialize_original.size());FlushInstructionCache(GetCurrentProcess(),p,original.size());}
}
