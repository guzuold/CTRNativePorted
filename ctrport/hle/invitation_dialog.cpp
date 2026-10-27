// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Lifecycle behavior: pinned shadPS4 v0.18.0 invitation_dialog/invitation_dialog.cpp.
// The real CommonDialog owner is shared with SaveDataDialog and NpProfileDialog.
// Native SEND/RECV workers share the runtime CommonDialog owner.
#include "ctr_hle.h"
#include "invitation_types.h"
#include "invitation_flow.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <future>
#include <string>
#include <vector>
namespace {
std::mutex invitation_mutex;
int (*common_operation)(int) = nullptr;
bool loaded = false;
int status = 0;
constexpr const char* LIB = "libSceInvitationDialog";
using namespace ctr_hle::invitation;
using SendResult = Outcome;
std::future<SendResult> pending;
SendResult completed;
void* callback = nullptr;
auto& confirmation_thread = dialog_thread;
constexpr int err(unsigned code) {
    return static_cast<int>(code);
}
void collect() {
    if (status == 2 && pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            completed = pending.get();
            post_accepted(completed);
        } catch (...) {
            completed = {};
            completed.error = err(0x80B8000Eu);
        }
        status = 3;
    }
}
}
CTR_HLE_API int ctr_hle_invitation_load(int (*operation)(int)) {
    std::scoped_lock lock{invitation_mutex};
    if (!ctr_hle::online_enabled() || !operation) return -1;
    common_operation = operation;
    loaded = true;
    set_owner(operation);
    ctr_hle::log_call("libSceSysmodule", "load_InvitationDialog_lifecycle", 0,
                      ",\"module_id\":162,\"fixed_recipient_send_available\":true");
    return 0;
}
// Retail v01.21 unloads module 0xa2 immediately after Terminate. The exports
// stay in our shared DLL; unloading deactivates this module's lifecycle only.
int CTR_SYSV sceSysmoduleUnloadInvitation(unsigned short id) {
    if (id != 0xa2) {
        std::fprintf(stderr, "NID=eR2bZFAAU0Q library=libSceSysmodule caller=%p module=%u unsupported unload\n",
                     __builtin_return_address(0), unsigned(id));
        ExitProcess(4);
    }
    std::scoped_lock lock{invitation_mutex};
    collect();
    int rc = !loaded ? err(0x805A1001u) : status ? err(0x805A10FFu) : 0;
    if (!rc) {
        loaded = false;
        common_operation = nullptr;
        callback = nullptr;
        completed = {};
    }
    ctr_hle::log_call("libSceSysmodule", "sceSysmoduleUnloadModule_InvitationDialog", rc);
    return rc;
}
int CTR_SYSV sceInvitationDialogInitialize() {
    std::scoped_lock lock{invitation_mutex};
    int rc = 0;
    if (!loaded || !common_operation || !common_operation(0))
        rc = static_cast<int>(0x80B80001u);
    else if (status)
        rc = static_cast<int>(0x80B80004u);
    else if (!common_operation(1))
        rc = static_cast<int>(0x80B80008u);
    else
        status = 1;
    ctr_hle::log_call(LIB, "sceInvitationDialogInitialize", rc);
    return rc;
}
int CTR_SYSV sceInvitationDialogGetStatus() {
    std::scoped_lock lock{invitation_mutex};
    collect();
    ctr_hle::log_call(LIB, "sceInvitationDialogGetStatus", status);
    return status;
}
int CTR_SYSV sceInvitationDialogUpdateStatus() {
    std::scoped_lock lock{invitation_mutex};
    collect();
    ctr_hle::log_call(LIB, "sceInvitationDialogUpdateStatus", status);
    return status;
}
int CTR_SYSV sceInvitationDialogTerminate() {
    std::scoped_lock lock{invitation_mutex};
    collect();
    const int rc = status == 2 ? err(0x80B80008u) : (status ? 0 : err(0x80B80003u));
    if (!rc) {
        status = 0;
        common_operation(2);
    }
    ctr_hle::log_call(LIB, "sceInvitationDialogTerminate", rc);
    return rc;
}
int CTR_SYSV sceInvitationDialogOpenA(const Param* param) {
    std::scoped_lock lock{invitation_mutex};
    const auto finish = [](int rc) {
        ctr_hle::log_call(LIB, "sceInvitationDialogOpenA", rc);
        return rc;
    };
    if (!param) return finish(err(0x80B8000Du));
    if (status != 1 && status != 3) return finish(err(0x80B80006u));
    if (param->size != sizeof(Param) || !param->data) return finish(err(0x80B8000Au));
    if (param->mode != 1 && param->mode != 2) return finish(err(0x80B8000Fu));
    std::string session;
    std::vector<std::uint64_t> recipients;
    unsigned maximum = 0;
    if (param->mode == 1) {
        const auto& data = param->data->send;
        if (!data.session) return finish(err(0x80B8000Au));
        const auto length = strnlen(data.session->data, 46);
        if (!length || length > 45) return finish(err(0x80B8000Au));
        session.assign(data.session->data, length);
        if (data.address.type == 1) {
            const auto& fixed = data.address.info.fixed;
            if (!fixed.accounts || !fixed.count || fixed.count > 16) return finish(err(0x80B8000Au));
            recipients.assign(fixed.accounts, fixed.accounts + fixed.count);
            auto sorted = recipients;
            std::sort(sorted.begin(), sorted.end());
            if (!sorted.front() || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
                return finish(err(0x80B8000Au));
        } else if (data.address.type == 2) {
            maximum = data.address.info.maximum;
            if (!maximum || maximum > 16) return finish(err(0x80B8000Au));
        } else
            return finish(err(0x80B8000Au));
    }
    try {
        const int mode = param->mode;
        pending = std::async(std::launch::async, [session, recipients, maximum, mode] {
            return mode == 1 ? send(session, recipients, maximum, confirmation_thread) : receive(confirmation_thread);
        });
    } catch (...) {
        return finish(err(0x80B80009u));
    }
    callback = param->callback;
    completed = {};
    status = 2;
    return finish(0);
}
int CTR_SYSV sceInvitationDialogClose() {
    std::scoped_lock lock{invitation_mutex};
    collect();
    int rc = err(0x80B8000Bu);
    if (status == 2) {
        bool posted = false;
        const DWORD thread = confirmation_thread.load();
        if (thread)
            EnumThreadWindows(
                thread,
                [](HWND window, LPARAM argument) -> BOOL {
                    wchar_t title[64]{};
                    GetWindowTextW(window, title, 64);
                    if (!std::wcscmp(title, L"CTR - Invitation"))
                        *reinterpret_cast<bool*>(argument) = PostMessageW(window, WM_COMMAND, IDCANCEL, 0) != 0;
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&posted));
        // An in-flight HTTP operation cannot be retracted; do not report it cancelled.
        rc = posted ? 0 : err(0x80B80008u);
    }
    ctr_hle::log_call(LIB, "sceInvitationDialogClose", rc);
    return rc;
}
int CTR_SYSV sceInvitationDialogGetResultA(Result* result) {
    std::scoped_lock lock{invitation_mutex};
    collect();
    int rc = 0;
    if (!status)
        rc = err(0x80B80003u);
    else if (status != 3)
        rc = err(0x80B80005u);
    else if (!result)
        rc = err(0x80B8000Du);
    else {
        result->callback = callback;
        result->error = completed.error;
        result->result = completed.result;
        if (result->sent) {
            result->sent->count = static_cast<std::uint32_t>(completed.sent.size());
            for (std::size_t i = 0; i < completed.sent.size(); ++i) {
                result->sent->users[i] = {};
                result->sent->users[i].account = completed.sent[i];
                if (i < completed.names.size())
                    std::memcpy(result->sent->users[i].online_id, completed.names[i].data(),
                                std::min<std::size_t>(16, completed.names[i].size()));
            }
        }
        rc = completed.error ? completed.error : completed.result;
    }
    ctr_hle::log_call(LIB, "sceInvitationDialogGetResultA", rc);
    return rc;
}
CTR_HLE_REGISTER_ONLINE("sAxbHhAWMXM", "libSceInvitationDialog", sceInvitationDialogOpenA);
CTR_HLE_REGISTER_ONLINE("WuuUhuKOxwQ", "libSceInvitationDialog", sceInvitationDialogGetResultA);
CTR_HLE_REGISTER_ONLINE("WWtCL5lzi7Y", "libSceInvitationDialog", sceInvitationDialogClose);
CTR_HLE_REGISTER_ONLINE("XvA5KS56wcs", "libSceInvitationDialog", sceInvitationDialogInitialize);
CTR_HLE_REGISTER_ONLINE("EiF92YDNHRA", "libSceInvitationDialog", sceInvitationDialogGetStatus);
CTR_HLE_REGISTER_ONLINE("9+g9iOq+7kg", "libSceInvitationDialog", sceInvitationDialogUpdateStatus);
CTR_HLE_REGISTER_ONLINE("B6HVJtDYxEE", "libSceInvitationDialog", sceInvitationDialogTerminate);
CTR_HLE_REGISTER_ONLINE("eR2bZFAAU0Q", "libSceSysmodule", sceSysmoduleUnloadInvitation);
