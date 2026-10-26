// SPDX-License-Identifier: GPL-2.0-or-later
// libkernel event queues (kqueue model) for the events the backend raises: GPU end-of-pipe and
// compute release interrupts (GraphicsCore), VideoOut flip and vblank, and user events.
#pragma once
#include "common.h"

struct Equeue;

constexpr s16 kFilterUser = -11;
constexpr s16 kFilterVideoOut = -13;
constexpr s16 kFilterGraphicsCore = -14;
constexpr u16 kEvAdd = 0x1, kEvClear = 0x20;

// The queue behind a guest handle, nullptr when it is not one of ours.
Equeue* EqueueFromHandle(u64 handle);
void EqueueAdd(Equeue* eq, u64 ident, s16 filter, u16 flags, u64 data, void* udata);
bool EqueueDelete(Equeue* eq, u64 ident, s16 filter);
// Triggers the registrations matching ident and filter; `data` becomes the event data.
void EqueueTrigger(Equeue* eq, u64 ident, s16 filter, u64 data);
// VideoOut events: data = time[11:0] | count[15:12] | hint (event id | arg << 16)[63:16].
void EqueueTriggerDisplay(Equeue* eq, u64 ident, u64 hint);
