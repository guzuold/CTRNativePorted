// SPDX-License-Identifier: GPL-2.0-or-later
// Guest memory as the renderer sees it: readability checks without a system call per access,
// and write tracking (Windows write watch on the guest mappings ctrport reserves with
// MEM_WRITE_WATCH; content hashes for memory that has no write watch).
#pragma once
#include "common.h"

// GPU-visible guest mappings reported by ctrport (ctr_gpu_map_memory / unmap).
void GuestAddRange(u64 addr, u64 size);
void GuestRemoveRange(u64 addr, u64 size);
// [addr, addr+size) is committed, readable guest memory.
bool GuestReadable(u64 addr, u64 size);
// The guest wrote the range since the last reset; `reset` starts a new interval.
bool GuestWrittenSince(u64 addr, u64 size, bool reset);
void GuestResetWritten(u64 addr, u64 size);
// Write tracking for a range that may share its first and last page with other data (a texture
// smaller than a page, or not page-aligned): the pages it fills alone are watched and reset
// freely; the parts on shared pages are compared by content, so a write by a neighbour neither
// counts as a change nor is lost to the neighbour by a reset.
struct GuestWatch {
    u64 inner_begin = 0, inner_end = 0; // whole pages inside the range
    u64 edge_hash = 0;                  // content of the parts on shared pages
};
GuestWatch GuestWatchStart(u64 addr, u64 size);  // also resets the inner pages
bool GuestWatchChanged(const GuestWatch& watch, u64 addr, u64 size);
