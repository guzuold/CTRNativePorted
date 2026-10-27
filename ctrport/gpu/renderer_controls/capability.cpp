#include "live.hpp"
extern "C" __declspec(dllexport) unsigned ctr_gpu_settings_abi() {
    return 1;
}
extern "C" __declspec(dllexport) unsigned ctr_gpu_live_settings_abi() {
    return 1;
}
extern "C" __declspec(dllexport) unsigned ctr_gpu_live_settings_set(unsigned key, unsigned value) {
    return ctrport::settings::live_gpu::set(key, value);
}
extern "C" __declspec(dllexport) unsigned ctr_gpu_ao_abi() {
    return 1;
}
extern "C" __declspec(dllexport) unsigned ctr_gpu_hero_abi() {
    return 1;
}
