// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: shadPS4's ImGui developer overlay is not part of the runtime.
#pragma once
namespace Core::Devtools {
class Layer {
public:
    static void DrawNullGpuNotice() {}
};
} // namespace Core::Devtools
