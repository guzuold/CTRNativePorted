// SPDX-License-Identifier: GPL-2.0-only
#include "shader_settings.h"
#include <atomic>
#include <mutex>
#include <memory>
#include <stdexcept>
namespace CtrVideo {
namespace {
std::atomic<const ShaderSettings*> published{nullptr};
std::mutex initialization;
// Deliberately process-lifetime: workers may retain immutable references.
}
void InitializeShaderSettings(ShaderSettings settings) {
    std::lock_guard lock(initialization);
    if (published.load(std::memory_order_acquire)) throw std::logic_error("Shader settings already initialized");
    auto owned = std::make_unique<const ShaderSettings>(settings);
    published.store(owned.release(), std::memory_order_release);
}
const ShaderSettings& GetShaderSettings() {
    auto* settings = published.load(std::memory_order_acquire);
    if (!settings) throw std::logic_error("Shader settings missing explicit initialization");
    return *settings;
}
}
