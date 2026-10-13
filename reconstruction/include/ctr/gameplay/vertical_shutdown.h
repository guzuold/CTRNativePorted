// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/vertical_reset.h"
namespace ctr {
using VerticalTerminalRelease=void(*)(VerticalResetHandle*&,void*);
// Whole00f14a80, adapted borrowed storage; original atomic shared lifetime unverified.
void shutdown_vertical_state(bool& active,VerticalResetHandle*& slot,
 VerticalTerminalRelease terminal_release=nullptr,void* context=nullptr);
}
