// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_guest_copy.hpp"
#include <memory>
#include <cstdio>
namespace ctrport {
// A CPU exclusion lease, not image tiling or a GPU image provider. Only whole
// owned RW pages are admitted: callers must establish any padded plane extent.
// The lease must outlive guest execution and retain the genuine GPU generation.
class GpuResidentMemory {
    struct Range {
        uint64_t address, bytes, image, generation;
        std::shared_ptr<void> owner;
    };
    std::vector<Range> ranges_;
    mutable std::mutex mutex_;

public:
    GpuResidentMemory() = default;
    GpuResidentMemory(const GpuResidentMemory&) = delete;
    GpuResidentMemory& operator=(const GpuResidentMemory&) = delete;
    ~GpuResidentMemory() { shutdown(); }
    int protect(uint64_t address, uint64_t bytes, uint64_t image, uint64_t generation, std::shared_ptr<void> owner,
                std::vector<unsigned char>* prior = nullptr) {
        std::lock_guard<std::mutex> state(mutex_);
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const uint64_t page = system.dwPageSize;
        if (!page || !address || !bytes || !image || !generation || !owner || address % page || bytes % page ||
            bytes > 32 * 1024 * 1024 || address > UINT64_MAX - bytes)
            return 60;
        for (const auto& range : ranges_)
            if (address < range.address + range.bytes && range.address < address + bytes) return 88;
        std::lock_guard<std::mutex> lock(owned_output_lock);
        bool owned = false;
        for (const auto& item : owned_output_extents)
            for (const auto& extent : item.second)
                if (address >= extent.first && address + bytes <= extent.second) owned = true;
        if (!owned) return 62;
        for (uint64_t cursor = address; cursor < address + bytes;) {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) || info.State != MEM_COMMIT ||
                info.Protect != PAGE_READWRITE)
                return 62;
            const uint64_t end = reinterpret_cast<uint64_t>(info.BaseAddress) + info.RegionSize;
            if (end <= cursor) return 62;
            cursor = end;
        }
        try {
            ranges_.reserve(ranges_.size() + 1);
        } catch (...) {
            return 112;
        }
        // Preserve original CPU backing before exclusion, solely as padding/prior-byte
        // authority. It is never a snapshot of the later GPU-written logical pixels.
        std::vector<unsigned char> snapshot;
        if (prior) {
            try {
                snapshot.resize(static_cast<size_t>(bytes));
            } catch (...) {
                return 112;
            }
            std::memcpy(snapshot.data(), reinterpret_cast<const void*>(address), static_cast<size_t>(bytes));
        }
        DWORD old = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(address), bytes, PAGE_NOACCESS, &old)) return 62;
        if (prior) prior->swap(snapshot);
        ranges_.push_back({address, bytes, image, generation, std::move(owner)});
        return 0;
    }
    bool identify(uint64_t address, uint64_t& image, uint64_t& generation, uint64_t& begin, uint64_t& bytes) const {
        std::lock_guard<std::mutex> state(mutex_);
        for (const auto& range : ranges_)
            if (address >= range.address && address - range.address < range.bytes) {
                image = range.image;
                generation = range.generation;
                begin = range.address;
                bytes = range.bytes;
                return true;
            }
        return false;
    }
    // Exact backing lookup only. Format/sample/layout matching remains the real
    // renderer provider's responsibility; partial overlaps cannot invent aliases.
    int resolve(uint64_t address, uint64_t bytes, uint64_t& image, uint64_t& generation,
                std::shared_ptr<void>& owner) const {
        if (!address || !bytes || address > UINT64_MAX - bytes) return 60;
        std::shared_ptr<void> selected;
        uint64_t selected_image = 0, selected_generation = 0;
        {
            std::lock_guard<std::mutex> state(mutex_);
            for (const auto& range : ranges_) {
                if (address == range.address && bytes == range.bytes) {
                    selected_image = range.image;
                    selected_generation = range.generation;
                    selected = range.owner;
                    break;
                }
                if (address < range.address + range.bytes && range.address < address + bytes) return 88;
            }
        }
        if (!selected) return 62;
        image = selected_image;
        generation = selected_generation;
        owner = std::move(selected);
        return 0;
    }
    // Called only after the real writer/fence. The page remains NOACCESS while
    // the same retained image advances; an obsolete expected generation fails.
    int advance(uint64_t address, uint64_t bytes, uint64_t image, uint64_t expected, uint64_t generation) {
        if (!generation || generation <= expected) return 60;
        std::lock_guard<std::mutex> state(mutex_);
        for (auto& range : ranges_)
            if (range.address == address && range.bytes == bytes) {
                if (range.image != image || range.generation != expected) return 88;
                range.generation = generation;
                return 0;
            }
        return 62;
    }
    // Swap a completed real GPU writer without ever exposing stale CPU pages.
    int replace(uint64_t address, uint64_t bytes, uint64_t prior, uint64_t expected, uint64_t image,
                uint64_t generation, std::shared_ptr<void> owner) {
        if (!image || !owner || generation <= expected) return 60;
        std::shared_ptr<void> released;
        {
            std::lock_guard<std::mutex> state(mutex_);
            for (auto& range : ranges_)
                if (range.address == address && range.bytes == bytes) {
                    if (range.image != prior || range.generation != expected) return 88;
                    released = std::move(range.owner);
                    range.owner = std::move(owner);
                    range.image = image;
                    range.generation = generation;
                    return 0;
                }
        }
        return 62;
    }
    // Publish a fully fenced staged result overlapping only the beginning of
    // one exclusion lease. The submitting guest is blocked in GnmSubmit; other
    // guest consumers must obey the game's GPU/EOS synchronization. This mutex
    // serializes host mappings, not unsynchronized guest CPU instructions, and
    // this transfer is NOT claimed atomic against such instructions.
    int publish_overlap_prefix(uint64_t address, const std::vector<unsigned char>& initial,
                               const std::vector<unsigned char>& result, uint64_t guard, uint64_t image,
                               uint64_t generation) {
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const uint64_t page = system.dwPageSize, bytes = result.size();
        if (!page || !address || !guard || !image || !generation || generation == UINT64_MAX ||
            initial.size() != bytes || !bytes || bytes > 32 * 1024 * 1024 || address > UINT64_MAX - bytes ||
            address >= guard || address + bytes <= guard || guard % page || (address + bytes) % page)
            return 60;
        const uint64_t prefix = guard - address, overlap = bytes - prefix;
        const auto host = reinterpret_cast<uint64_t>(result.data());
        if (host > UINT64_MAX - bytes || (host < address + bytes && address < host + bytes)) return 60;
        std::lock_guard<std::mutex> state(mutex_);
        Range* found = nullptr;
        for (auto& range : ranges_)
            if (address < range.address + range.bytes && range.address < address + bytes) {
                if (found || range.address != guard) return 88;
                found = &range;
            }
        if (!found) return 62;
        if (found->image != image || found->generation != generation || !found->owner || overlap >= found->bytes)
            return 88;
        std::lock_guard<std::mutex> lock(owned_output_lock);
        bool owned = false;
        for (const auto& item : owned_output_extents)
            for (const auto& extent : item.second)
                if (address >= extent.first && address + bytes <= extent.second) owned = true;
        if (!owned) return 62;
        for (unsigned part = 0; part < 2; ++part) {
            const uint64_t begin = part ? guard : address, end = part ? found->address + found->bytes : guard;
            const DWORD protection = part ? PAGE_NOACCESS : PAGE_READWRITE;
            for (uint64_t cursor = begin; cursor < end;) {
                MEMORY_BASIC_INFORMATION info{};
                if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) || info.State != MEM_COMMIT ||
                    info.Protect != protection)
                    return 62;
                const uint64_t next = reinterpret_cast<uint64_t>(info.BaseAddress) + info.RegionSize;
                if (next <= cursor) return 62;
                cursor = std::min(next, end);
            }
        }
        if (std::memcmp(reinterpret_cast<const void*>(address), initial.data(), static_cast<size_t>(prefix))) return 88;
        // All fallible allocation/conversion/ownership checks precede permission
        // change. No stale excluded bytes are read. Retain the exact suffix owner.
        DWORD old = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(guard), overlap, PAGE_READWRITE, &old)) return 62;
        std::memcpy(reinterpret_cast<void*>(address), result.data(), static_cast<size_t>(bytes));
        found->address += overlap;
        found->bytes -= overlap;
        ++found->generation;
        // Old full-range lookup now fails; remaining pages stay excluded. A failure
        // after this point is a poisoned publication, never an EOS/success receipt.
        return std::memcmp(reinterpret_cast<const void*>(address), result.data(), static_cast<size_t>(bytes)) ? 112 : 0;
    }
    // A completed whole GPU writer can replace an excluded suffix and extend
    // exclusion into adjacent owned RW pages. No CPU publication or old pixel reuse.
    int replace_extend(uint64_t address, uint64_t bytes, uint64_t prior, uint64_t expected, uint64_t image,
                       uint64_t generation, std::shared_ptr<void> owner, const std::vector<unsigned char>& prefix_prior,
                       std::vector<unsigned char>& output) {
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const uint64_t page = system.dwPageSize;
        if (!page || !address || !bytes || address % page || bytes % page || bytes > 32 * 1024 * 1024 ||
            address > UINT64_MAX - bytes || !image || !owner || generation <= expected)
            return 60;
        std::shared_ptr<void> released;
        {
            std::lock_guard<std::mutex> state(mutex_);
            Range* found = nullptr;
            for (auto& range : ranges_)
                if (address < range.address + range.bytes && range.address < address + bytes) {
                    if (found || range.address != address) return 88;
                    found = &range;
                }
            if (!found || found->image != prior || found->generation != expected || !found->owner ||
                found->bytes >= bytes || prefix_prior.size() != found->bytes)
                return 88;
            std::lock_guard<std::mutex> owned_lock(owned_output_lock);
            bool owned = false;
            for (const auto& item : owned_output_extents)
                for (const auto& extent : item.second)
                    if (address >= extent.first && address + bytes <= extent.second) owned = true;
            if (!owned) return 62;
            for (unsigned part = 0; part < 2; ++part) {
                const uint64_t begin = part ? address + found->bytes : address,
                               end = part ? address + bytes : address + found->bytes;
                for (uint64_t cursor = begin; cursor < end;) {
                    MEMORY_BASIC_INFORMATION info{};
                    if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) ||
                        info.State != MEM_COMMIT || info.Protect != DWORD(part ? PAGE_READWRITE : PAGE_NOACCESS))
                        return 62;
                    const uint64_t next = reinterpret_cast<uint64_t>(info.BaseAddress) + info.RegionSize;
                    if (next <= cursor) return 62;
                    cursor = std::min(next, end);
                }
            }
            std::vector<unsigned char> backing;
            try {
                backing.resize(static_cast<size_t>(bytes));
            } catch (...) {
                return 112;
            }
            std::copy(prefix_prior.begin(), prefix_prior.end(), backing.begin());
            std::memcpy(backing.data() + found->bytes, reinterpret_cast<const void*>(address + found->bytes),
                        static_cast<size_t>(bytes - found->bytes));
            DWORD old = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(address + found->bytes), bytes - found->bytes, PAGE_NOACCESS,
                                &old))
                return 62;
            released = std::move(found->owner);
            found->owner = std::move(owner);
            found->bytes = bytes;
            found->image = image;
            found->generation = generation;
            output.swap(backing);
        }
        released.reset();
        return 0;
    }
    // Shutdown only: after guest execution has joined. This restores allocation
    // permissions for destruction; it never grants a guest-visible publication.
    int shutdown() {
        std::vector<std::shared_ptr<void>> released;
        int status = 0;
        {
            std::lock_guard<std::mutex> state(mutex_);
            std::lock_guard<std::mutex> lock(owned_output_lock);
            try {
                released.reserve(ranges_.size());
            } catch (...) {
                return 112;
            }
            for (auto it = ranges_.begin(); it != ranges_.end();) {
                DWORD old = 0;
                if (!VirtualProtect(reinterpret_cast<void*>(it->address), it->bytes, PAGE_READWRITE, &old)) {
                    status = 62;
                    ++it;
                } else {
                    released.push_back(std::move(it->owner));
                    it = ranges_.erase(it);
                }
            }
        }
        // Provider destructors may revoke owned memory or query this registry.
        // Release them only after both runtime ownership locks have been dropped.
        released.clear();
        return status;
    }
    bool diagnose(EXCEPTION_POINTERS* exception) const {
        if (!exception || !exception->ExceptionRecord || !exception->ContextRecord) return false;
        const auto& record = *exception->ExceptionRecord;
        if (record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record.NumberParameters < 2 ||
            record.ExceptionInformation[0] > 1)
            return false;
        uint64_t image = 0, generation = 0, begin = 0, bytes = 0;
        const uint64_t address = record.ExceptionInformation[1];
        if (!identify(address, image, generation, begin, bytes)) return false;
        char output[512];
        const int count = snprintf(
            output, sizeof(output),
            "{\"scope\":\"GPU_resident_CPU_access_missing\",\"address\":%llu,\"image\":%llu,\"generation\":%llu,\"caller\":%llu,\"operation\":%llu,\"status\":88}\n",
            static_cast<unsigned long long>(address), static_cast<unsigned long long>(image),
            static_cast<unsigned long long>(generation), static_cast<unsigned long long>(exception->ContextRecord->Rip),
            static_cast<unsigned long long>(record.ExceptionInformation[0]));
        DWORD written = 0;
        if (count > 0 && count < int(sizeof(output)))
            WriteFile(GetStdHandle(STD_ERROR_HANDLE), output, DWORD(count), &written, nullptr);
        return true;
    }
    size_t size() const {
        std::lock_guard<std::mutex> state(mutex_);
        return ranges_.size();
    }
};
inline GpuResidentMemory runtime_gpu_resident_memory;
}
