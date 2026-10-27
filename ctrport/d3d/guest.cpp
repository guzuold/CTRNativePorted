// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest.h"

#include <windows.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace {
std::shared_mutex g_mutex;
// [begin, end) by begin: reported mappings and regions learned from VirtualQuery
std::map<u64, u64> g_ranges;
// content hashes of ranges without write watch, by (addr, size)
std::mutex g_hash_mutex;
std::unordered_map<u64, u64> g_hashes;

bool Contains(u64 addr, u64 end) {
    auto it = g_ranges.upper_bound(addr);
    if (it == g_ranges.begin()) return false;
    --it;
    return addr >= it->first && end <= it->second;
}

void Insert(u64 begin, u64 end) {
    // merge with overlapping or adjacent ranges
    auto it = g_ranges.upper_bound(begin);
    if (it != g_ranges.begin()) {
        auto prev = std::prev(it);
        if (prev->second >= begin) {
            begin = prev->first;
            end = std::max(end, prev->second);
            it = g_ranges.erase(prev);
        }
    }
    while (it != g_ranges.end() && it->first <= end) {
        end = std::max(end, it->second);
        it = g_ranges.erase(it);
    }
    g_ranges.emplace(begin, end);
}

// Learns the committed readable regions covering [addr, end) with VirtualQuery.
bool Learn(u64 addr, u64 end) {
    u64 cursor = addr;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)cursor, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !mbi.Protect) return false;
        const u64 rb = (u64)mbi.BaseAddress, re = rb + mbi.RegionSize;
        std::unique_lock lk(g_mutex);
        Insert(rb, re);
        cursor = re;
    }
    return true;
}

u64 Fnv(const u8* p, u64 n) {
    u64 h = 0xCBF29CE484222325ull;
    // 8 bytes at a time: CPU-written texture data (video frames) is megabytes per frame
    u64 i = 0;
    for (; i + 8 <= n; i += 8) {
        u64 v;
        memcpy(&v, p + i, 8);
        h = (h ^ v) * 0x100000001B3ull;
    }
    for (; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}
u64 Key(u64 addr, u64 size) {
    return addr * 0x9E3779B97F4A7C15ull ^ size;
}
} // namespace

void GuestAddRange(u64 addr, u64 size) {
    if (!size) return;
    std::unique_lock lk(g_mutex);
    Insert(addr, addr + size);
}

void GuestRemoveRange(u64 addr, u64 size) {
    std::unique_lock lk(g_mutex);
    const u64 end = addr + size;
    auto it = g_ranges.upper_bound(addr);
    if (it != g_ranges.begin()) --it;
    while (it != g_ranges.end() && it->first < end) {
        const u64 b = it->first, e = it->second;
        if (e <= addr) {
            ++it;
            continue;
        }
        it = g_ranges.erase(it);
        if (b < addr) g_ranges.emplace(b, addr);
        if (e > end) g_ranges.emplace(end, e);
    }
}

bool GuestReadable(u64 addr, u64 size) {
    if (addr < 0x10000 || addr + size < addr) return false;
    const u64 end = addr + std::max<u64>(size, 1);
    {
        std::shared_lock lk(g_mutex);
        if (Contains(addr, end)) return true;
    }
    return Learn(addr, end);
}

bool GuestWrittenSince(u64 addr, u64 size, bool reset) {
    void* page;
    ULONG_PTR count = 1;
    ULONG gran;
    if (GetWriteWatch(0, (void*)addr, size, &page, &count, &gran) == 0) {
        if (count && reset) ResetWriteWatch((void*)addr, size);
        return count != 0;
    }
    // no write watch on this memory: compare the content
    if (!GuestReadable(addr, size)) return false;
    const u64 h = Fnv((const u8*)addr, size);
    std::lock_guard lk(g_hash_mutex);
    auto [it, fresh] = g_hashes.try_emplace(Key(addr, size), h);
    const bool changed = !fresh && it->second != h;
    if (reset || fresh) it->second = h;
    return changed;
}

void GuestResetWritten(u64 addr, u64 size) {
    if (ResetWriteWatch((void*)addr, size) == 0) return;
    if (!GuestReadable(addr, size)) return;
    const u64 h = Fnv((const u8*)addr, size);
    std::lock_guard lk(g_hash_mutex);
    g_hashes[Key(addr, size)] = h;
}
