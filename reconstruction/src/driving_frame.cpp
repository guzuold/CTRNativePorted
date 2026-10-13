#include "ctr/driving_frame.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace ctr {
DrivingPrefixResult prepare_airborne_driving(const DrivingComponentView& component,
    const EntityLocalView& body, Vec3 defaults, Vec3 velocity,
    bool flag34, bool flag36) {
    if (!component.archetype || !component.archetype->forward ||
        !component.archetype->in_air || !component.archetype->reverse)
        throw std::invalid_argument("Missing required driving archetype/profile");
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z))
        throw std::invalid_argument("Non-finite velocity");
    const auto a = unparented_angles(body, defaults);
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z))
        throw std::invalid_argument("Non-finite angles");
    const float cy = a.y == 0.f ? 1.f : std::cos(a.y);
    const float cz = a.z == 0.f ? 1.f : std::cos(a.z);
    const float sz = a.z == 0.f ? 0.f : std::sin(a.z);
    const float x = cy * cz, y = cy * sz;
    // +1fc=0 and +d0!=2 selects horizontal projection, before normalization.
    const float reciprocal = 1.f / std::sqrt(((x*x + 0.f) + y*y) + 0.f + std::numeric_limits<float>::min());
    const Vec3 forward{reciprocal*x, reciprocal*y, reciprocal*0.f};
    const auto up = entity_up(body, defaults);
    Vec3 side{up.y*forward.z - up.z*forward.y,
              up.z*forward.x - forward.z*up.x,
              forward.y*up.x - up.y*forward.x};
    const float square = ((side.x*side.x + 0.f) + side.y*side.y) + side.z*side.z;
    if (square < 1.4210854715202004e-14f) side = entity_side(body, defaults);
    else {
        const float scale = 1.f/std::sqrt(square);
        side = {scale*side.x, scale*side.y, scale*side.z};
    }
    const float speed = ((velocity.x*forward.x + 0.f) + velocity.y*forward.y) + velocity.z*forward.z;
    SpeedMode mode;
    unsigned direction;
    if (speed > 50.f) {
        direction = 0;
        mode = flag34 ? SpeedMode::Accelerate : SpeedMode::Coast;
    } else if (speed < -50.f) {
        direction = 2;
        mode = flag36 ? SpeedMode::Accelerate : SpeedMode::Coast;
    } else if (!flag34) {
        direction = flag36 ? 2u : 1u;
        mode = flag36 ? SpeedMode::Accelerate : SpeedMode::Brake;
    } else {
        direction = 0;
        mode = SpeedMode::Accelerate;
    }
    return {forward,side,velocity,speed,mode,direction};
}
AirborneSpeedResult advance_airborne_speed(const DrivingComponentView& component,
    const EntityLocalView& body, Vec3 defaults, Vec3 velocity,
    bool flag34, bool flag36, float dt, float steering) {
    auto prepared = prepare_airborne_driving(component,body,defaults,velocity,flag34,flag36);
    if (!std::isfinite(dt) || dt < 0.f || !std::isfinite(steering))
        throw std::invalid_argument("Invalid airborne time/steering");
    if (prepared.direction == 1) return {prepared,0.f,0};
    const bool reverse = prepared.direction == 2;
    const auto& profile = *(reverse ? component.archetype->reverse : component.archetype->in_air);
    const float input = reverse ? -prepared.longitudinal_speed : prepared.longitudinal_speed;
    const auto updated = advance_without_components(input,dt,steering,profile,prepared.mode);
    return {prepared,reverse ? -updated.speed : updated.speed,reverse ? 2u : 1u};
}
AirborneSpeedResult correct_airborne_longitudinal_speed(const AirborneSpeedResult& updated,
    std::int32_t elapsed_ticks, DrivingContactState contact) {
    auto result=updated;
    const bool forced = contact.state == 2 && contact.correction_enabled;
    if (!forced && static_cast<float>(elapsed_ticks)*0.0001220703125f >= 2.f) return result;
    const auto v=updated.prepared.previous_velocity;
    const float horizontal=std::sqrt(((v.x*v.x+0.f)+v.y*v.y)+0.f+std::numeric_limits<float>::min());
    if (horizontal<=50.f) return result;
    const float reciprocal=1.f/horizontal;
    const auto f=updated.prepared.forward;
    // Preserve the original packed lane calculation, including its zero term.
    const float alignment=((reciprocal*v.x*f.x+0.f)+(reciprocal*v.y)*f.y)+0.f;
    if (alignment>0.7070000171661377f) result.speed=horizontal-0.f; // no boost in this contract
    return result;
}
AirborneVectorResult assemble_airborne_vector(const DrivingComponentView& component,
    const AirborneSpeedResult& updated, float forward_retention,
    float air_retention, float reverse_retention, ContactAge contact_age) {
    if (!component.archetype || !component.archetype->forward)
        throw std::invalid_argument("Missing forward normalization profile");
    const float maximum=component.archetype->forward->maximum_speed;
    if (!std::isfinite(maximum) || maximum<=0.f || !std::isfinite(updated.speed) ||
        !std::isfinite(forward_retention) || !std::isfinite(air_retention) || !std::isfinite(reverse_retention))
        throw std::invalid_argument("Invalid airborne vector parameters");
    const auto& p=updated.prepared;
    const float speed=0.f+updated.speed; // Original adds the absent boost, including zero sign.
    const float ratio=std::fabs(speed)/maximum;
    Vec3 previous=p.previous_velocity;
    if (ratio>1.f) {
        const float scale=1.f/ratio;
        previous={scale*previous.x,scale*previous.y,scale*previous.z};
    }
    float coefficient=updated.profile==2 ? reverse_retention :
        (updated.profile==1 ? air_retention : forward_retention);
    if (contact_age.present && static_cast<float>(contact_age.ticks)*0.0001220703125f < 0.5f)
        coefficient=0.8999999761581421f;
    const float lateral=coefficient*(((previous.x*p.side.x+0.f)+previous.y*p.side.y)+previous.z*p.side.z);
    Vec3 result{lateral*p.side.x+speed*p.forward.x,
                lateral*p.side.y+speed*p.forward.y,
                p.previous_velocity.z}; // Original airborne branch restores vertical velocity.
    return {result,std::clamp(ratio,0.f,1.f),0.f};
}
bool apply_airborne_vector(Vec3& current, const AirborneVectorResult& result,
    VectorNotification notify, void* context) {
    return update_notified_vector(current,result.velocity,notify,context);
}
}
