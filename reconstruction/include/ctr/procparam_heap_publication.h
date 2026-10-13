#pragma once
#include "ctr/native_procparam.h"
#include "ctr/native_heap_api_registry.h"
namespace ctr {
struct ProcParamHeapLease final {
 std::shared_ptr<const NativeProcParam> parameters;
 std::shared_ptr<const NativeGameHeapApi> table;
};
// Retains the genuine loaded source together with the callable heap lease.
ProcParamHeapLease retain_procparam_heap_api(std::shared_ptr<const NativeProcParam>,std::shared_ptr<const NativeGameHeapApi>);
}
