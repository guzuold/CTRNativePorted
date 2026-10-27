// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace CtrPm4 {
// Whole public LiverpoolToVK PrimitiveType/PolygonMode/LogicOp mappings, pinned
// e3ce810f3a653f43ac64ebab63023de281a4103a. Unknown enums explicit88 atomically.
inline int GraphicsTopology(uint32_t value, VkPrimitiveTopology& output) {
    VkPrimitiveTopology topology;
    switch (value) {
    case 1: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
    case 2: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
    case 3: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
    case 4: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    case 5:
    case 21: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
    case 6: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
    case 10: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY; break;
    case 11: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY; break;
    case 12: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY; break;
    case 13: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY; break;
    case 9:
    case 17:
    case 19: topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
    default: return 88;
    }
    output = topology;
    return 0;
}
inline int GraphicsPolygonMode(uint32_t value, VkPolygonMode& output) {
    static constexpr VkPolygonMode modes[] = {VK_POLYGON_MODE_POINT, VK_POLYGON_MODE_LINE, VK_POLYGON_MODE_FILL};
    if (value > 2) return 88;
    output = modes[value];
    return 0;
}
inline int GraphicsLogicOp(uint32_t value, VkLogicOp& output) {
    static constexpr VkLogicOp operations[] = {
        VK_LOGIC_OP_CLEAR,       VK_LOGIC_OP_NOR,        VK_LOGIC_OP_AND_INVERTED, VK_LOGIC_OP_COPY_INVERTED,
        VK_LOGIC_OP_AND_REVERSE, VK_LOGIC_OP_INVERT,     VK_LOGIC_OP_XOR,          VK_LOGIC_OP_NAND,
        VK_LOGIC_OP_AND,         VK_LOGIC_OP_EQUIVALENT, VK_LOGIC_OP_NO_OP,        VK_LOGIC_OP_OR_INVERTED,
        VK_LOGIC_OP_COPY,        VK_LOGIC_OP_OR_REVERSE, VK_LOGIC_OP_OR,           VK_LOGIC_OP_SET};
    if (value > 255 || (value & 15) != (value >> 4)) return 88;
    output = operations[value >> 4];
    return 0;
}
}
