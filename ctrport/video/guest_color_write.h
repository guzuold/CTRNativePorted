// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted public sceVideoOutColorSettingsSetGamma, videoout/video_out.cpp,
// e3ce810f3a653f43ac64ebab63023de281a4103a. Host adapter only.
#pragma once
#include "../guest_copy_api.h"
static inline int ctr_video_write_gamma(const CtrGuestCopyApi* memory, uint64_t address, float gamma) {
    if (!memory || memory->size != sizeof(*memory) || memory->version != 1 || !memory->context || !memory->write)
        return 60;
    // Preserve public comparison semantics, including unordered NaN; not a claim
    // that retail accepts NaN or that the renderer can apply it.
    if (gamma < 0.1f || gamma > 2.0f) return 89;
    // Gamma occupies only the first4 bytes; reserved settings remain untouched.
    return memory->write(memory->context, address, &gamma, sizeof(gamma)) ? 112 : 0;
}
