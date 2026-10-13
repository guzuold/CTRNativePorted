#include "ctr/data/igz_dependency_lookup.h"
#include "ctr/object_nonterminal_release.h"
#include <cstring>
#include <stdexcept>
namespace ctr::data {
std::uint64_t igz_dependency_lookup(const IgzDependencyPublisher& p,
    std::uint32_t secondary_hash, const IgzPublishedName& first,
    std::span<const IgzLookupObject> objects) {
    if (!p.first_initialized) return 0;
    // The first-list count, rather than the key count, controls this scan.
    for (std::size_t i = 0; i < p.first.size(); ++i) {
        if (p.first[i].hash != first.hash) continue;
        if (!p.second_initialized)
            throw std::runtime_error("missing original fallback global026edc00");
        if (i >= p.second.size()) throw std::runtime_error("second list extent");
        if (p.second[i].hash != secondary_hash) continue;
        if (i >= p.keys.size()) throw std::runtime_error("key list extent");
        const auto result = p.keys[i];
        if (!result) return 0;
        for (const auto& object : objects) {
            if (object.pointer != result) continue;
            if (object.storage.size() < 12) throw std::runtime_error("object header extent");
            // This interface is bounded to resolved single-thread storage, as
            // is the verified core release interface. No concurrent credit.
            std::uint32_t flags;
            std::memcpy(&flags, object.storage.data() + 8, 4);
            ++flags;
            std::memcpy(object.storage.data() + 8, &flags, 4);
            ctr::release_nonterminal_object_storage(object.storage);
            return result;
        }
        throw std::runtime_error("missing original object storage");
    }
    return 0;
}
}
