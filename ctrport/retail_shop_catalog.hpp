// SPDX-License-Identifier: GPL-2.0-only
// User-owned retail data only. EU v01.21 offsets, found in other retail eboots by retail_map.hpp
// (reward_catalog::start).
// Constant-price single products and bundle membership read from retail memory.
// Redirect handles and other price implementations remain unsupported.
#pragma once
namespace ctrport::reward_catalog {
inline bool shop_object(std::uintptr_t handle, std::uintptr_t& object) {
    std::uint32_t flags = 0;
    return handle && read(handle, flags) && !(flags & 0x30000000) && read(handle + 0x28, object) && object;
}
inline bool shop_snapshot(std::uintptr_t base, std::string& result) {
    std::uintptr_t market = 0, data = 0, list = 0, array = 0;
    std::uint32_t count = 0;
    if (!read(base + retail_map::at(0x2ac8348), market) || !market || !read(market + 0x18, data) || !data ||
        !read(data + 0x70, list) || !list || !read(list + 0xc, count) || !count || count > 4096 ||
        !read(list + 0x20, array) || !array)
        return false;
    std::ostringstream out;
    out << "{\"schema\":1,\"eboot_sha256\":\"" << supported_sha << "\",\"skus\":[";
    std::set<std::uint32_t> ids;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uintptr_t sku = 0, vtable = 0, accessor = 0, handle = 0, product = 0, currency = 0;
        std::uintptr_t items = 0, item_array = 0, entry = 0, item = 0;
        std::uint32_t id = 0, price = 0, currency_id = 0, item_count = 0, item_id = 0, quantity = 0;
        if (!read(array + i * 8, sku) || !sku || !read(sku, vtable) || !read(vtable + 28 * 8, accessor)) continue;
        if (accessor == base + retail_map::at(0xab21a0)) {
            // Original accessor walks +0x90 and sums unowned component prices.
            std::uintptr_t members = 0, member_array = 0;
            std::uint32_t member_count = 0;
            if (!read(sku + 0xc, id) || !id || !read(sku + 0x90, members) || !members ||
                !read(members + 0xc, member_count) || !member_count || member_count > 31 ||
                !read(members + 0x20, member_array) || !member_array)
                continue;
            std::set<std::uint32_t> components;
            bool valid = true;
            for (std::uint32_t j = 0; j < member_count; ++j) {
                std::uintptr_t component = 0;
                std::uint32_t component_id = 0;
                if (!read(member_array + j * 8, handle) || !shop_object(handle, component) ||
                    !read(component + 0xc, component_id) || !component_id || component_id == id ||
                    !components.insert(component_id).second) {
                    valid = false;
                    break;
                }
            }
            if (!valid) continue;
            if (!ids.insert(id).second) return false;
            if (ids.size() > 1) out << ',';
            out << "{\"id\":" << id << ",\"components\":[";
            bool first = true;
            for (auto component : components) {
                if (!first) out << ',';
                first = false;
                out << component;
            }
            out << "]}";
            continue;
        }
        if (accessor != base + retail_map::at(0xab1620) || !read(sku + 0xc, id) || !id || !read(sku + 0x28, price) ||
            !price || price > 1000000 || !read(sku + 0x18, handle) || !shop_object(handle, product) ||
            !read(sku + 0x20, handle) || !shop_object(handle, currency) || !read(currency + 0xc, currency_id) ||
            currency_id != 10 || !read(product + 0x20, items) || !items || !read(items + 0xc, item_count) ||
            item_count != 1 || !read(items + 0x20, item_array) || !item_array || !read(item_array, entry) || !entry ||
            !read(entry + 0x10, handle) || !shop_object(handle, item) || !read(item + 0xc, item_id) || !item_id ||
            !read(entry + 0x18, quantity) || quantity != 1)
            continue;
        if (!ids.insert(id).second) return false;
        if (ids.size() > 1) out << ',';
        out << "{\"id\":" << id << ",\"currency\":" << currency_id << ",\"price\":" << price << ",\"item\":" << item_id
            << ",\"quantity\":1}";
    }
    if (ids.empty()) return false;
    out << "]}\n";
    result = out.str();
    return true;
}
}
