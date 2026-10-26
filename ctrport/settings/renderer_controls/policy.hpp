// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../../online_server/json.hpp"
#include "gpu_policy.hpp"
#include <vector>
#include <utility>
namespace ctrport::settings::renderer {
inline std::string value(const ctr_online::Json& doc,const char* key) {
    const auto* p=doc.find(key);
    if(!p) return "original";
    if(!p->is_string()) throw std::invalid_argument(std::string("expected string: ")+key);
    return p->text;
}
inline void validate(const ctr_online::Json& doc) {
    const auto mode=value(doc,"present_mode");
    if(mode!="original" && mode!="fifo" && mode!="mailbox" && mode!="immediate")
        throw std::invalid_argument("invalid presentation mode");
    anisotropy_request(value(doc,"anisotropy").c_str());
    sharpness_policy(value(doc,"fsr1_sharpen").c_str());
    sharpness_policy(value(doc,"upscaling_sharpness").c_str());
}
// Call before loading the renderer DLL. An empty value means remove a stale
// process variable, restoring the renderer's original default on next launch.
inline std::vector<std::pair<std::string,std::string>> environment(
        const ctr_online::Json& doc,bool extended_gpu) {
    validate(doc);
    const auto mode=value(doc,"present_mode"), aniso=value(doc,"anisotropy");
    const auto sharp=doc.find("upscaling_sharpness") ? value(doc,"upscaling_sharpness") : value(doc,"fsr1_sharpen");
    if(!extended_gpu && (aniso!="original" || sharp!="original"))
        throw std::runtime_error("selected controls require the settings GPU build");
    return {{"CTR_GPU_PRESENT_MODE",mode=="fifo"?"Fifo":mode=="mailbox"?"Mailbox":mode=="immediate"?"Immediate":""},
            {"CTR_SETTINGS_ANISOTROPY",aniso=="original"?"":aniso},
            {"CTR_SETTINGS_FSR1_SHARPEN",sharp=="original"?"":sharp}};
}
} // namespace ctrport::settings::renderer
