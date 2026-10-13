// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include "shader_recompiler/frontend/fetch_shader.h"
namespace Shader::Gcn {
// Host-owned immutable words only; throws on incomplete/unsupported descriptors.
FetchShaderData ParseCapturedFetchShader(std::span<const u32> words);
}
