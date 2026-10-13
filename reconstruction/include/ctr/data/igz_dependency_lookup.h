#pragma once
#include "ctr/data/igz_dependency_publish.h"
namespace ctr::data {
// Borrowed object storage corresponding to a pointer in the key list.
struct IgzLookupObject { std::uint64_t pointer; std::span<std::byte> storage; };
// Whole0155f410; physical RDIreceiver, RSIsecondaryhash, RDXfirstname.
// Real fallback global and terminal/weak lifetime remain missing.
std::uint64_t igz_dependency_lookup(const IgzDependencyPublisher&,
    std::uint32_t secondary_hash, const IgzPublishedName& first,
    std::span<const IgzLookupObject> objects);
}
