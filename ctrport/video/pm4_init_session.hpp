// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "pm4_init_plan.hpp"
#include "../direct_mapping_lease.hpp"
// Synchronous Win64/MSVC companion. Immutable owned host snapshot ONLY.
// First initialization route (no draws/dispatch/CCB/IRQ); unknown packets reject.
// Caller owns serialization with other GPU submissions; session owner required.
extern "C" int ctr_video_submit_init_v1(void* session,
    const std::shared_ptr<ctrport::DirectMemoryBacking>* owner,
    const uint32_t* snapshot,size_t words);
