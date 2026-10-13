// Compile the same verified heap/memory/mutex publication implementation into
// a separate DLL. Existing provider DLLs and their ABI remain unchanged.
#include "heap_publication_exports.cpp"
#include "ctr/native_procparam.h"
#include "ctr/procparam_heap_publication.h"
namespace {
struct ComposedSession {
 std::shared_ptr<const ctr::NativeProcParam> parameters;
 std::unique_ptr<Session> heap=std::make_unique<Session>();
};
struct ComposedLease {
 std::shared_ptr<const ctr::NativeProcParam> parameters;
 std::unique_ptr<Lease> heap;
};
}
extern "C" {
__declspec(dllexport) ComposedSession* ctr_composed_session_create(const wchar_t* self,const wchar_t* flat) noexcept {try{if(!self||!flat)return nullptr;auto context=std::make_unique<ComposedSession>();context->parameters=ctr::NativeProcParam::load(self,flat);return context.release();}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_composed_session_destroy(ComposedSession* s) noexcept {delete s;}
__declspec(dllexport) Session* ctr_composed_session_heap(ComposedSession* s) noexcept {return s?s->heap.get():nullptr;}
__declspec(dllexport) std::uint64_t ctr_composed_procparam_query(ComposedSession* s) noexcept {return s?s->parameters->query_logical_root():0;}
__declspec(dllexport) const void* ctr_composed_procparam_page(ComposedSession* s,std::uint64_t address) noexcept {try{return s?s->parameters->page(address).data():nullptr;}catch(...){return nullptr;}}
__declspec(dllexport) int ctr_composed_publish_original_table(ComposedSession* s,const void* slots,std::uint64_t argument) noexcept {try{if(!s||!slots)return 22;const auto root=s->parameters->section(0x02600000),libc=s->parameters->section(0x02200000);std::array<std::byte,512> graph{};std::memcpy(graph.data(),root.data(),88);std::memcpy(graph.data()+88,libc.data(),144);std::memcpy(graph.data()+232,libc.data()+0xd8,112);std::memcpy(graph.data()+344,libc.data()+0x148,112);std::memcpy(graph.data()+456,libc.data()+0x1b8,56);return ctr_heap_publish_original_table(s->heap.get(),graph.data(),slots,argument);}catch(...){return 22;}}
__declspec(dllexport) ComposedLease* ctr_composed_acquire(ComposedSession* s) noexcept {try{if(!s)return nullptr;const auto joined=ctr::retain_procparam_heap_api(s->parameters,s->heap->registry.acquire());auto lease=std::make_unique<ComposedLease>();lease->parameters=joined.parameters;lease->heap=std::make_unique<Lease>(Lease{joined.table,s->heap->operation});return lease.release();}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_composed_lease_destroy(ComposedLease* p) noexcept {delete p;}
__declspec(dllexport) const void* ctr_composed_lease_page(ComposedLease* p,std::uint64_t address) noexcept {try{return p?p->parameters->page(address).data():nullptr;}catch(...){return nullptr;}}
__declspec(dllexport) const void* ctr_composed_lease_table(ComposedLease* p) noexcept {return p?p->heap->api.get():nullptr;}
__declspec(dllexport) int ctr_composed_lease_malloc(ComposedLease* p,std::uint64_t size,std::uint64_t* out) noexcept {return p?ctr_heap_lease_malloc(p->heap.get(),size,out):22;}
__declspec(dllexport) int ctr_composed_lease_free(ComposedLease* p,std::uint64_t address) noexcept {return p?ctr_heap_lease_free(p->heap.get(),address):22;}
}
