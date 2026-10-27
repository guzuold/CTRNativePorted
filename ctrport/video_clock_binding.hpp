// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "display_event_host.hpp"
#include <iostream>
#include "video/session_api.h"
namespace ctrport {
using VideoClockBinder = int (*)(void*, const CtrDisplayEventApi*);
// Borrow callback context/code from the global runtime clock for session lifetime.
// No event queue or registration is created. Provider copies the table synchronously.
inline int bind_video_clock(const CtrVideoSessionApiV1* session, DisplayEventHost& host, VideoClockBinder binder) {
    if (!session || session->size != sizeof(*session) || session->version != 1 || !session->context || !binder)
        return 60;
    const auto clock = display_event_api(host);
    return binder(session->context, &clock);
}
}
