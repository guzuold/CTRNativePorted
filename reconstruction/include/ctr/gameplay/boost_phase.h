#pragma once
#include <cstdint>
namespace ctr {
struct BoostPhasePayload;
struct BoostPhaseHandle { std::uint32_t flags; const BoostPhasePayload* payload; };
struct BoostPhasePayload {
 const BoostPhaseHandle* canonical;
 unsigned char enabled, gate35, gate34, gate36;
 std::int32_t marker40;
};
struct BoostPhaseTimer {
 bool active; std::int32_t now,started,finished;
 float delay,duration,threshold;
};
// 00ee6fb0, borrowed stable views; resolver mutations remain missing.
bool boost_phase_complete(const BoostPhaseHandle* first,const BoostPhaseHandle* second,
 unsigned char input3a,const BoostPhaseTimer* timer);
}
