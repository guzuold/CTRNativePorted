#include "ctr/entity_activation.h"
#include <stdexcept>
namespace ctr {
void update_entity_activation(EntityActivationView& entity, bool enabled,
    ActivationRegistryView registry) {
    if (((entity.flags38 >> 13) & 1u) == static_cast<unsigned>(enabled)) return;
    const bool registered = (entity.flags38 & 4u) != 0;
    const auto index = entity.registration88 & 0xfffu;
    // Missing services reject before writes; no fake successful initialization.
    if (registered && (!registry.initialized || index >= registry.entry_flags.size() ||
        (index >> 5) >= registry.changed_bitmap.size()))
        throw std::invalid_argument("Required activation registry unavailable");
    entity.flags78 = enabled ? entity.flags78 | 0x8000u : entity.flags78 & ~0x8000u;
    if (registered) {
        auto& entry = registry.entry_flags[index];
        if ((entry & 0x8000u) != (entity.flags78 & 0x8000u)) {
            auto& bitmap = registry.changed_bitmap[index >> 5];
            const auto bit = 1u << (index & 31u);
            if (enabled) { entry |= 0x8000u; bitmap |= bit; }
            else { entry &= ~0x8000u; bitmap &= ~bit; }
        }
    }
    entity.flags38 = (entity.flags38 & ~0x2000u) | (static_cast<unsigned>(enabled) << 13);
}
}
