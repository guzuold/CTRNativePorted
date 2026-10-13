// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/handle_payload.h"
#include <cstdint>
namespace ctr {
struct SelectionAvailablePayload : GameplayPayload { std::uint8_t blocked; };
// Original object+18 -> owner+108 -> virtual+b0. Mandatory if queried.
using SelectionOwnerQuery=bool(*)(void*);
// Whole00eed420; mutable handle and unknown owner query never default to success.
bool selection_available(const GameplayHandle* handle,std::int32_t selection,
                         std::uint32_t input,SelectionOwnerQuery query,void* context);
}
