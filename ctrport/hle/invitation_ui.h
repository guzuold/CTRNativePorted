// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
#include <vector>
namespace ctr_hle::invitation {
// Returns selected row indices, or an empty vector on cancellation. Runs on a UI worker.
std::vector<int> choose(const std::vector<std::wstring>& rows, unsigned maximum, const wchar_t* title,
                        const wchar_t* action);
}
