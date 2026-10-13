#pragma once
#include "ctr/direction.h"
#include "ctr/scalar_speed.h"
namespace ctr {
struct DrivingArchetypeView {
    const LinearVelocityProfile* forward;
    const LinearVelocityProfile* in_air;
    const LinearVelocityProfile* reverse;
};
// Adapted mandatory payload view: original component +0xa8 points to archetype.
struct DrivingComponentView { const DrivingArchetypeView* archetype; };
struct DrivingPrefixResult {
    Vec3 forward, side, previous_velocity;
    float longitudinal_speed;
    SpeedMode mode;
    unsigned direction; // original r8: forward=0, neutral=1, reverse=2
};
// Bounded 00efb8f0 through 00efbd91. Contract: +1fc=0, +d0=0,
// +68=0, +35=0, no +168 modifier handle; unparented body.
// Does not advance time or notify/write the body's velocity.
DrivingPrefixResult prepare_airborne_driving(const DrivingComponentView& component,
    const EntityLocalView& body, Vec3 defaults, Vec3 velocity,
    bool flag34, bool flag36);
struct AirborneSpeedResult {
    DrivingPrefixResult prepared;
    float speed;
    unsigned profile; // neutral=0, in_air=1, reverse=2
};
// Continues through the first scalar update/sign restoration, before force
// assembly, clock-dependent corrections and body notification.
AirborneSpeedResult advance_airborne_speed(const DrivingComponentView& component,
    const EntityLocalView& body, Vec3 defaults, Vec3 velocity,
    bool flag34, bool flag36, float dt, float steering);
struct AirborneVectorResult { Vec3 velocity; float normalized_speed; float boost; };
struct ContactAge { bool present = false; std::int32_t ticks = 0; };
// Original +0xd0/+0xd8 inputs. This view does not create or query a contact.
struct DrivingContactState { std::int32_t state = 0; bool correction_enabled = false; };
// Signed elapsed ticks from kart +0x17c; separate from lateral contact +0x150.
AirborneSpeedResult correct_airborne_longitudinal_speed(const AirborneSpeedResult& updated,
    std::int32_t elapsed_ticks, DrivingContactState contact = {});
// Profile +0x14 lateral coefficients supplied separately from the scalar view.
// Optional known contact age controls the original half-second lateral override.
// Height limit and contact modifier remain outside this contract.
AirborneVectorResult assemble_airborne_vector(const DrivingComponentView& component,
    const AirborneSpeedResult& updated, float forward_retention,
    float air_retention, float reverse_retention, ContactAge contact_age = {});
// Explicit notification service; actual game callback remains to reconstruct.
bool apply_airborne_vector(Vec3& current, const AirborneVectorResult& result,
    VectorNotification notify, void* context);
}
