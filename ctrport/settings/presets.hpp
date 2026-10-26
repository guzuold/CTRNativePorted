// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "control_schema.hpp"
#include <string>
namespace ctrport::settings::presets {
using ctr_online::Json;
// Quality bundles, not measured performance promises. Display, resolution,
// pacing, HDR, language, inputs and user extensions are intentionally independent.
// Legacy neutral-only effects are reset to Original by every named profile.
// Their protocol/storage support remains for existing user configurations.
inline bool owns(const std::string& key) {
    for(const char* name : {"anisotropy","antialiasing","ambient_occlusion","screen_space_reflections",
        "bloom","motion_blur","vignette","depth_of_field","distance_scale","cascaded_shadows",
        "racer_mesh_lod","texture_quality","shadow_resolution","shadow_cascade_limit",
        "static_mesh_lod","hero_shadows","vfx_quality","shadow_quality","shadow_map_quality","native_antialiasing","native_bloom",
        "native_motion_blur","native_depth_of_field","native_fur_detail","native_view_distance"}) if(key==name)return true;
    return false;
}
inline Json expand(const Json& changes) {
    auto result=changes;
    const auto* selected=changes.find("quality_preset");
    if(selected && selected->text!="custom") {
        for(auto option:control_schema::options) {
            const auto* key=control_schema::key(option);
            if(owns(key))result.set(key,Json::of("original"));
        }
        result.set("shadow_quality",Json::of("high"));  // its original level
        const auto set=[&](const char* key,const char* value){result.set(key,Json::of(value));};
        const auto& name=selected->text;
        if(name=="low") {
            set("anisotropy","2");
            set("native_depth_of_field","off");set("native_motion_blur","off");set("native_fur_detail","3");
            set("distance_scale","0.5");
            set("shadow_quality","off");set("texture_quality","quarter");set("static_mesh_lod","reduced");
            set("vfx_quality","low");
        } else if(name=="medium") {
            set("anisotropy","4");set("native_fur_detail","5");set("distance_scale","0.75");
            set("texture_quality","half");set("shadow_quality","low");
            set("static_mesh_lod","reduced");set("vfx_quality","low");
        } else if(name=="high") {
            set("anisotropy","8");set("shadow_quality","medium");
        } else if(name=="ultra") {
            set("anisotropy","16");set("racer_mesh_lod","highest");set("vfx_quality","high");
        } else if(name!="original")throw std::invalid_argument("unknown graphics preset");
        // A bundle must be unambiguous, never silently overwrite explicit values.
        for(const auto& item:changes.members)if(owns(item.first) && !(item.second==*result.find(item.first)))
            throw std::invalid_argument("preset conflicts with explicit graphics setting");
    } else if(!selected) {
        for(const auto& item:changes.members)if(owns(item.first)){result.set("quality_preset",Json::of("custom"));break;}
    }
    return result;
}
}
