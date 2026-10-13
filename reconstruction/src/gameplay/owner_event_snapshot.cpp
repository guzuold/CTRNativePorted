// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/owner_event_snapshot.h"
#include <cmath>
#include <stdexcept>
namespace ctr {
void update_owner_event_snapshot(std::uint16_t* output,float amount,std::uint32_t flags,
 std::uint64_t bits,const OwnerEventSnapshotServices& services){
 if(!output)return;
 if(!services.lookup)throw std::invalid_argument("Missing genuine snapshot singleton/table lookup");
 const auto indexes=services.lookup(services.context);
 const float scaled=amount*127.0f; // Retail1.00 constant1b9ad14.
 // CVTTSS2SI exceptional inputs remain outside this adapted finite contract.
 if(!std::isfinite(scaled)||scaled < -2147483648.0f||scaled >= 2147483648.0f)
  throw std::invalid_argument("Snapshot float conversion outside finite int32 domain");
 const auto converted=static_cast<std::int32_t>(scaled);
 const auto low=static_cast<std::uint32_t>(converted)&255u;
 const auto byte35=(flags>>8u)&255u;
 const auto byte36=(flags>>16u)&255u;
 const auto first=static_cast<std::uint32_t>((bits>>(indexes.first&63u))&1u);
 const auto second=static_cast<std::uint32_t>((bits>>(indexes.second&63u))&1u);
 *output=static_cast<std::uint16_t>((*output&0xe000u)|low|(flags<<8u)|(byte36<<9u)|(byte35<<10u)|(first<<11u)|(second<<12u));
}
}
