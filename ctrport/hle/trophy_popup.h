// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <string>
#include <vector>
namespace ctr_hle::trophy {
// Queues a notice in the top right corner of the game window for five seconds: the trophy
// icon (PNG), a title and the trophy name. Notices are shown one after another.
void show_popup(std::vector<unsigned char> png, std::wstring title, std::wstring name);
}
