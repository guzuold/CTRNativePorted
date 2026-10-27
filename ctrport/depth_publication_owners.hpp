// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "depth_guest_writeback.hpp"
namespace ctrport {
// Submission-local GPU generations. Enrollment must precede the first writer;
// a token first observed dirty has no trustworthy pre-writer snapshot and cannot
// be retroactively enrolled. Only a full verified publication retires dirtiness.
class DepthPublicationOwners {
    struct Entry {
        uint64_t token;
        uint32_t writer;
        bool dirty;
        DepthGuestWriteback backing;
        DepthGuestWriteback::Download download;
        void* context;
    };
    std::vector<Entry> entries_;
    bool running_ = false;
    static int verify(const Entry& entry, const CtrGuestCopyApi& memory) {
        try {
            for (uint32_t i = 0; i < entry.backing.count; ++i) {
                const auto& plane = entry.backing.planes[i];
                std::vector<unsigned char> current(plane.initial.size());
                const int read = image_guest_copy(memory, plane.address, current.data(), current.size(), false);
                if (read) return read;
                if (current != plane.initial) return 88;
            }
            return 0;
        } catch (...) {
            return 112;
        }
    }

public:
    int enroll(const CtrGuestCopyApi& memory, uint64_t token, const uint64_t* addresses, const uint64_t* lengths,
               uint32_t count, DepthGuestWriteback::Download download, void* context) {
        if (running_) return 91;
        if (!token) return 60;
        for (const auto& entry : entries_)
            if (entry.token == token) return 91;
        // Guest overlap between distinct live GPU owners requires explicit alias
        // authority outside this class; never independently publish both snapshots.
        if (!addresses || !lengths || !count || count > 2) return 60;
        for (uint32_t i = 0; i < count; ++i) {
            if (!addresses[i] || !lengths[i] || addresses[i] > UINT64_MAX - lengths[i]) return 60;
            for (const auto& entry : entries_)
                for (uint32_t j = 0; j < entry.backing.count; ++j) {
                    const auto& plane = entry.backing.planes[j];
                    if (addresses[i] < plane.address + plane.initial.size() &&
                        plane.address < addresses[i] + lengths[i])
                        return 88;
                }
        }
        Entry pending{token, 0, false, {}, download, context};
        const int status = DepthGuestWriteback::capture(memory, token, addresses, lengths, count, pending.backing);
        if (status) return status;
        try {
            entries_.push_back(std::move(pending));
            return 0;
        } catch (...) {
            return 112;
        }
    }
    // A newly loaded real provider may bind to a pre-writer snapshot while
    // paused. Binding cannot manufacture a missing snapshot or repair poisoning.
    // Provider/module lifetime remains owned by the executor through finish().
    int bind(uint64_t token, DepthGuestWriteback::Download download, void* context) {
        if (running_) return 91;
        if (!token || !download) return 60;
        for (auto& entry : entries_)
            if (entry.token == token) {
                if (entry.backing.poisoned) return 91;
                if (!entry.backing.count) return 88;
                entry.download = download;
                entry.context = context;
                return 0;
            }
        return 62;
    }
    // Recheck the enrolled CPU generation before pages become GPU-resident.
    // No late snapshot may replace the bytes captured before the first clear.
    int verify_pre_writer(uint64_t token, const CtrGuestCopyApi& memory) const {
        if (running_) return 91;
        if (!token) return 60;
        for (const auto& entry : entries_)
            if (entry.token == token) {
                if (entry.backing.poisoned) return 91;
                if (!entry.backing.count) return 88;
                return verify(entry, memory);
            }
        return 62;
    }
    int mark(uint64_t token, uint32_t writer) {
        if (running_) return 91;
        if (!token) return 60;
        for (auto& entry : entries_)
            if (entry.token == token) {
                if (entry.backing.poisoned) return 91;
                if (!entry.dirty && entry.backing.completed) {
                    entry.backing.completed = false;
                    for (auto& plane : entry.backing.planes) plane.completed = false;
                }
                entry.dirty = true;
                entry.writer = writer;
                return 0;
            }
        try {
            entries_.push_back({token, writer, true, {}, nullptr, nullptr});
            return 0;
        } catch (...) {
            return 112;
        }
    }
    size_t size() const {
        size_t count = 0;
        for (const auto& entry : entries_)
            if (entry.dirty) ++count;
        return count;
    }
    bool empty() const { return !size(); }
    size_t missing_snapshots() const {
        size_t count = 0;
        for (const auto& entry : entries_)
            if (entry.dirty && !entry.backing.count) ++count;
        return count;
    }
    template <class Observer> void observe(Observer&& observer) const {
        for (const auto& entry : entries_)
            observer(entry.token, entry.writer, entry.dirty, entry.backing.count, entry.backing.completed,
                     entry.backing.poisoned);
    }
    using Resident = int (*)(void*, uint64_t, uint32_t);
    int finish(const CtrGuestCopyApi& memory, Resident resident = nullptr, void* context = nullptr) {
        if (running_) return 91;
        running_ = true;
        struct Reset {
            bool& state;
            ~Reset() { state = false; }
        } reset{running_};
        // Validate prior completed publications on every resume, even if a later
        // owner still lacks a provider. A CPU mutation cannot silently signal EOP.
        for (const auto& entry : entries_)
            if (entry.backing.completed) {
                const int status = verify(entry, memory);
                if (status) return status;
            }
        bool pending = false;
        for (auto& entry : entries_)
            if (entry.dirty) {
                if (entry.backing.poisoned) return 91;
                // A genuine retained/protected GPU generation may satisfy completion while
                // remaining GPU-dirty. This does not publish or manufacture guest bytes.
                if (resident) {
                    const int status = resident(context, entry.token, entry.writer);
                    if (!status) continue;
                    if (status != 88) return status;
                }
                if (!entry.backing.count || !entry.download) {
                    pending = true;
                    continue;
                }
                const int status = entry.backing.commit(memory, entry.download, entry.context);
                if (status == 88 && !entry.backing.poisoned) {
                    pending = true;
                    continue;
                }
                if (status) return status;
                entry.dirty = false;
            }
        for (const auto& entry : entries_)
            if (entry.backing.completed) {
                const int status = verify(entry, memory);
                if (status) return status;
            }
        return pending ? 88 : 0;
    }
};
}
