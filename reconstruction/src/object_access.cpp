#include "ctr/object_access.h"
#include <cmath>
#include <stdexcept>

namespace ctr {
Vec3 unparented_position(const EntityLocalView& entity) {
    if (entity.has_parent) throw std::invalid_argument("Missing parent matrix callback +0xb0");
    return entity.position;
}
Vec3 unparented_angles(const EntityLocalView& entity, Vec3 default_angles) {
    if (entity.has_parent) throw std::invalid_argument("Missing parent quaternion callback +0xc0");
    return entity.transform_angles ? *entity.transform_angles : default_angles;
}
bool update_notified_vector(Vec3& current, Vec3 requested,
                            VectorNotification notify, void* context) {
    const auto finite = [](Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (!finite(current) || !finite(requested)) throw std::invalid_argument("Non-finite vector contract");
    constexpr float epsilon = 4.999999987376214e-7f; // DAT_01c98700
    if (std::fabs(requested.x - current.x) <= epsilon &&
        std::fabs(requested.y - current.y) <= epsilon &&
        std::fabs(requested.z - current.z) <= epsilon) return false;
    if (!notify) throw std::invalid_argument("Missing vector-change callback +0xb8");
    current = requested;
    notify(context, 1);
    return true;
}
void rebind_stable_handle(HandleRecord*& slot) {
    if (!slot || !slot->payload) return;
    HandleRecord* target = slot->payload->canonical;
    if (target && (target->flags_and_count.load() & 0x10000000u))
        throw std::invalid_argument("Recursive canonical resolver 014daa70 missing");
    const auto before = slot->flags_and_count.load();
    const auto threshold = (before & 0x90000000u) ? 3u : 2u;
    // Refuse before mutating anything: original destruction calls 014da170.
    // If target==slot, its increment changes the old value used by the threshold.
    const auto before_release = before + (target == slot ? 1u : 0u);
    if ((before_release & 0xffffu) == threshold)
        throw std::invalid_argument("Handle destruction callback 014da170 missing");
    if (target) target->flags_and_count.fetch_add(1);
    slot->flags_and_count.fetch_sub(1);
    slot = target;
}
HandlePayload* inspect_handle_chain(HandleRecord* record) {
    for (int depth = 0; depth < 64; ++depth) {
        if (!record) return nullptr;
        const auto flags = record->flags_and_count.load();
        if (flags & 0x10000000u) throw std::invalid_argument("Handle requires mutating resolver");
        auto* payload = record->payload;
        if (!(flags & 0x20000000u) || !payload) return payload;
        record = payload->canonical;
    }
    throw std::invalid_argument("Unbounded or cyclic handle chain");
}
}
