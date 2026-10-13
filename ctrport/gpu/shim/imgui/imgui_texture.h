// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: no ImGui in the runtime. The patched presenter blits frames directly;
// these declarations only keep vk_presenter.h's members compiling.
#pragma once
#include "common/types.h"
using ImTextureID = u64;
namespace ImGui {
class RefCountedTexture {
public:
    struct Image {
        ImTextureID im_id;
        u32 width;
        u32 height;
    };
    Image GetTexture() const {
        return {};
    }
    explicit(false) operator bool() const {
        return false;
    }
};
} // namespace ImGui
