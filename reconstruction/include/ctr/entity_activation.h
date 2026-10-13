#pragma once
#include <cstdint>
#include <span>
namespace ctr {
// Adapted fields of the registered CEntity/CActor activation target 00c70fc0.
// This is not a complete entity layout or a physics body.
struct EntityActivationView {
    std::uint32_t flags38;
    std::uint32_t flags78;
    std::uint32_t registration88;
};
struct ActivationRegistryView {
    bool initialized;
    std::span<std::uint32_t> entry_flags;
    std::span<std::uint32_t> changed_bitmap;
};
// Explicit, already initialized registry; lazy guard/mutex construction missing.
// Does not bind the callback to a kart or implement physics side effects.
void update_entity_activation(EntityActivationView& entity, bool enabled,
    ActivationRegistryView registry);
}
