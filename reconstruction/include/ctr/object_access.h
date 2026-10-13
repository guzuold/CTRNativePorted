#pragma once
#include <atomic>
#include <cstdint>

namespace ctr {
struct Vec3 { float x, y, z; };
struct EntityLocalView {
    Vec3 position;              // entity +0x20..0x28
    const Vec3* transform_angles; // entity +0x30 -> object +0x60..0x68
    bool has_parent;            // entity +0x10 != 0
};
Vec3 unparented_position(const EntityLocalView& entity); // 004da220
Vec3 unparented_angles(const EntityLocalView& entity, Vec3 default_angles); // 004daab0

using VectorNotification = void (*)(void* context, unsigned reason);
// 00c7c420 writes the vector before virtual slot +0xb8 is invoked with reason 1.
// The actual game's callback is unresolved; callers must supply it explicitly.
bool update_notified_vector(Vec3& current, Vec3 requested,
                            VectorNotification notify, void* context);

// Adapted ownership views, NOT the complete original classes or their ABI layout.
struct HandlePayload;
struct HandleRecord {
    std::atomic<std::uint32_t> flags_and_count{0}; // original record +0
    HandlePayload* payload = nullptr;            // original record +0x28
};
struct HandlePayload {
    HandleRecord* canonical = nullptr; // original payload +0x10
};
// Reconstructs 014daa70 when the canonical handle is stable and destruction is
// not triggered. Atomic counters are preserved; concurrent slot lifetime and
// recursive resolver/release callbacks have not been reconstructed or tested.
void rebind_stable_handle(HandleRecord*& slot);
// Read-only inline traversal at the entry of 00efb8f0. Returns absence, which the
// caller must reject before dereferencing the REQUIRED archetype at payload +0xa8.
HandlePayload* inspect_handle_chain(HandleRecord* record);
}
