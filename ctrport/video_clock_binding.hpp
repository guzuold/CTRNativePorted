// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "display_event_host.hpp"
#include <iostream>
#include "video/session_api.h"
namespace ctrport {
using VideoClockBinder=int(*)(void*,const CtrDisplayEventApi*);
// Borrow callback context/code from the global runtime clock for session lifetime.
// No event queue or registration is created. Provider copies the table synchronously.
inline int bind_video_clock(const CtrVideoSessionApiV1* session,DisplayEventHost& host,VideoClockBinder binder){
 if(!session || session->size!=sizeof(*session) || session->version!=1 || !session->context || !binder)return 60;
 const auto clock=display_event_api(host);return binder(session->context,&clock);
}
inline int video_clock_binding_control(){
 DisplayEventHost host;CtrVideoSessionApiV1 session{};session.size=sizeof(session);session.version=1;session.context=&host;
 const auto binder=+[](void* context,const CtrDisplayEventApi* api)->int{
  if(context!=api->context || api->size!=sizeof(*api) || api->version!=1 || !api->clock)return 88;
  uint64_t first=0,second=0,a=0,b=0;if(api->clock(api->context,&first,&a))return 91;
  std::this_thread::sleep_for(std::chrono::milliseconds(2));if(api->clock(api->context,&second,&b) || second<=first || b<=a)return 91;return 0;
 };
 if(bind_video_clock(&session,host,binder) || bind_video_clock(nullptr,host,binder)!=60 || bind_video_clock(&session,host,nullptr)!=60)return 1;
 auto invalid=session;invalid.version=2;if(bind_video_clock(&invalid,host,binder)!=60)return 1;
 if(bind_video_clock(&session,host,+[](void*,const CtrDisplayEventApi*)->int{return 91;})!=91)return 1;
 // A real queue allocated AFTER binding still receives token1: no hidden registration.
 const auto queue=host.queues.create();if(queue!=1 || !host.queues.poll(queue,16).empty())return 1;host.queues.destroy(queue);
 std::cout<<"{\"scope\":\"authored_Video_clock_binding_transport\",\"passed\":true,\"actual_monotonic_kernel_clock\":true,\"queue_created_by_binding\":false,\"provider_failure_propagated\":true,\"GPU_presentation\":false}\n";return 0;
}
}
