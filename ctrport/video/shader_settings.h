// SPDX-License-Identifier: GPL-2.0-only
#pragma once
namespace CtrVideo {
class ShaderSettings final {
public:
    constexpr ShaderSettings(bool dump_shaders, bool direct_memory_access) noexcept
        : dump_(dump_shaders), direct_(direct_memory_access) {}
    bool IsDumpShaders() const noexcept { return dump_; }
    bool IsDirectMemoryAccessEnabled() const noexcept { return direct_; }
private:
    const bool dump_;
    const bool direct_;
};
// Host configuration only. Initialize before shader workers. No default policy.
void InitializeShaderSettings(ShaderSettings settings);
const ShaderSettings& GetShaderSettings();
}
