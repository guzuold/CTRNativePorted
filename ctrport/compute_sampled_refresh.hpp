// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
namespace ctrport {
// Refresh an existing sampled image from a completed, verified compute guest
// generation. Providers must perform real upload/fence and download/retile.
// Ready is published only after byte comparison and a second guest reread.
// This does not make a frame complete or admit a draw's other dependencies.
using ComputeSampledUpload = int (*)(void*, const unsigned char*, uint64_t);
inline int refresh_compute_sampled(const ComputeGuestWriteback& owner, const CtrGuestCopyApi& memory,
                                   ComputeSampledUpload upload, ComputeTiledDownload download, void* context,
                                   uint64_t& compared) {
    compared = 0;
    if (!owner.completed || owner.poisoned) return 88;
    if (memory.version != 1 || memory.size != sizeof(memory) || !memory.read || !owner.address ||
        owner.initial.empty() || owner.initial.size() > 16 * 1024 * 1024 ||
        owner.address > UINT64_MAX - owner.initial.size())
        return 60;
    if (!upload || !download) return 88;
    try {
        std::vector<unsigned char> current(owner.initial.size()), returned(owner.initial.size());
        int status = memory.read(memory.context, owner.address, current.data(), current.size());
        if (status) return status;
        if (current != owner.initial) return 88;
        status = upload(context, owner.initial.data(), owner.initial.size());
        if (status) return status;
        status = download(context, owner.initial.data(), owner.initial.size(), returned.data(), returned.size());
        if (status) return status;
        if (returned != owner.initial) return 112;
        status = memory.read(memory.context, owner.address, current.data(), current.size());
        if (status) return status;
        if (current != owner.initial) return 88;
        compared = owner.initial.size();
        return 0;
    } catch (...) {
        return 112;
    }
}
}
