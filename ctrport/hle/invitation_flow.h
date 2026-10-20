// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
namespace ctr_hle::invitation {
extern std::atomic<unsigned long> dialog_thread;
struct Outcome {
    int error = 0, result = 1;
    std::vector<std::uint64_t> sent;
    std::vector<std::string> names;
    std::string session, invitation;
};
Outcome send(std::string session, std::vector<std::uint64_t> recipients, unsigned pick_maximum,
             std::atomic<unsigned long>& ui_thread);
Outcome receive(std::atomic<unsigned long>& ui_thread);
void set_owner(int(*operation)(int));
void post_accepted(const Outcome& outcome);
void tick();
}
