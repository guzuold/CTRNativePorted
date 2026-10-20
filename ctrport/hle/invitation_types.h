// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// ABI adapted from pinned e3ce810f3a653f43ac64ebab63023de281a4103a:
// core/libraries/invitation_dialog/invitation_dialog.h and system/commondialog.h.
#pragma once
#include <cstddef>
#include <cstdint>
namespace ctr_hle::invitation {
struct Base { std::size_t size; std::uint8_t reserved[36]; std::uint32_t magic; };
struct SessionId { char data[45]; char term; char padding[2]; };
struct Address {
    std::int32_t type, padding;
    union {
        struct { const std::uint64_t* accounts; std::uint32_t count, padding; } fixed;
        std::uint32_t maximum;
    } info;
};
union Data {
    struct { const char* message; const SessionId* session; Address address; } send;
    std::uint8_t reserved[64];
};
struct Param {
    Base base;
    std::uint32_t size;
    std::int32_t mode, user, padding;
    void* callback;
    const Data* data;
    std::uint8_t reserved[64];
};
struct User { char online_id[20]; std::uint64_t account; };
struct Users { std::uint32_t count; User users[16]; };
struct Result {
    void* callback;
    std::int32_t error, result;
    Users* sent;
    std::uint8_t reserved[32];
};
static_assert(sizeof(Param) == 144 && offsetof(Param, data) == 72);
static_assert(sizeof(User) == 32 && offsetof(Users, users) == 8);
static_assert(sizeof(Result) == 56 && offsetof(Result, sent) == 16);
} // namespace ctr_hle::invitation
