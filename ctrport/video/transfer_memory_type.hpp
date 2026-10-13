// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Preserve first compatible visible/coherent fallback; prefer cached only for CPU reads.
inline uint32_t CtrTransferMemoryType(const VkPhysicalDeviceMemoryProperties& memory,uint32_t bits,bool cpuRead) {
 uint32_t fallback=UINT32_MAX;
 constexpr auto required=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
 for(uint32_t i=0;i<memory.memoryTypeCount && i<32;++i) {
  const auto flags=memory.memoryTypes[i].propertyFlags;
  if(!(bits&(1u<<i)) || (flags&required)!=required)continue;
  if(fallback==UINT32_MAX)fallback=i;
  if(!cpuRead || (flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT))return i;
 }
 return fallback;
}
inline bool CtrTransferMemoryTypeControl() {
 VkPhysicalDeviceMemoryProperties m{};m.memoryTypeCount=4;
 m.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
 m.memoryTypes[1].propertyFlags=6;m.memoryTypes[2].propertyFlags=14;m.memoryTypes[3].propertyFlags=14;
 return CtrTransferMemoryType(m,15,true)==2 && CtrTransferMemoryType(m,15,false)==1 &&
  CtrTransferMemoryType(m,3,true)==1 && CtrTransferMemoryType(m,8,true)==3 &&
  CtrTransferMemoryType(m,1,true)==UINT32_MAX && CtrTransferMemoryType(m,0,true)==UINT32_MAX;
}
