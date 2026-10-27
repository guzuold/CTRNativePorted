#include "gpu_policy.hpp"
extern "C" __declspec(dllexport) unsigned ctr_gpu_display_abi() {
    return 1;
}
extern "C" __declspec(dllexport) unsigned ctr_gpu_temporal_state() {
    return ctrport::settings::display::temporal_state.load(std::memory_order_relaxed);
}
extern "C" __declspec(dllexport) unsigned ctr_gpu_temporal_capabilities() {
    return ctrport::settings::display::temporal_capabilities.load(std::memory_order_relaxed);
}
