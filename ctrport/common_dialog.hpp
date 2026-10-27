// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Initialize state transition adapted from system/commondialog.cpp88-98,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a. Thread-safe runtime owner.
// Dialog launch/render/result APIs remain explicit missing imports.
#pragma once
#include <atomic>
namespace ctrport {
class CommonDialogLifecycle {
    std::atomic<bool> initialized{false};
    std::atomic<bool> used{false};

public:
    int initialize() {
        bool expected = false;
        return initialized.compare_exchange_strong(expected, true) ? 0 : static_cast<int>(0x80B80002U);
    }
    bool claim() {
        bool expected = false;
        return used.compare_exchange_strong(expected, true);
    }
    void release() { used = false; }
    bool in_use() const { return used.load(); }
    bool ready() const { return initialized.load(); }
};
inline CommonDialogLifecycle common_dialog;
}
