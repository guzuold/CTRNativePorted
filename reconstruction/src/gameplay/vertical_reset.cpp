#include "ctr/gameplay/vertical_reset.h"
#include "ctr/object_nonterminal_release.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void reset_vertical_payload(VerticalResetHandle*& slot) {
    auto* handle = slot;
    VerticalResetPayload* payload = nullptr;
    for (unsigned depth = 0; handle; ++depth) {
        if (depth >= 64) throw std::invalid_argument("Missing unbounded handle chain");
        if (handle->flags & 0x10000000u)
            throw std::invalid_argument("Missing mutating handle resolver014daa70");
        payload = handle->payload;
        if (!(handle->flags & 0x20000000u) || !payload) break;
        handle = payload->canonical;
        payload = nullptr;
    }
    if (!payload) return;
    // Refuse unresolved terminal destruction before modifying borrowed storage.
    const auto threshold = (slot->flags & 0x90000000u) ? 3u : 2u;
    if ((slot->flags & 0xffffu) == threshold)
        throw std::invalid_argument("Missing terminal handle release014da170");
    std::uint32_t count;
    std::memcpy(&count,payload->header.data()+8,4);
    ++count;
    std::memcpy(payload->header.data()+8,&count,4);
    if (!payload->mode) payload->mode = 7;
    payload->value = 0;
    --slot->flags;
    slot = nullptr;
    release_nonterminal_object_storage(payload->header);
}
}
