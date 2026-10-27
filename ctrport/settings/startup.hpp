// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_override.hpp"
#include "renderer_startup.hpp"
#include "retail_graphics/overrides.hpp"
#include "geometry_controls/distance_hook.hpp"
#include "geometry_controls/shadow_override.hpp"
#include "menu_bridge/bridge.hpp"
#include "quality_controls/texture_hook.hpp"
#include "quality_controls/mesh_override.hpp"
#include "shadow_quality/hook.hpp"
#include "shadow_quality/hero_patch.hpp"
#include "quality_controls/world_hook.hpp"
#include "vfx_controls/override.hpp"
#include "native_graphics.hpp"
#include "../retail_map.hpp"
#include <cstdio>
#include <memory>

namespace ctrport::settings {
inline fs::path startup_path() {
    wchar_t path[32768]{};
    const auto size = GetEnvironmentVariableW(L"CTR_SETTINGS_PATH", path, 32768);
    if (!size || size >= 32768) throw std::invalid_argument("settings: invalid startup configuration path");
    return fs::path(path);
}

inline bridge::Hooks startup_hooks() {
    bridge::Hooks hooks;
    hooks.error = [](const char* error) noexcept {
        std::fprintf(stderr, "settings menu error: %s\n", error);
        std::fflush(stderr);
        MessageBoxA(nullptr, error, "PC Settings - save failed", MB_OK | MB_ICONERROR);
    };
    hooks.saved_restart_required = [](const char* key, const Json&) noexcept {
        std::printf("settings_saved key=%s application=restart\n", key);
        std::fflush(stdout);
    };
    hooks.publish_live = [](menu::Option option, const Json& value) noexcept {
        // Native retail graphics fields: applied by the per-frame native_graphics pass.
        try {
            if (native_graphics::set(menu::key(option), value)) return true;
        } catch (...) {
            return false;
        }
        if ((option == menu::Option::presentation_limit || option == menu::Option::upscaling_sharpness) &&
            live_gpu_set) {
            const bool pacing = option == menu::Option::presentation_limit;
            // sharpness: the index in its domain (original, 0, 10, ..., 100), as live_gpu::sharp reads it
            const unsigned index = pacing ? (value.text == "30"    ? 1u
                                             : value.text == "60"  ? 2u
                                             : value.text == "90"  ? 3u
                                             : value.text == "120" ? 4u
                                                                   : 0u)
                                          : (value.text == "original" ? 0u : unsigned(std::stoi(value.text) / 10 + 1));
            if (!live_gpu_set(option == menu::Option::presentation_limit ? 1u : 2u, index)) {
                std::fprintf(stderr, "settings live update rejected key=%s; saved for restart\n", menu::key(option));
                return false;
            }
        } else
            return false;
        std::printf("settings_saved key=%s application=live\n", menu::key(option));
        std::fflush(stdout);
        return true;
    };
    hooks.available = [](menu::Option option, unsigned index) noexcept {
        if (option == menu::Option::presentation_limit) return live_gpu_set != nullptr;
        if (option == menu::Option::upscaling_sharpness)
            return index == 0 || (live_gpu_set != nullptr && renderer_extensions_confirmed);
        // Direct3D 11: FSR 1 is its only upscaler; Ultra shadows are its 2x shadow maps
        if (option == menu::Option::upscaler && d3d_extensions_confirmed) return index < 2;
        if (option == menu::Option::shadow_quality) return index < 4 || d3d_extensions_confirmed;
        if (option == menu::Option::presentation_limit || option == menu::Option::window_mode ||
            option == menu::Option::display_refresh)
            return index == 0 || pacing_extensions_confirmed;
        if (option == menu::Option::upscaler) return index < 2 || display_extensions_confirmed;
        if (option == menu::Option::hero_shadows) return index == 0 || hero_extensions_confirmed;
        if (option == menu::Option::quality_preset) return index < 2 || renderer_extensions_confirmed;
        if (option == menu::Option::ambient_occlusion) return index == 0 || ao_extensions_confirmed;
        if (option == menu::Option::shadow_map_quality) return index == 0 || d3d_extensions_confirmed;
        if (option == menu::Option::hdr_output || option == menu::Option::display_gamma ||
            option == menu::Option::hdr_peak_nits || option == menu::Option::hdr_exposure)
            return index == 0 || display_extensions_confirmed;
        if (option == menu::Option::anisotropy || option == menu::Option::fsr1_sharpen)
            return index == 0 || renderer_extensions_confirmed;
        return true;
    };
    return hooks;
}

inline retail_graphics::Options retail_graphics_selection(const Json& document) {
    retail_graphics::Options result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto* value = document.find(retail_graphics::descriptors[i].key);
        result[i] = value && value->text == "off";
    }
    return result;
}

