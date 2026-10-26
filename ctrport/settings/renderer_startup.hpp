// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "store.hpp"
#include "renderer_controls/policy.hpp"
#include "display_controls/policy.hpp"
#include "pacing_controls/policy.hpp"
#include <cstdio>

namespace ctrport::settings {
inline unsigned (*live_gpu_set)(unsigned,unsigned) = nullptr;
inline bool renderer_extensions_confirmed = false;
inline bool display_extensions_confirmed = false;
inline bool ao_extensions_confirmed = false;
inline bool hero_extensions_confirmed = false;
inline bool pacing_extensions_confirmed = false;
inline bool d3d_extensions_confirmed = false;  // Direct3D 11 renderer settings (shadow map detail)

// Loader-thread only, before LoadLibrary and its CRT initialization. The public
// startup document is authoritative over inherited process environment values.
inline void prepare_renderer_environment() {
    const auto* document = runtime_snapshot();
    if (!document) return;
    if(const auto* resolution=document->find("window_resolution")) {
        if(resolution->text!="original") {
            const auto split=resolution->text.find('x');
            const auto width=resolution->text.substr(0,split), height=resolution->text.substr(split+1);
            if(!SetEnvironmentVariableA("CTR_GPU_WIDTH",width.c_str()) ||
               !SetEnvironmentVariableA("CTR_GPU_HEIGHT",height.c_str()))
                win_error("settings window dimensions");
        }
    }
    const auto* up=document->find("upscaler");
    if(!SetEnvironmentVariableA("CTR_UPSCALER",up ? up->text.c_str() : "off")) win_error("settings upscaler environment");
    const auto* ao=document->find("ambient_occlusion");
    if(!SetEnvironmentVariableA("CTR_GPU_AO",ao && ao->text=="off" ? "off" : nullptr)) win_error("settings AO environment");
    const auto* hero=document->find("hero_shadows");
    if(!SetEnvironmentVariableA("CTR_GPU_HERO_SHADOWS",hero && hero->text=="off" ? "off" : nullptr)) win_error("settings hero environment");
    // Direct3D 11 shadow maps at twice the PS4 size (renderer ShadowQuality 2); the setting is
    // offered only by that renderer (d3d_extensions_confirmed)
    const auto* shadow_maps=document->find("shadow_map_quality");
    if(!SetEnvironmentVariableA("CTR_D3D_ShadowQuality",shadow_maps && shadow_maps->text=="high" ? "2" : nullptr)) win_error("settings shadow map environment");
    auto values = renderer::environment(*document, true);
    const auto display_values=display::environment(*document,true);
    values.insert(values.end(),display_values.begin(),display_values.end());
    const auto pacing_values=pacing::environment(*document,true);
    values.insert(values.end(),pacing_values.begin(),pacing_values.end());
    for (const auto& pair : values) {
        if (!SetEnvironmentVariableA(pair.first.c_str(), pair.second.empty() ? nullptr : pair.second.c_str()))
            win_error("settings renderer environment");
    }
}

// Call after LoadLibrary but BEFORE ctr_gpu_init; merely finding a DLL is not
// acknowledgement of the extension. A legacy DLL can still use original values.
inline void confirm_renderer_extensions(HMODULE module) {
    const auto* document = runtime_snapshot();
    renderer_extensions_confirmed = false;
    if (!document) return;
    auto live_query=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"ctr_gpu_live_settings_abi"));
    live_gpu_set=live_query && live_query()==1 ? reinterpret_cast<unsigned(*)(unsigned,unsigned)>(GetProcAddress(module,"ctr_gpu_live_settings_set")) : nullptr;
    auto query = reinterpret_cast<unsigned(*)()>(GetProcAddress(module, "ctr_gpu_settings_abi"));
    renderer_extensions_confirmed = query && query() == 1;
    (void)renderer::environment(*document, renderer_extensions_confirmed);
    auto display_query=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"ctr_gpu_display_abi"));
    display_extensions_confirmed=display_query && display_query()==1;
    (void)display::environment(*document,display_extensions_confirmed);
    auto pacing_query=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"ctr_gpu_pacing_abi"));
    pacing_extensions_confirmed=pacing_query && pacing_query()==1;
    (void)pacing::environment(*document,pacing_extensions_confirmed);
    auto hero_query=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"ctr_gpu_hero_abi"));
    hero_extensions_confirmed=hero_query && hero_query()==1;
    if(const auto* hero=document->find("hero_shadows"))
        if(hero->text!="original" && !hero_extensions_confirmed) throw std::runtime_error("hero shadows require GPU hero ABI 1");
    auto d3d_query=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"ctr_gpu_d3d_abi"));
    d3d_extensions_confirmed=d3d_query && d3d_query()==1;
    auto ao_query=reinterpret_cast<unsigned(*)()>(GetProcAddress(module,"ctr_gpu_ao_abi"));
    ao_extensions_confirmed=ao_query && ao_query()==1;
    if(const auto* ao=document->find("ambient_occlusion"))
        if(ao->text!="original" && !ao_extensions_confirmed) throw std::runtime_error("AO requires settings GPU AO ABI 1");
    std::printf("settings_renderer_capability extension_v1=%s application=restart\n",
                renderer_extensions_confirmed ? "true" : "false");
    std::fflush(stdout);
}
} // namespace ctrport::settings
