# Provenance of ctrport/gpu

- Compiled in place, not copied: shadPS4 v0.18.0 (`engine/shadPS4`, GPL-2.0-or-later):
  `src/video_core`, `src/shader_recompiler`, part of `src/common`,
  `src/core/libraries/gnmdriver/gnmdriver.cpp`, `src/core/libraries/videoout/{driver,video_out}.cpp`,
  `src/core/libraries/kernel/equeue.cpp`, and the externals they need (boost headers, fmt, SDL3,
  Vulkan headers, VMA, glslang, robin-map, magic_enum, xxhash, sirit, half, miniz, Zydis, xbyak).
- Derived at build time: `vk_presenter.cpp` by `make_patched.py` (exact replacements, listed there).
- Copied unchanged from bbport for Windows, revision `4fcdf770e078911cc7f36e409d316ec423b7c81e`
  (`reports/reference_material/bloodborne_windows_runtime/gpu/shim`, GPL-2.0):
  `shim/core/address_space.h`, `shim/core/signals.h`, `shim/core/libraries/libs.h`,
  `shim/core/devtools/widget/gpu_memory.h`, `shim/common/debug.h`, `shim/common/logging/log.h`,
  `shim/tracy/Tracy.hpp`, `shim/tracy/TracyVulkan.hpp`, `shim/imgui/renderer/texture_manager.h`.
- Adapted from the same bbport files (marked in their headers): `shim/core/memory.h`,
  `shim/core/emulator_settings.h`, `shim/core/debug_state.h`, `shim/core/libraries/kernel/kernel.h`,
  `shim/sdl_window.h`, `shim/window.cpp`, `shim/common/logging/log.cpp`, `shim/common/assert.cpp`,
  `shim/imgui/renderer/imgui_core.h`, `shim/ctr_gpu.cpp` (structure of `bbgpu.cpp`).
- Written for ctrport: `ctr_gpu.h`, `shim/core/file_sys/fs.h`, `shim/core/devtools/layer.h`,
  `shim/imgui/imgui_texture.h`, `CMakeLists.txt`, `build.ps1`, `make_patched.py`.

Build: `python -B tools/lane_lock.py --name build-claude --lane claude -- powershell -File ctrport/gpu/build.ps1`
(clang-cl from `tools/toolchains/llvm-19.1.1`, CMake and Ninja from Visual Studio 2022).
