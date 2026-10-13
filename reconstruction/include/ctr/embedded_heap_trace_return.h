#pragma once
#include "ctr/native_embedded_libc_page.h"
#include "ctr/native_heap_trace.h"
#include "ctr/procparam_heap_publication.h"
#include "ctr/heap_callback_continuation.h"
namespace ctr {
class EmbeddedHeapTraceReturn final {
 std::shared_ptr<NativeEmbeddedLibcPage> page_;
 std::shared_ptr<NativeHeapTraceRegistry> trace_;
 ProcParamHeapLease lease_;
 std::filesystem::path source_;
public:
 EmbeddedHeapTraceReturn(const std::filesystem::path&,ProcParamHeapLease,std::shared_ptr<NativeHeapTraceRegistry>);
 void publish_caller_parameter(std::span<const std::byte> original_publication8);
 HeapCallbackRegistration run(const HeapCallbackRegistration&,std::span<std::byte> stack256,std::uint64_t actual_cookie);
 std::span<const std::byte> page() const {return page_->page();}
 std::shared_ptr<NativeHeapTraceRegistry> trace_owner() const {return trace_;}
};
}