struct StartupAdapter {
    std::unique_ptr<bridge::Adapter> menu;
    std::unique_ptr<geometry_controls::DistanceHook> distance;
    unsigned char* image = nullptr;
    std::size_t extent = 0;
    std::string sha;
    std::unique_ptr<shadow_quality::Hook> shadow_detail;
    bool shadows = false;
    bool mesh = false;
    bool hero = false;
    vfx::Quality effects_quality = vfx::Quality::original;
    std::unique_ptr<quality_controls::WorldHook> world;
    std::unique_ptr<quality_controls::TextureHook> texture;
    void uninstall() {
        if (hero) {
            hero_shadows::rollback(image, extent, sha);
            hero = false;
        }
        if (world) world->uninstall();
        if (effects_quality != vfx::Quality::original) {
            vfx::rollback(image, extent, sha, effects_quality);
            effects_quality = vfx::Quality::original;
        }
        if (shadow_detail) shadow_detail->uninstall();
        if (texture) texture->uninstall();
        if (mesh) {
            quality_controls::rollback_mesh(image, extent, sha);
            mesh = false;
        }
        if (distance) distance->uninstall();
        if (shadows) {
            geometry_controls::rollback_shadow(image, extent, sha);
            shadows = false;
        }
        FlushInstructionCache(GetCurrentProcess(), image, extent);
        if (menu) menu->uninstall();
    }
};

