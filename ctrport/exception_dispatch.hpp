// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <Windows.h>
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "exception_dispatch_api.h"
namespace ctrport {
inline thread_local bool exception_extension_active=false;
class ExceptionDispatch {
    struct Entry {
        uint32_t code;uint64_t begin,end,token;int priority;
        CtrExceptionCallback callback;void* user;
        std::mutex mutex;bool enabled=true;
    };
    std::mutex mutex_;uint64_t serial_=0;
    std::map<uint64_t,std::shared_ptr<Entry>> entries_;
    using Snapshot=std::vector<std::shared_ptr<Entry>>;
    std::shared_ptr<const Snapshot> snapshot_=std::make_shared<const Snapshot>();
public:
    int add(uint32_t code,uint64_t begin,uint64_t end,int priority,CtrExceptionCallback callback,void* user,uint64_t* token) {
        if(exception_extension_active)return 116;
        if(!code || !begin || end<=begin || !callback || !token)return 60;
        std::lock_guard<std::mutex> lock(mutex_);
        for(const auto& item:entries_) {
            const auto& e=*item.second;
            if(e.code==code && e.begin==begin && e.end==end && e.priority==priority && e.callback==callback && e.user==user)return 109;
        }
        if(serial_==UINT64_MAX)return 117;
        auto value=std::make_shared<Entry>();value->code=code;value->begin=begin;value->end=end;
        value->token=++serial_;value->priority=priority;value->callback=callback;value->user=user;
        auto next=std::make_shared<Snapshot>();
        for(const auto& item:entries_)next->push_back(item.second);next->push_back(value);
        std::sort(next->begin(),next->end(),[](const auto& a,const auto& b){return a->priority==b->priority?a->token<b->token:a->priority<b->priority;});
        entries_.emplace(value->token,value);
        std::atomic_store(&snapshot_,std::shared_ptr<const Snapshot>(std::move(next)));
        *token=value->token;return 0;
    }
    int remove(uint64_t token) {
        if(exception_extension_active)return 116;
        std::shared_ptr<Entry> entry;
        {std::lock_guard<std::mutex> lock(mutex_);auto found=entries_.find(token);if(found==entries_.end())return 62;entry=found->second;
            auto next=std::make_shared<Snapshot>();for(const auto& item:entries_)if(item.first!=token)next->push_back(item.second);
            std::sort(next->begin(),next->end(),[](const auto& a,const auto& b){return a->priority==b->priority?a->token<b->token:a->priority<b->priority;});
            entries_.erase(found);std::atomic_store(&snapshot_,std::shared_ptr<const Snapshot>(std::move(next)));
        }
        // Dispatcher holds this lock throughout callback; removal joins it.
        std::lock_guard<std::mutex> lock(entry->mutex);entry->enabled=false;return 0;
    }
    LONG dispatch(EXCEPTION_POINTERS* exception) noexcept {
        if(exception_extension_active || !exception || !exception->ExceptionRecord || !exception->ContextRecord)return EXCEPTION_CONTINUE_SEARCH;
        exception_extension_active=true;
        struct Reset {~Reset(){exception_extension_active=false;}} reset;
        try {
            const auto rip=exception->ContextRecord->Rip;const auto code=exception->ExceptionRecord->ExceptionCode;
            // No allocation or registration mutex in native exception delivery.
            const auto selected=std::atomic_load(&snapshot_);
            for(const auto& entry:*selected) {
                if(entry->code!=code || rip<entry->begin || rip>=entry->end)continue;
                std::lock_guard<std::mutex> lock(entry->mutex);
                if(entry->enabled && entry->callback(entry->user,exception)==1)return EXCEPTION_CONTINUE_EXECUTION;
            }
        }catch(...) {
            // Host callback failure remains an unhandled fault; no repaired data.
            OutputDebugStringA("CTR exception extension failed; continuing exception search\n");
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
};
inline ExceptionDispatch runtime_exception_dispatch;
inline int exception_api_add(void* context,uint32_t code,uint64_t begin,uint64_t end,int priority,CtrExceptionCallback callback,void* user,uint64_t* token) noexcept {
    if(!context)return 60;try{return static_cast<ExceptionDispatch*>(context)->add(code,begin,end,priority,callback,user,token);}catch(...){return 90;}
}
inline int exception_api_remove(void* context,uint64_t token) noexcept {
    if(!context)return 60;try{return static_cast<ExceptionDispatch*>(context)->remove(token);}catch(...){return 90;}
}
inline CtrExceptionDispatchApi make_exception_dispatch_api(){return {sizeof(CtrExceptionDispatchApi),1,&runtime_exception_dispatch,&exception_api_add,&exception_api_remove};}
}
