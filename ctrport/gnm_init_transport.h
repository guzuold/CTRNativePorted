// SPDX-License-Identifier: GPL-2.0-only
#ifndef CTRPORT_GNM_INIT_TRANSPORT_H
#define CTRPORT_GNM_INIT_TRANSPORT_H
#include "guest_copy_api.h"
/* Synchronous Windows C ABI. Nonzero transport status is failure, not a PS4
   return value. On success written_dwords is the original service result.
   neo_mode comes from the runtime's pinned hardware profile. Owner/callback
   and memory authority remain live until the call returns. */
typedef int (*CtrGnmInit350Transport)(const CtrGuestCopyApi* memory, uint64_t destination, uint32_t capacity_dwords,
                                      int neo_mode, uint32_t* written_dwords);
#endif
