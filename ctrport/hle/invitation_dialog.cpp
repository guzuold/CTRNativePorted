// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Lifecycle behavior: pinned shadPS4 v0.18.0 invitation_dialog/invitation_dialog.cpp.
// The real CommonDialog owner is shared with SaveDataDialog and NpProfileDialog.
// Invitation UI, sending and receiving remain missing until implemented.
#include "ctr_hle.h"
namespace {
std::mutex invitation_mutex;
int (*common_operation)(int) = nullptr;
bool loaded = false;
int status = 0;
constexpr const char* LIB = "libSceInvitationDialog";
}
CTR_HLE_API int ctr_hle_invitation_load(int (*operation)(int)) {
    std::scoped_lock lock{invitation_mutex};
    if (!ctr_hle::online_enabled() || !operation) return -1;
    common_operation = operation; loaded = true;
    ctr_hle::log_call("libSceSysmodule", "load_InvitationDialog_lifecycle", 0,
                      ",\"module_id\":162,\"invitation_ui_available\":false");
    return 0;
}
int CTR_SYSV sceInvitationDialogInitialize() {
    std::scoped_lock lock{invitation_mutex};
    int rc = 0;
    if (!loaded || !common_operation || !common_operation(0)) rc = static_cast<int>(0x80B80001u);
    else if (status) rc = static_cast<int>(0x80B80004u);
    else if (!common_operation(1)) rc = static_cast<int>(0x80B80008u);
    else status = 1;
    ctr_hle::log_call(LIB, "sceInvitationDialogInitialize", rc); return rc;
}
int CTR_SYSV sceInvitationDialogGetStatus() {
    std::scoped_lock lock{invitation_mutex};
    ctr_hle::log_call(LIB, "sceInvitationDialogGetStatus", status); return status;
}
int CTR_SYSV sceInvitationDialogUpdateStatus() {
    std::scoped_lock lock{invitation_mutex};
    ctr_hle::log_call(LIB, "sceInvitationDialogUpdateStatus", status); return status;
}
int CTR_SYSV sceInvitationDialogTerminate() {
    std::scoped_lock lock{invitation_mutex};
    const int rc = status ? 0 : static_cast<int>(0x80B80003u);
    if (!rc) { status = 0; common_operation(2); }
    ctr_hle::log_call(LIB, "sceInvitationDialogTerminate", rc); return rc;
}
CTR_HLE_REGISTER_ONLINE("XvA5KS56wcs", "libSceInvitationDialog", sceInvitationDialogInitialize);
CTR_HLE_REGISTER_ONLINE("EiF92YDNHRA", "libSceInvitationDialog", sceInvitationDialogGetStatus);
CTR_HLE_REGISTER_ONLINE("9+g9iOq+7kg", "libSceInvitationDialog", sceInvitationDialogUpdateStatus);
CTR_HLE_REGISTER_ONLINE("B6HVJtDYxEE", "libSceInvitationDialog", sceInvitationDialogTerminate);
