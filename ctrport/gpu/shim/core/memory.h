// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: the MemoryManager surface used by the shadPS4 video core and GnmDriver.
// Guest memory lives at identical host addresses inside ctrport's direct memory
// backing; ctrport reports GPU-visible mappings through ctr_gpu_map_memory().
// Adapted from bbport gpu/shim/core/memory.h (GPL-2.0).
#pragma once
#include "common/singleton.h"
#include "common/types.h"
#include "core/address_space.h"

namespace Vulkan {
class Rasterizer;
}

namespace Core {
class MemoryManager {
public:
    void SetRasterizer(Vulkan::Rasterizer* rasterizer_);
    AddressSpace& GetAddressSpace() {
        return impl;
    }
    bool IsValidGpuMapping(VAddr virtual_addr, u64 size = 0) {
        // The PS4's GPU can only handle 40 bit addresses.
        return virtual_addr + size < 0x100'0000'0000ULL;
    }
    bool IsValidMapping(VAddr virtual_addr, u64 size = 0);
    u64 ClampRangeSize(VAddr virtual_addr, u64 size);
    void CopySparseMemory(VAddr source, u8* dest, u64 size);
    bool TryWriteBacking(void* address, const void* data, u64 size);
    void InvalidateMemory(VAddr address, u64 size);
    Vulkan::Rasterizer* GetRasterizer() const {
        return rasterizer;
    }

private:
    AddressSpace impl;
    Vulkan::Rasterizer* rasterizer{};
};
using Memory = Common::Singleton<MemoryManager>;
} // namespace Core
