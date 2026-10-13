// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace ctr {
struct SnapshotEventIndexes { std::uint8_t first,second; };
struct OwnerEventSnapshotServices {
 // Must traverse original singleton00ab10d0 (ELF9b10d0) and table for owner+148&3.
 SnapshotEventIndexes (*lookup)(void*)=nullptr;
 void* context=nullptr;
};
// Whole00ee7370, adapted views. packed_flags is raw controller+34 (includes35/36).
// output points to original record+0c, not an entire record layout.
void update_owner_event_snapshot(std::uint16_t* output,float amount,std::uint32_t packed_flags,
 std::uint64_t event_bits,const OwnerEventSnapshotServices& services);
}
