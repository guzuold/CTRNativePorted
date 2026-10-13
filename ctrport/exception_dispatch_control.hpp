// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "exception_dispatch.hpp"
#include "flat_loader.hpp"
#include <future>
#include <thread>
namespace ctrport {
struct ExceptionControlCall {std::vector<int>* order;int id;bool handle;bool* recursion;};
inline int exception_control_callback(void* raw,void* pointers){
    auto& value=*static_cast<ExceptionControlCall*>(raw);auto* exception=static_cast<EXCEPTION_POINTERS*>(pointers);
    value.order->push_back(value.id);
    if(value.handle) {
        *value.recursion=runtime_exception_dispatch.dispatch(exception)==EXCEPTION_CONTINUE_SEARCH && runtime_exception_dispatch.remove(1)==116;
        exception->ContextRecord->Rip+=2;return 1; // Exact synthetic UD2 length.
    }
    return 0;
}
inline LONG CALLBACK exception_control_delivery(EXCEPTION_POINTERS* exception){return runtime_exception_dispatch.dispatch(exception);}
struct ExceptionBlockingControl {std::promise<void> entered;std::shared_future<void> release;};
inline int exception_blocking_callback(void* raw,void*){
    auto& value=*static_cast<ExceptionBlockingControl*>(raw);value.entered.set_value();value.release.wait();return 0;
}
inline int exception_throwing_callback(void*,void*){throw std::runtime_error("Controlled host callback failure");}
inline int exception_dispatch_control(){
    const auto api=make_exception_dispatch_api();
    MappedImage code(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
    if(!code)return 1;code.get()[0]=0x0f;code.get()[1]=0x0b;code.get()[2]=0xc3;
    DWORD old=0;if(!VirtualProtect(code.get(),4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),code.get(),3))return 1;
    const auto begin=reinterpret_cast<uint64_t>(code.get());
    struct NativeDelivery {void* handle=nullptr;~NativeDelivery(){if(handle)RemoveVectoredExceptionHandler(handle);}} delivery;
    delivery.handle=AddVectoredExceptionHandler(1,&exception_control_delivery);if(!delivery.handle)return 1;
    std::vector<int> order;bool recursion=false;
    ExceptionControlCall second{&order,2,false,&recursion},first{&order,1,false,&recursion},third{&order,3,true,&recursion};
    uint64_t a=0,b=0,c=0,unchanged=0x1234;
    if(api.add(api.context,EXCEPTION_ILLEGAL_INSTRUCTION,begin,begin+2,0,&exception_control_callback,&second,&a) ||
       api.add(api.context,EXCEPTION_ILLEGAL_INSTRUCTION,begin,begin+2,-1,&exception_control_callback,&first,&b) ||
       api.add(api.context,EXCEPTION_ILLEGAL_INSTRUCTION,begin,begin+2,0,&exception_control_callback,&third,&c))return 1;
    if(api.add(api.context,EXCEPTION_ILLEGAL_INSTRUCTION,begin,begin+2,0,&exception_control_callback,&second,&unchanged)!=109 || unchanged!=0x1234)return 1;
    reinterpret_cast<void(*)()>(code.get())();
    if(order!=std::vector<int>({1,2,3}) || !recursion)return 1;
    EXCEPTION_RECORD record{};CONTEXT context{};EXCEPTION_POINTERS exception{&record,&context};
    record.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;context.Rip=begin+2;
    if(runtime_exception_dispatch.dispatch(&exception)!=EXCEPTION_CONTINUE_SEARCH)return 1;
    context.Rip=begin;record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    if(runtime_exception_dispatch.dispatch(&exception)!=EXCEPTION_CONTINUE_SEARCH)return 1;
    if(api.remove(api.context,a) || api.remove(api.context,b) || api.remove(api.context,c) || api.remove(api.context,a)!=62)return 1;
    record.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;
    if(runtime_exception_dispatch.dispatch(&exception)!=EXCEPTION_CONTINUE_SEARCH)return 1;
    std::promise<void> release;ExceptionBlockingControl block;block.release=release.get_future().share();auto entered=block.entered.get_future();uint64_t token=0;
    if(api.add(api.context,record.ExceptionCode,begin,begin+2,0,&exception_blocking_callback,&block,&token))return 1;
    auto pending=std::async(std::launch::async,[&]{return runtime_exception_dispatch.dispatch(&exception);});entered.wait();
    auto removal=std::async(std::launch::async,[&]{return api.remove(api.context,token);});
    const bool waited=removal.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout;
    release.set_value();if(pending.get()!=EXCEPTION_CONTINUE_SEARCH || removal.get()!=0 || !waited)return 1;
    if(runtime_exception_dispatch.dispatch(&exception)!=EXCEPTION_CONTINUE_SEARCH)return 1;
    if(api.add(api.context,record.ExceptionCode,begin,begin+2,0,&exception_throwing_callback,nullptr,&token))return 1;
    if(runtime_exception_dispatch.dispatch(&exception)!=EXCEPTION_CONTINUE_SEARCH || api.remove(api.context,token))return 1;
    if(!RemoveVectoredExceptionHandler(delivery.handle))return 1;delivery.handle=nullptr;
    std::cout<<"{\"scope\":\"runtime_scoped_exception_owner_control\",\"real_UD2_resumed\":true,\"equal_priority_order_preserved\":true,\"duplicate_rejected\":true,\"reentrant_search\":true,\"callback_removal_rejected\":true,\"unknown_and_expired_search\":true,\"remove_waited_active_callback\":true,\"callback_exception_search\":true,\"native_handler_removed\":true,\"retail_shader\":false,\"zero_on_fault\":false}\n";return 0;
}
}
