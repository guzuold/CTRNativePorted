// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <filesystem>
namespace CtrVideo {
// Returns a validated path only. Does not create directories or files.
std::filesystem::path ResolveShaderOutput(const std::filesystem::path& project_root,
                                         const std::filesystem::path& relative);
}
