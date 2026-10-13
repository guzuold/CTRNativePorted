// SPDX-License-Identifier: GPL-2.0-only
// Narrow shader build substitution; does not provide shadPS4 core settings.
#pragma once
#include "../../shader_settings.h"
#define EmulatorSettings (::CtrVideo::GetShaderSettings())
