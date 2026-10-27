// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdint.h>
typedef struct CtrIntegerHostApi {
    uint32_t size, version;
    uint32_t (*call)(uint64_t, uint64_t, uint64_t, uint64_t*);
} CtrIntegerHostApi;
