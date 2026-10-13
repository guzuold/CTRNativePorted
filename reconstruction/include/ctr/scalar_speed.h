#pragma once
#include <array>
#include <cstdint>

namespace ctr {
// Decoded view of the original 84-byte curve at acceleration record +0x14.
// Names are reconstructed; no assertion that these were the original C++ names.
class QuantizedCurve {
public:
    explicit QuantizedCurve(const std::array<std::uint8_t, 84>& bytes);
    float evaluate(float time) const; // FUN_003f22b0, deterministic branch
    float inverse(float value, int iterations = 10) const; // FUN_00b82790
private:
    std::array<std::uint8_t, 84> bytes_;
    int count_;
    float time_at(int index) const;
    float scale_at(int index) const;
    float signed_byte(int offset) const;
    float value_at(int index) const;
};

struct AccelerationData {
    float duration;         // CKartLinearAccelerationData +0x0c
    float turning_duration;// +0x10; <=0 disables turning interpolation
    QuantizedCurve curve;  // +0x14
};
struct LinearVelocityProfile {
    float maximum_speed; // CKartLinearVelocityData +0x0c
    const AccelerationData* acceleration; // +0x18
    const AccelerationData* natural_deceleration; // +0x20
    const AccelerationData* braking; // +0x28
};
enum class SpeedMode : int { Accelerate = 0, Coast = 1, Brake = 2 };
struct SpeedResult { float speed; float inverse_time; };

// Reconstructed subset of FUN_00eff240. Explicit contract: no registered queried
// component, no special curve handle (+0x98), +0x178=1, +0x180=1, +0x184=0.
// This is not a substitute for component resolution or a complete kart update.
SpeedResult advance_without_components(float speed, float dt, float steering,
                                       const LinearVelocityProfile& profile, SpeedMode mode);
}
