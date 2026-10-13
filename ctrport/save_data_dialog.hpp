// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Lifecycle adapted from save_data/dialog/savedatadialog.cpp51-65,123-140
// e3ce810f3a653f43ac64ebab63023de281a4103a. UI/open/result unsupported.
#pragma once
#include "common_dialog.hpp"
#include <mutex>
namespace ctrport {
class SaveDataDialogLifecycle {
    CommonDialogLifecycle& common;
    std::mutex lock;
    int status=0;
public:
    explicit SaveDataDialogLifecycle(CommonDialogLifecycle& owner):common(owner) {}
    int initialize() {
        std::lock_guard<std::mutex> guard(lock);
        if(!common.ready()) return static_cast<int>(0x80B80001U);
        if(status!=0) return static_cast<int>(0x80B80004U);
        if(!common.claim()) return static_cast<int>(0x80B80008U);
        status=1;return 0;
    }
    int update_status() {std::lock_guard<std::mutex> guard(lock);return status;}
    int terminate() {
        std::lock_guard<std::mutex> guard(lock);
        if(!status) return static_cast<int>(0x80B80003U);
        status=0;common.release();return 0;
    }
};
inline SaveDataDialogLifecycle save_data_dialog(common_dialog);
}
