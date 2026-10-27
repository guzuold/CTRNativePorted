// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "tiled_surface.hpp"
#include "../video_label_memory.hpp"
#include <array>
#include <mutex>
#include <condition_variable>
// Owner-thread surface registry. A retained mapping lease must outlive every entry.
// Labels use retained runtime-owned checked guest-visible128B memory.
// Never infer GPU readiness from registration or zero labels.
class CtrTiledSurfaceRegistry {
    struct Group {
        bool occupied = false;
        uint32_t first = 0, count = 0;
    };
    struct Slot {
        int32_t group = -1;
        std::unique_ptr<CtrTiledSurface> surface;
        std::shared_ptr<ctrport::DirectMappingLease> lease;
    };
    std::array<Group, 4> groups_{};
    std::array<Slot, 16> slots_{};
    std::shared_ptr<ctrport::VideoLabelMemory> labelMemory_ = std::make_shared<ctrport::VideoLabelMemory>();
    std::mutex labelMutex_;
    std::condition_variable labelChanged_;
    uint64_t generation_ = 0;

public:
    int AdmitMemoryChange(uint64_t address, uint64_t bytes) const {
        if (!bytes || address > UINT64_MAX - bytes) return 60;
        const auto overlaps = [&](uint64_t other, uint64_t length) {
            return address < other + length && other < address + bytes;
        };
        for (const auto& slot : slots_)
            if (slot.surface && overlaps(slot.surface->guestAddress, slot.surface->guestBytes)) return 88;
        if (labelMemory_ && overlaps(labelMemory_->address(), 128)) return 88;
        return 0;
    }
    int Register(VkDevice device, PFN_vkGetDeviceProcAddr get, const VkPhysicalDeviceMemoryProperties& properties,
                 uint32_t first, uint32_t count, const CtrVideoBufferAttributesV1& attributes,
                 const uint64_t* addresses, uint32_t& output) {
        if (!addresses || !count || first >= 16 || count > 16 - first) return 60;
        uint32_t group = 0;
        while (group < 4 && groups_[group].occupied) ++group;
        if (group == 4) return 108;
        for (uint32_t i = 0; i < count; ++i)
            if (slots_[first + i].group != -1) return 109;
        std::array<std::unique_ptr<CtrTiledSurface>, 16> prepared;
        // Prepare every real resource before committing any slot/group/label.
        for (uint32_t i = 0; i < count; ++i) {
            const int status = CtrTiledSurface::Create(device, get, properties, attributes, addresses[i], prepared[i]);
            if (status) return status;
        }
        {
            std::lock_guard<std::mutex> lock(labelMutex_);
            std::array<uint64_t, 16> labels{};
            auto memory = labelMemory_->api();
            if (memory.read(memory.context, labelMemory_->address(), labels.data(), sizeof(labels))) return 112;
            for (uint32_t i = 0; i < count; ++i) labels[first + i] = 0;
            if (memory.write(memory.context, labelMemory_->address(), labels.data(), sizeof(labels))) return 112;
            for (uint32_t i = 0; i < count; ++i) {
                auto& slot = slots_[first + i];
                slot.group = static_cast<int32_t>(group);
                slot.surface = std::move(prepared[i]);
            }
            groups_[group] = {true, first, count};
            ++generation_;
        }
        labelChanged_.notify_all();
        output = group;
        return 0;
    }
    int RegisterMapped(VkDevice device, PFN_vkGetDeviceProcAddr get, const VkPhysicalDeviceMemoryProperties& properties,
                       uint32_t first, uint32_t count, const CtrVideoBufferAttributesV1& attributes,
                       const uint64_t* addresses, const std::shared_ptr<ctrport::DirectMemoryBacking>& backing,
                       uint32_t& output) {
        if (!backing || !addresses || !count || first >= 16 || count > 16 - first) return 60;
        std::array<std::shared_ptr<ctrport::DirectMappingLease>, 16> leases;
        try {
            for (uint32_t i = 0; i < count; ++i)
                leases[i] = std::make_shared<ctrport::DirectMappingLease>(backing, addresses[i], 8355840);
        } catch (const std::runtime_error&) {
            return 112;
        }
        const int status = Register(device, get, properties, first, count, attributes, addresses, output);
        if (status) return status;
        for (uint32_t i = 0; i < count; ++i) slots_[first + i].lease = std::move(leases[i]);
        return 0;
    }
    int Unregister(uint32_t group) {
        if (group >= 4 || !groups_[group].occupied) return 62;
        const auto entry = groups_[group];
        // No GPU submissions reference these resources yet. Future queue must retain
        // references/fences and wait before this owner releases images.
        for (uint32_t i = 0; i < entry.count; ++i) {
            auto& slot = slots_[entry.first + i];
            slot.surface.reset();
            slot.lease.reset();
            slot.group = -1;
        }
        groups_[group] = {};
        return 0;
    }
    const CtrTiledSurface* Surface(uint32_t slot) const { return slot < 16 ? slots_[slot].surface.get() : nullptr; }
    uint64_t LabelAddress() const { return labelMemory_->address(); }
    std::shared_ptr<ctrport::VideoLabelMemory> RetainLabelMemory() const { return labelMemory_; }
    int LabelSnapshot(std::array<uint64_t, 16>& output, uint64_t& generation) {
        std::lock_guard<std::mutex> lock(labelMutex_);
        auto memory = labelMemory_->api();
        std::array<uint64_t, 16> value{};
        if (memory.read(memory.context, labelMemory_->address(), value.data(), sizeof(value))) return 112;
        output = value;
        generation = generation_;
        return 0;
    }
    // Caller must establish genuine GPU completion before publishing a GPU result.
    // This checked write itself is no fence and gives no completion credit.
    int PublishLabel(uint32_t slot, uint64_t value) {
        if (slot >= 16) return 60;
        {
            std::lock_guard<std::mutex> lock(labelMutex_);
            auto memory = labelMemory_->api();
            if (memory.write(memory.context, labelMemory_->address() + slot * 8, &value, 8)) return 112;
            ++generation_;
        }
        labelChanged_.notify_all();
        return 0;
    }
    int WaitLabel(uint32_t slot, uint64_t expected, std::chrono::milliseconds timeout) {
        if (slot >= 16 || timeout.count() < 0 || timeout.count() > 5000) return 60;
        std::unique_lock<std::mutex> lock(labelMutex_);
        int error = 0;
        const auto predicate = [&] {
            uint64_t value = 0;
            auto memory = labelMemory_->api();
            if (memory.read(memory.context, labelMemory_->address() + slot * 8, &value, 8)) {
                error = 112;
                return true;
            }
            return value == expected;
        };
        const bool ready = labelChanged_.wait_for(lock, timeout, predicate);
        return error ? error : ready ? 0 : 52;
    }
};
