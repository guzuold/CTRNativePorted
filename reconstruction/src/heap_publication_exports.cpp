#include "ctr/game_heap_api_publication.h"
#include <cstring>
#include <mutex>
#include <stdexcept>
namespace {
struct Session {
 std::shared_ptr<ctr::SystemPoolBackend> backend=std::make_shared<ctr::SystemPoolBackend>();
 ctr::NativeHeapApiRegistry registry;
 std::shared_ptr<std::mutex> operation=std::make_shared<std::mutex>();
 bool poison{},bound{};
 unsigned publications{};
};
struct Lease {std::shared_ptr<const ctr::NativeGameHeapApi> api;std::shared_ptr<std::mutex> operation;};
struct PointerGuard {
 std::byte* cell{};std::uint64_t saved{};
 explicit PointerGuard(const ctr::InstalledGameGlobalHeap& binding){
  auto& b=*binding.backend;if(!b.heap.mutex)return;
  auto bytes=b.memory.bytes();cell=bytes.data()+b.heap.header_token-0x60000000+0x378;
  std::memcpy(&saved,cell,8);const auto actual=reinterpret_cast<std::uint64_t>(&b.heap.mutex->prefix);
  if(saved!=actual&&saved!=0x4000e000)throw std::invalid_argument("Declared guest/native mutex pointer adapter required");
  std::memcpy(cell,&actual,8);
 }
 ~PointerGuard(){if(cell)std::memcpy(cell,&saved,8);}
};
}
extern "C" {
__declspec(dllexport) Session* ctr_heap_session_create(){try{return new Session;}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_heap_session_destroy(Session* s){delete s;}
__declspec(dllexport) ctr::NativeDirectMemory* ctr_heap_session_memory(Session* s){return s?&s->backend->memory:nullptr;}
// Original bootstrap produced this memory; do not recreate/copy its heap.
// Mutex must come from this SAME DLL. Ownership transfers exactly once on success.
__declspec(dllexport) int ctr_heap_bind_original_backend(Session* s,const std::uint64_t* fields,ctr::NativeMutex* mutex,unsigned poison,unsigned debug){try{
 if(!s||!fields||s->bound||poison>1||debug>1)throw std::invalid_argument("Single original backend binding required");
 auto& b=*s->backend;auto bytes=b.memory.bytes();
 if(bytes.size()!=0x4000||reinterpret_cast<std::uint64_t>(bytes.data())!=0x60000000||fields[0]!=0x025b9c90||fields[1]!=0||fields[2]!=0x60000020||fields[3]!=0x026ee0b8||fields[4]!=0x4000)throw std::invalid_argument("Actual original preconfigured backend required");
 std::uint64_t guest{};std::memcpy(&guest,bytes.data()+0x20+0x378,8);
 if(debug){if(!mutex||guest!=0x4000e000||mutex->prefix.flags!=0x101||mutex->prefix.owner.load())throw std::invalid_argument("Actual original same-provider mutex required");}
 else if(mutex||guest)throw std::invalid_argument("Mutex-disabled original path required");
 b.original_descriptor_identity=fields[0];b.physical=0;b.heap.header_token=fields[2];b.original_manager_identity=fields[3];b.capacity=fields[4];b.initialized_flag=0;
 b.heap.mutex.reset(mutex);s->poison=poison!=0;s->bound=true;return 0;
 }catch(...){return 22;}}
__declspec(dllexport) int ctr_heap_session_native_bootstrap(Session* s,unsigned poison,unsigned debug){try{
 if(!s||s->bound||poison>1||debug>1)throw std::invalid_argument("Fresh native behavior session required");
 ctr::MspaceGlobalConfiguration g{static_cast<std::uint8_t>(poison),1,1,1,static_cast<std::uint8_t>(debug)};
 ctr::initialize_preconfigured_system_pool(*s->backend,0x4000,g);s->poison=poison!=0;s->bound=true;return 0;
 }catch(...){return 22;}}
__declspec(dllexport) int ctr_heap_publish_original_table(Session* s,const void* graph512,const void* slots192,std::uint64_t argument){try{
 if(!s||!s->bound||!graph512||!slots192)throw std::invalid_argument("Live original graph/slots/backend required");
 const auto bytes=std::span(static_cast<const std::byte*>(graph512),512);
 auto graph=std::make_shared<const ctr::GameParameterGraph>(bytes.first(456));
 auto parameter=std::make_shared<const ctr::GameZeroHeapApiParameter>(bytes.subspan(456,56));
 const auto supplied=std::span(static_cast<const std::byte*>(slots192),192);std::array<std::byte,192> check{};std::memcpy(check.data(),slots192,192);std::array<std::byte,8> pub{};
 const auto installed=ctr::install_game_global_callbacks(graph,check,pub,0x2000ffc0,0x2000fff0);
 if(std::memcmp(check.data(),supplied.data(),192))throw std::invalid_argument("Table must contain actual original installed stores");
 const auto boundary=ctr::prepare_game_heap_api_publication(installed,parameter,0x50000000);
 ctr::publish_game_heap_api(s->registry,s->backend,boundary,supplied,argument,s->poison);++s->publications;return 0;
 }catch(...){return 22;}}
__declspec(dllexport) Lease* ctr_heap_acquire(Session* s){try{return s?new Lease{s->registry.acquire(),s->operation}:nullptr;}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_heap_lease_destroy(Lease* p){delete p;}
__declspec(dllexport) void ctr_heap_unpublish(Session* s){if(s)s->registry.unpublish();}
__declspec(dllexport) unsigned ctr_heap_publication_count(Session* s){return s?s->publications:0;}
__declspec(dllexport) const void* ctr_heap_lease_table_address(Lease* p){return p?p->api.get():nullptr;}
__declspec(dllexport) int ctr_heap_lease_slots(Lease* p,void* out){if(!p||!out)return 22;std::memcpy(out,p->api->binding().slots.data(),192);return 0;}
__declspec(dllexport) int ctr_heap_lease_malloc(Lease* p,std::uint64_t size,std::uint64_t* out){try{
 if(!p||!out)return 22;std::lock_guard lock(*p->operation);PointerGuard pointers(p->api->binding());const auto allocation=p->api->allocate(size);*out=allocation.address_token;return 0;
 }catch(...){return 22;}}
__declspec(dllexport) int ctr_heap_lease_free(Lease* p,std::uint64_t address){try{
 if(!p)return 22;std::lock_guard lock(*p->operation);PointerGuard pointers(p->api->binding());return static_cast<int>(p->api->release(address));
 }catch(...){return 22;}}
}
