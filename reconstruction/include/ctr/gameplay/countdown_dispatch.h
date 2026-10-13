// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace ctr {
// Whole retail00ed7ab0: counter at owner+40; services borrow owner+18.
struct CountdownDispatchServices {
 void* (*query)(void* context){}; // genuine owner virtual+110
 void (*dispatch)(void* object,void* context){}; // genuine00d28310 with retail fixed token
 void* context{};
};
void decrement_dispatch_countdown(std::uint32_t& counter,const CountdownDispatchServices& services);
}
