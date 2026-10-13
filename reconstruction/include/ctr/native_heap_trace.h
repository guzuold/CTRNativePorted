#pragma once
#include "ctr/system_pool_backend.h"
#include <array>
#include <mutex>
namespace ctr {
struct NativeHeapTraceInfo {std::uint64_t size;std::uint32_t flag,get_segment_info;std::uint64_t* mask;std::uint64_t* table;};
static_assert(sizeof(NativeHeapTraceInfo)==32);
static_assert(offsetof(NativeHeapTraceInfo,flag)==8&&offsetof(NativeHeapTraceInfo,get_segment_info)==12&&offsetof(NativeHeapTraceInfo,mask)==16&&offsetof(NativeHeapTraceInfo,table)==24);
class NativeHeapTraceRegistry final {
public:
 NativeHeapTraceRegistry();~NativeHeapTraceRegistry();
 NativeHeapTraceRegistry(const NativeHeapTraceRegistry&)=delete;
 void query(std::span<std::byte> info32)const;
 unsigned insert(std::shared_ptr<SystemPoolBackend>);
 void erase(unsigned);
 std::array<std::byte,4096> snapshot()const;
 void* page()const noexcept{return page_;}
private:
 void* page_{};mutable std::mutex lock_;
 std::array<std::shared_ptr<SystemPoolBackend>,64> owners_{};
};
}
