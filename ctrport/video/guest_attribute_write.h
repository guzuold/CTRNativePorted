// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from videoout/video_out.cpp sceVideoOutSetBufferAttribute at
// e3ce810f3a653f43ac64ebab63023de281a4103a. Host checked-copy adapter, not SysV.
#pragma once
#include "session_api.h"
#include "../guest_copy_api.h"
#include <string.h>
static inline int ctr_video_write_buffer_attributes(const CtrGuestCopyApi* memory, uint64_t address, uint32_t format,
                                                    uint32_t tiling, uint32_t aspect, uint32_t width, uint32_t height,
                                                    uint32_t pitch) {
    if (!memory || memory->size != sizeof(*memory) || memory->version != 1 || !memory->context || !memory->write)
        return 60;
    CtrVideoBufferAttributesV1 value;
    memset(&value, 0, sizeof(value));
    value.format = format;
    // Preserve all32 input bits without implementation-defined unsigned conversion.
    memcpy(&value.tiling, &tiling, sizeof(tiling));
    memcpy(&value.aspect, &aspect, sizeof(aspect));
    value.width = width;
    value.height = height;
    value.pitch_pixels = pitch;
    return memory->write(memory->context, address, &value, sizeof(value)) ? 112 : 0;
}
