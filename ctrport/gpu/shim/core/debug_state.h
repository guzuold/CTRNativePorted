// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: shadPS4's frame/register dump tooling is not part of the runtime; the
// flip and submission counters feed ctr_gpu_flip_count / ctr_gpu_submit_count.
// Adapted from bbport gpu/shim/core/debug_state.h (GPL-2.0) to the v0.18.0 interface.
#pragma once
#include <atomic>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "common/types.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/amdgpu/regs.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace DebugStateType {
enum class QueueType { dcb = 0, ccb = 1, acb = 2 };
struct QueueDump {
    QueueType type;
    u32 submit_num;
    u32 num2;
    std::vector<u32> data;
    uintptr_t base_addr;
};
class DebugStateImpl {
public:
    using CsState = AmdGpu::ComputeProgram;
    std::pair<u32, u32> game_resolution{};
    std::pair<u32, u32> output_resolution{};
    bool is_using_fsr{};
    std::atomic<u64> flips{0}, gnm_frames{0}, draws{0}, dispatches{0};
    void IncFlipFrameNum() {
        flips.fetch_add(1, std::memory_order_relaxed);
    }
    void IncDrawCall() noexcept {
        draws.fetch_add(1, std::memory_order_relaxed);
    }
    void IncDispatch() noexcept {
        dispatches.fetch_add(1, std::memory_order_relaxed);
    }
    void IncGnmFrameNum() {
        gnm_frames.fetch_add(1, std::memory_order_relaxed);
    }
    u32 GetFrameNum() const {
        return u32(flips.load(std::memory_order_relaxed));
    }
    bool DumpingCurrentFrame() const {
        return false;
    }
    bool DumpingCurrentReg() {
        return false;
    }
    bool ShouldPauseInSubmit() const {
        return false;
    }
    void PushQueueDump(QueueDump) {}
    void PushRegsDump(uintptr_t, uintptr_t, const AmdGpu::Regs&) {}
    void PushRegsDumpCompute(uintptr_t, uintptr_t, const CsState&) {}
    void CollectShader(const std::string&, Shader::LogicalStage, vk::ShaderModule,
                       std::span<const u32>, std::span<const u32>, std::span<const u32>, bool) {}
    void ShowDebugMessage(std::string) {}
    bool IsGuestThreadsPaused() const {
        return false;
    }
    void PauseGuestThreads() {}
    void ResumeGuestThreads() {}
};
} // namespace DebugStateType
inline DebugStateType::DebugStateImpl DebugStateInstance;
#define DebugState DebugStateInstance
