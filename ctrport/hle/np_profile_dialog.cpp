// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Lifecycle adapted from pinned shadPS4 v0.18.0
// src/core/libraries/np/np_profile_dialog/np_profile_dialog.cpp.
// CommonDialog ownership stays in ctrport.exe, shared with SaveDataDialog.
// Open/Close/GetResult are deliberately unbound until a local UI is implemented.
#include "ctr_hle.h"

namespace {
std::mutex profile_mutex;
int (*common_operation)(int) = nullptr; // Win64: 0 ready, 1 claim, 2 release
bool loaded = false;
int status = 0;
constexpr const char* LIB = "libSceNpProfileDialog";
constexpr int error(unsigned value) { return static_cast<int>(value); }
}

CTR_HLE_API int ctr_hle_profile_load(int (*operation)(int)) {
    std::scoped_lock lock{profile_mutex};
    if (!ctr_hle::online_enabled() || !operation) return -1;
    common_operation = operation;
    loaded = true;
    ctr_hle::log_call("libSceSysmodule", "load_NpProfileDialog_lifecycle", 0,
                      ",\"module_id\":178,\"profile_ui_available\":false");
    return 0;
}

int CTR_SYSV sceNpProfileDialogInitialize() {
    std::scoped_lock lock{profile_mutex};
    int rc = 0;
    if (!loaded || !common_operation || !common_operation(0)) rc = error(0x80B80001u);
    else if (status) rc = error(0x80B80004u);
    else if (!common_operation(1)) rc = error(0x80B80008u);
    else status = 1;
    ctr_hle::log_call(LIB, "sceNpProfileDialogInitialize", rc);
    return rc;
}
int CTR_SYSV sceNpProfileDialogGetStatus() {
    std::scoped_lock lock{profile_mutex};
    ctr_hle::log_call(LIB, "sceNpProfileDialogGetStatus", status);
    return status;
}
int CTR_SYSV sceNpProfileDialogUpdateStatus() {
    std::scoped_lock lock{profile_mutex};
    ctr_hle::log_call(LIB, "sceNpProfileDialogUpdateStatus", status);
    return status;
}
int CTR_SYSV sceNpProfileDialogTerminate() {
    std::scoped_lock lock{profile_mutex};
    const int rc = status ? 0 : error(0x80B80003u);
    if (!rc) { status = 0; common_operation(2); }
    ctr_hle::log_call(LIB, "sceNpProfileDialogTerminate", rc);
    return rc;
}
CTR_HLE_REGISTER_ONLINE("Lg+NCE6pTwQ", "libSceNpProfileDialog", sceNpProfileDialogInitialize);
CTR_HLE_REGISTER_ONLINE("3BqoiFOjSsk", "libSceNpProfileDialog", sceNpProfileDialogGetStatus);
CTR_HLE_REGISTER_ONLINE("haVZE9FgKqE", "libSceNpProfileDialog", sceNpProfileDialogUpdateStatus);
CTR_HLE_REGISTER_ONLINE("0Sp9vJcB1-w", "libSceNpProfileDialog", sceNpProfileDialogTerminate);