// Loader barrier only: no guest worker or original callback may be executing.
// Both production entry and the full-image loader control call this function.
// Optional upscalers retain the bridge's conservative default capability gate.
inline std::unique_ptr<StartupAdapter> install_startup(unsigned char* image, std::size_t extent, const std::string& sha,
                                                       const fs::path& path, const Json& document,
                                                       bridge::Hooks hooks) {
    validate(document);
    // The EU 1.21 eboot (the reference) must take every requested control. On another retail
    // eboot (retail_map.hpp) a control whose code was not found is reported and left original.
    const bool strict = retail_map::identity();
    const auto optional = [&](const char* control, auto&& step) {
        if (strict) {
            step();
            return true;
        }
        try {
            step();
            return true;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "settings: %s not available on this eboot, left original: %s\n", control, e.what());
            std::fflush(stderr);
            return false;
        }
    };
    const auto* aa = document.find("antialiasing");
    bool disable = aa && aa->text == "off";
    if (disable)
        disable =
            optional("antialiasing", [&] { graphics::require_gate(image, extent, sha, graphics::aa_gate_original); });
    auto selected = retail_graphics_selection(document);
    if (strict)
        retail_graphics::verify(image, extent, sha, selected, false);
    else
        for (std::size_t i = 0; i < selected.size(); ++i) {
            retail_graphics::Options one{};
            one[i] = selected[i];
            if (selected[i])
                selected[i] = optional(retail_graphics::descriptors[i].key,
                                       [&] { retail_graphics::verify(image, extent, sha, one, false); });
        }
    const auto* shadow = document.find("cascaded_shadows");
    bool shadow_off = shadow && shadow->text == "off";
    if (shadow_off)
        shadow_off = optional("cascaded_shadows", [&] { geometry_controls::shadow_verify(image, extent, sha, false); });
    const auto* mesh = document.find("racer_mesh_lod");
    bool mesh_highest = mesh && mesh->text == "highest";
    if (mesh_highest)
        mesh_highest = optional("racer_mesh_lod", [&] { quality_controls::verify_mesh(image, extent, sha, false); });
    auto adapter = std::make_unique<StartupAdapter>();
    adapter->image = image;
    adapter->extent = extent;
    adapter->sha = sha;
    native_graphics::configure(image, sha, document);
    optional("PC Settings menu", [&] {
        auto menu = std::make_unique<bridge::Adapter>(image, sha, path, hooks, &document);
        menu->install();
        adapter->menu = std::move(menu);
    });
    bool aa_installed = false, retail_installed = false;
    try {
        const auto* hero = document.find("hero_shadows");
        if (hero && hero->text == "off")
            optional("hero_shadows", [&] {
                hero_shadows::apply(image, extent, sha);
                adapter->hero = true;
            });
        std::printf("settings_hero_shadows requested=%s application=restart\n", adapter->hero ? "off" : "original");
        const auto* world = document.find("static_mesh_lod");
        optional("static_mesh_lod", [&] {
            adapter->world = quality_controls::install_world(image, extent, sha,
                                                             world && world->text == "reduced"
                                                                 ? quality_controls::WorldLod::reduced
                                                                 : quality_controls::WorldLod::original);
        });
        const auto* vfx_value = document.find("vfx_quality");
        const auto effects_quality = vfx_value && vfx_value->text == "low"    ? vfx::Quality::low
                                     : vfx_value && vfx_value->text == "high" ? vfx::Quality::high
                                                                              : vfx::Quality::original;
        optional("vfx_quality", [&] {
            vfx::apply(image, extent, sha, effects_quality);
            adapter->effects_quality = effects_quality;
        });
        std::printf("settings_geometry_effects static_mesh=%s vfx=%s application=restart\n",
                    world ? world->text.c_str() : "original", vfx_value ? vfx_value->text.c_str() : "original");
        const auto* shadow_resolution = document.find("shadow_resolution");
        const auto* cascade_limit = document.find("shadow_cascade_limit");
        const auto resolution_value = shadow_resolution ? shadow_resolution->text : "original";
        const auto limit_value = cascade_limit ? cascade_limit->text : "original";
        shadow_quality::Options shadow_options;
        shadow_options.resolution = resolution_value == "0.5"    ? shadow_quality::Resolution::half
                                    : resolution_value == "0.75" ? shadow_quality::Resolution::three_quarters
                                                                 : shadow_quality::Resolution::original;
        shadow_options.cascade_limit = limit_value == "original" ? 0u : static_cast<unsigned>(std::stoul(limit_value));
        optional("shadow_resolution",
                 [&] { adapter->shadow_detail = shadow_quality::install(image, extent, sha, shadow_options); });
        std::printf("settings_shadow_quality resolution=%s limit=%s application=restart\n", resolution_value.c_str(),
                    limit_value.c_str());
        const auto* texture = document.find("texture_quality");
        const auto texture_value = texture ? texture->text : "original";
        optional("texture_quality", [&] {
            adapter->texture = quality_controls::install_texture(
                image, extent, sha,
                texture_value == "half"      ? quality_controls::TextureMips::half
                : texture_value == "quarter" ? quality_controls::TextureMips::quarter
                                             : quality_controls::TextureMips::original);
        });
        if (mesh_highest) {
            quality_controls::apply_mesh_highest(image, extent, sha);
            adapter->mesh = true;
        }
        std::printf("settings_quality texture=%s racer_mesh=%s application=restart\n", texture_value.c_str(),
                    mesh_highest ? "highest" : "original");
        if (shadow_off) {
            geometry_controls::apply_shadow_off(image, extent, sha);
            adapter->shadows = true;
        }
        std::printf("settings_shadows_applied requested=%s application=restart\n", shadow_off ? "off" : "original");
        const auto* distance = document.find("distance_scale");
        const auto distance_value = distance ? distance->text : "original";
        const auto scale = distance_value == "0.5"    ? geometry_controls::DistanceScale::half
                           : distance_value == "0.75" ? geometry_controls::DistanceScale::three_quarters
                                                      : geometry_controls::DistanceScale::original;
        optional("distance_scale", [&] { adapter->distance = geometry_controls::install(image, extent, sha, scale); });
        std::printf("settings_distance_applied requested=%s application=restart override_applied=%s\n",
                    distance_value.c_str(), adapter->distance ? "true" : "false");
        const auto applied = graphics::apply_aa(image, extent, sha, disable);
        aa_installed = applied.override_applied;
        const auto effects = retail_graphics::apply(image, extent, sha, selected);
        retail_installed = effects.controls_applied != 0;
        for (std::size_t i = 0; i < selected.size(); ++i)
            std::printf("settings_graphics_applied key=%s requested=%s application=restart override_applied=%s\n",
                        retail_graphics::descriptors[i].key, selected[i] ? "off" : "original",
                        selected[i] ? "true" : "false");
        std::printf(
            "{\"scope\":\"settings_antialiasing_applied\",\"requested\":\"%s\",\"application\":\"restart\",\"override_applied\":%s,\"changed_bytes\":%zu,\"reconstructed\":0}\n",
            disable ? "off" : "original", applied.override_applied ? "true" : "false", applied.changed_bytes);
        std::printf("{\"scope\":\"settings_original_menu_bridge\",\"installed\":%s,\"reconstructed\":0}\n",
                    adapter->menu ? "true" : "false");
        FlushInstructionCache(GetCurrentProcess(), image, extent);
        std::fflush(stdout);
    } catch (...) {
        if (retail_installed) retail_graphics::rollback(image, extent, sha, selected);
        if (aa_installed) graphics::rollback_aa(image, extent, sha);
        adapter->uninstall();
        throw;
    }
    return adapter;
}
} // namespace ctrport::settings
