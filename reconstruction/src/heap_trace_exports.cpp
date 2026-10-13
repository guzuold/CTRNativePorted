#include "procparam_heap_exports.cpp"
#include "ctr/native_heap_trace.h"
namespace {struct TraceOwner {std::shared_ptr<ctr::NativeHeapTraceRegistry> registry;};}
extern "C" {
__declspec(dllexport) TraceOwner* ctr_trace_create()noexcept{try{return new TraceOwner{std::make_shared<ctr::NativeHeapTraceRegistry>()};}catch(...){return nullptr;}}
__declspec(dllexport) TraceOwner* ctr_trace_retain(TraceOwner* s)noexcept{try{return s?new TraceOwner{s->registry}:nullptr;}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_trace_release(TraceOwner* s)noexcept{delete s;}
__declspec(dllexport) void* ctr_trace_page(TraceOwner* s)noexcept{return s?s->registry->page():nullptr;}
__declspec(dllexport) int ctr_trace_query(TraceOwner* s,void* info,std::size_t length)noexcept{try{if(!s||!info)throw std::invalid_argument("Trace output required");s->registry->query({static_cast<std::byte*>(info),length});return 0;}catch(...){return 22;}}
__declspec(dllexport) int ctr_trace_register(TraceOwner* s,ComposedSession* context,unsigned* index)noexcept{try{if(!s||!context||!index)throw std::invalid_argument("Trace backend/output required");const auto result=s->registry->insert(context->heap->backend);*index=result;return 0;}catch(...){return 22;}}
__declspec(dllexport) int ctr_trace_remove(TraceOwner* s,unsigned index)noexcept{try{if(!s)throw std::invalid_argument("Trace owner required");s->registry->erase(index);return 0;}catch(...){return 22;}}
__declspec(dllexport) int ctr_trace_snapshot(TraceOwner* s,void* output,std::size_t length)noexcept{try{if(!s||!output||length!=4096)throw std::invalid_argument("Trace snapshot extent required");const auto bytes=s->registry->snapshot();std::memcpy(output,bytes.data(),4096);return 0;}catch(...){return 22;}}
}
