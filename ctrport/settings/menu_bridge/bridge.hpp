// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../menu_protocol.hpp"
#include "../../retail_map.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>

extern "C" std::uint64_t ctr_settings_guest_call4(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
extern "C" void ctr_settings_event_thunk();
extern "C" void ctr_settings_index_thunk();

namespace ctrport::settings::bridge {
using Word = std::uint64_t;
constexpr char retail_sha[] = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
constexpr std::size_t selector_vtable = 0x27746c8;
constexpr std::size_t event_rva = 0x127f500, index_rva = 0x127f5d0, setter_rva = 0x125d880;
constexpr std::size_t slot0_rva = 0x127f4c0, load_event_type = 0x2a21ee8; // EU 1.21 offsets (retail_map.hpp)
constexpr std::size_t object_size = 0x1d0, option_offset = 0x1c8, player_offset = 0x1cc, index_offset = 0x1a0;

inline bool accessible(const void* pointer, std::size_t size, bool write = false) noexcept {
    auto p = reinterpret_cast<std::uintptr_t>(pointer);
    if (!p || size > UINTPTR_MAX - p) return false;
    const auto end = p + size;
    while (p < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(p), &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const DWORD protection = info.Protect & 0xff;
        if (write && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
            return false;
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= p) return false;
        p = next;
    }
    return true;
}
template <class T> inline T read(const void* p, std::size_t offset = 0) {
    const auto base = reinterpret_cast<std::uintptr_t>(p);
    if (offset > UINTPTR_MAX - base) throw std::runtime_error("settings: retail pointer overflow");
    const auto* address = reinterpret_cast<const void*>(base + offset);
    if (!accessible(address, sizeof(T))) throw std::runtime_error("settings: unreadable retail object");
    T value{};
    std::memcpy(&value, address, sizeof(T));
    return value;
}

struct Hooks {
    // Called on the original GUI thread. Must not throw or retain guest pointers.
    // Required error sink: the integration decides how to display a failed save.
    void (*error)(const char*) noexcept = nullptr;
    void (*saved_restart_required)(const char*, const Json&) noexcept = nullptr;
    // Optional modes need the renderer's positive capability acknowledgement.
    bool (*available)(menu::Option, unsigned) noexcept = nullptr;
    // Called on the GUI thread right after the in-memory request changes (the durable
    // save follows on the writer thread); no guest pointers retained.
    bool (*publish_live)(menu::Option, const Json&) noexcept = nullptr;
};

class Adapter {
    unsigned char* image_;
    // Where the reference bindings are in the running eboot (retail_map.hpp).
    std::size_t vtable_ = 0, event_rva_ = 0, index_rva_ = 0, setter_ = 0, slot0_ = 0, load_type_ = 0;
    Store store_;
    const Json applied_;
    Hooks hooks_;
    bool installed_ = false;
    void* event_ = nullptr;
    void* index_ = nullptr;
    std::recursive_mutex callbacks_;
    static inline std::atomic<Adapter*> active_{nullptr};
    // Requested configuration, read once: GUI callbacks never touch the disk. Edits are
    // queued to one writer thread that merges them under Store's cross-process lock.
    Json requested_;
    std::mutex queue_mutex_;
    std::condition_variable queue_wake_;
    std::optional<Json> queued_;
    bool writer_started_ = false;
    // Every GUI event of every selector of the game passes through event(): many per frame.
    // Whether an event class is the load event is decided once per event vtable.
    std::array<std::pair<const void*, bool>, 64> load_kinds_{};
    std::size_t load_kind_count_ = 0;

    void enqueue(const Json& changes) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!queued_) queued_ = Json::object();
            for (const auto& item : changes.members) queued_->set(item.first, item.second);
        }
        queue_wake_.notify_one();
    }
    void writer() noexcept {
        for (;;) {
            Json batch;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_wake_.wait(lock, [&] { return queued_.has_value(); });
                lock.unlock();
                // Coalesce quick successive selections into one durable write.
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                lock.lock();
                batch = std::move(*queued_);
                queued_.reset();
            }
            try {
                store_.update(batch);
            } catch (const std::exception& e) {
                error(e.what());
            }
        }
    }

    [[noreturn]] static void fatal(const char* message) noexcept {
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
        // An invalid guest contract cannot safely resume through a guessed ABI.
        TerminateProcess(GetCurrentProcess(), 4);
        std::terminate();
    }
    void error(const char* message) const noexcept { hooks_.error(message); }
    void* at(std::size_t rva) const noexcept { return image_ + rva; }
    Word original(void* fn, Word a, Word b = 0, Word c = 0, Word d = 0) const noexcept {
        return ctr_settings_guest_call4(fn, a, b, c, d);
    }
    bool own(void* object, menu::Option& option) const {
        if (!accessible(object, object_size) || read<void*>(object) != at(vtable_))
            throw std::runtime_error("settings: unexpected selector instance/vtable");
        const auto id = read<std::uint32_t>(object, option_offset);
        if (!menu::owns(id)) return false;
        if (read<unsigned char>(object, player_offset))
            throw std::runtime_error("settings: PC selector must not use player-specific storage");
        option = static_cast<menu::Option>(id);
        return true;
    }
    bool allowed(menu::Option option, unsigned index) const noexcept {
        if (hooks_.available) return hooks_.available(option, index);
        return option != menu::Option::upscaler || index < 2;
    }
    unsigned requested(menu::Option option, const Json& document) const {
        const auto* value = document.find(menu::key(option));
        Json fallback;
        if (!value) {
            switch (option) {
            case menu::Option::scene_height: fallback = Json::of(std::to_string(scene_height(document))); break;
            case menu::Option::fps_60:
            case menu::Option::xbox_prompts: fallback = Json::of(true); break;
            default: fallback = menu::value(option, 0); break;
            }
            value = &fallback;
        }
        for (unsigned n = 0; n != menu::count(option); ++n)
            if (*value == menu::value(option, n)) return n;
        throw std::runtime_error("settings: saved value has no menu index");
    }
    // The event's class is fixed by its vtable: the full ancestry check (memory checks and a
    // guest metadata call) runs once per class, not once per event.
    bool load_event_cached(void* event) {
        const void* vtable = nullptr;
        std::memcpy(&vtable, event, sizeof(vtable)); // a live event the game is dispatching
        for (std::size_t n = 0; n != load_kind_count_; ++n)
            if (load_kinds_[n].first == vtable) return load_kinds_[n].second;
        const bool loading = load_event(event);
        if (load_kind_count_ != load_kinds_.size()) load_kinds_[load_kind_count_++] = {vtable, loading};
        return loading;
    }
    bool load_event(void* event) const {
        auto* vtable = read<void*>(event);
        auto* getter = read<void*>(vtable);
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(getter, &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            throw std::runtime_error("settings: invalid event metadata getter");
        auto* meta = reinterpret_cast<void*>(original(getter, reinterpret_cast<Word>(event)));
        const auto* target = read<void*>(at(load_type_));
        if (!target) throw std::runtime_error("settings: load event metadata is not initialized");
        // Retail tests the TARGET type's ancestry flag, not each child type's.
        // A derived event with its own zero flag can still descend from target.
        const bool inherited = read<Word>(target, 0x48) != 0;
        std::array<void*, 64> seen{};
        for (unsigned n = 0; meta; ++n) {
            if (n == seen.size()) throw std::runtime_error("settings: event metadata ancestry exceeds bound");
            if (meta == target) return true;
            for (unsigned p = 0; p < n; ++p)
                if (seen[p] == meta) throw std::runtime_error("settings: cyclic event metadata");
            seen[n] = meta;
            if (!inherited) return false;
            meta = read<void*>(meta, 0x40);
        }
        return false;
    }
    static void signature(void* pointer, std::size_t reference, const std::array<unsigned char, 16>& bytes) {
        if (!accessible(pointer, bytes.size()) ||
            !retail_map::same(static_cast<const unsigned char*>(pointer), reference, bytes.data(), bytes.size()))
            throw std::runtime_error("settings: retail callback signature mismatch; bridge not installed");
    }

public:
    Adapter(void* image, const std::string& sha256, fs::path path, Hooks hooks, const Json* startup = nullptr)
        : image_(static_cast<unsigned char*>(image)), store_(std::move(path)),
          applied_(startup ? *startup : store_.read()), hooks_(hooks) {
        if (!image_ || !retail_map::bound(sha256)) throw std::runtime_error("settings: unsupported executable SHA256");
        vtable_ = retail_map::at(selector_vtable);
        event_rva_ = retail_map::at(event_rva);
        index_rva_ = retail_map::at(index_rva);
        setter_ = retail_map::at(setter_rva);
        slot0_ = retail_map::at(slot0_rva);
        load_type_ = retail_map::at(load_event_type);
        if (!hooks_.error) throw std::invalid_argument("settings: an error display sink is required");
        validate(applied_);
        requested_ = store_.read();
    }
    Adapter(const Adapter&) = delete;
    Adapter& operator=(const Adapter&) = delete;
    ~Adapter() {
        if (installed_) fatal("settings: adapter destroyed before quiescent uninstall");
    }
    const Json& applied_at_startup() const noexcept { return applied_; }
    Json requested_document() {
        std::lock_guard<std::recursive_mutex> lock(callbacks_);
        return requested_;
    }
    // Re-read the requested document after another writer changed it (for example a
    // control fixture). Never called from a GUI callback.
    void reload() {
        std::lock_guard<std::recursive_mutex> lock(callbacks_);
        requested_ = store_.read();
    }

    // Startup/quiescent operations only: the parent must call before guest threads
    // start, and keep this object alive until every guest GUI callback has ended.
    // The original image's vtable page must still be writable (no code patch).
    void install() {
        if (installed_ || active_.load()) throw std::runtime_error("settings: bridge already active");
        signature(at(slot0_), slot0_rva,
                  {0x48, 0x8b, 0x05, 0x51, 0x67, 0x86, 0x01, 0xc3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        signature(at(event_rva_), event_rva,
                  {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x49, 0x89, 0xd6, 0x49, 0x89});
        signature(at(index_rva_), index_rva,
                  {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x50, 0x48, 0x89});
        signature(at(setter_), setter_rva,
                  {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec});
        auto** table = static_cast<void**>(at(vtable_));
        if (!accessible(table, 34 * sizeof(void*), true) || table[0] != at(slot0_) || table[25] != at(event_rva_) ||
            table[33] != at(index_rva_))
            throw std::runtime_error("settings: original selector vtable mismatch; bridge not installed");
        event_ = table[25];
        index_ = table[33];
        Adapter* expected = nullptr;
        if (!active_.compare_exchange_strong(expected, this))
            throw std::runtime_error("settings: concurrent bridge install");
        table[25] = reinterpret_cast<void*>(&ctr_settings_event_thunk);
        table[33] = reinterpret_cast<void*>(&ctr_settings_index_thunk);
        installed_ = true;
        if (!writer_started_) {
            std::thread([this] { writer(); }).detach();
            writer_started_ = true;
        }
    }
    void uninstall() {
        if (!installed_) return;
        auto** table = static_cast<void**>(at(vtable_));
        if (!accessible(table, 34 * sizeof(void*), true) ||
            table[25] != reinterpret_cast<void*>(&ctr_settings_event_thunk) ||
            table[33] != reinterpret_cast<void*>(&ctr_settings_index_thunk))
            throw std::runtime_error("settings: bridge slots changed; refusing to overwrite foreign patch");
        table[25] = event_;
        table[33] = index_;
        installed_ = false;
        active_.store(nullptr);
    }
    Word event(Word self, Word event, Word place) {
        // Fast path for the original game's selectors: the thunk sits only in the selector
        // vtable, so self is a live selector the game is calling; its option ID is read
        // directly. Only PC Settings selectors get the checked path below.
        std::uint32_t id = 0;
        std::memcpy(&id, reinterpret_cast<const unsigned char*>(self) + option_offset, sizeof(id));
        if (!menu::owns(id)) return original(event_, self, event, place);
        std::lock_guard<std::recursive_mutex> lock(callbacks_);
        auto* object = reinterpret_cast<void*>(self);
        void* vtable = nullptr;
        unsigned char player = 0;
        std::memcpy(&vtable, object, sizeof(vtable));
        std::memcpy(&player, static_cast<const unsigned char*>(object) + player_offset, 1);
        if (vtable != at(vtable_)) throw std::runtime_error("settings: unexpected selector instance/vtable");
        if (player) throw std::runtime_error("settings: PC selector must not use player-specific storage");
        const auto option = static_cast<menu::Option>(id);
        const bool loading = load_event_cached(reinterpret_cast<void*>(event));
        const Word result = original(event_, self, event, place);
        if (loading) {
            // Original loading has constructed its lists. No guest object or
            // event pointer is kept after this synchronous call.
            try {
                const unsigned selected = requested(option, requested_);
                if (!allowed(option, selected))
                    throw std::runtime_error("settings: saved mode unavailable on this renderer");
                original(at(setter_), self, selected, 0, 0);
                if (read<int>(object, index_offset) != static_cast<int>(selected))
                    throw std::runtime_error("settings: original selector rejected saved index");
            } catch (const std::exception& e) {
                error(e.what());
            }
        }
        return result;
    }
    Word changed(Word self, Word selection, Word place, Word reverse) {
        std::lock_guard<std::recursive_mutex> lock(callbacks_);
        menu::Option option{};
        auto* object = reinterpret_cast<void*>(self);
        if (!own(object, option)) return original(index_, self, selection, place, reverse);
        const int old = read<int>(object, index_offset);
        const auto selected = static_cast<std::int32_t>(selection);
        if (selected < 0 || static_cast<unsigned>(selected) >= menu::count(option)) {
            error("settings: menu index outside verified domain");
            return 0;
        }
        if (!allowed(option, static_cast<unsigned>(selected))) {
            error("settings: selected mode unavailable on this renderer");
            return 0;
        }
        const Word result = original(index_, self, selection, place, reverse);
        if (read<int>(object, index_offset) != selected || old == selected) return result;
        try {
            Json changes = Json::object();
            const Json value = menu::value(option, static_cast<unsigned>(selected));
            changes.set(menu::key(option), value);
            // Validate and expand (quality profiles) in memory, then apply and queue the save.
            validate(changes, false);
            const auto expanded = presets::expand(changes);
            auto next = requested_;
            for (const auto& item : expanded.members) next.set(item.first, item.second);
            validate(next);
            requested_ = std::move(next);
            enqueue(expanded);
            bool live = false;
            for (const auto& item : expanded.members)
                for (auto other : control_schema::options)
                    if (item.first == menu::key(other) && hooks_.publish_live &&
                        hooks_.publish_live(other, item.second))
                        live |= other == option;
            if (!live && hooks_.saved_restart_required) hooks_.saved_restart_required(menu::key(option), value);
        } catch (const std::exception& e) {
            original(at(setter_), self, static_cast<Word>(old), place, reverse);
            if (read<int>(object, index_offset) != old)
                fatal("settings: failed save and original selector rollback failed");
            error(e.what());
            return 0;
        }
        return result;
    }
    static Word dispatch_event(Word self, Word event, Word place) noexcept {
        auto* active = active_.load();
        if (!active) fatal("settings: event without active adapter");
        try {
            return active->event(self, event, place);
        } catch (const std::exception& e) {
            active->error(e.what());
            fatal("settings: invalid event callback contract");
        } catch (...) {
            fatal("settings: unexpected event callback exception");
        }
    }
    static Word dispatch_index(Word self, Word index, Word place, Word reverse) noexcept {
        auto* active = active_.load();
        if (!active) fatal("settings: index without active adapter");
        try {
            return active->changed(self, index, place, reverse);
        } catch (const std::exception& e) {
            active->error(e.what());
            fatal("settings: invalid index callback contract");
        } catch (...) {
            fatal("settings: unexpected index callback exception");
        }
    }
};
} // namespace ctrport::settings::bridge
