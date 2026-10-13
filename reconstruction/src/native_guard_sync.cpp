#define NOMINMAX
#include "ctr/native_guard_sync.h"
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <new>
#include <stdexcept>
namespace ctr {namespace {
struct Mutex {SRWLOCK lock;std::atomic<std::uint64_t> owner;std::atomic<std::uint32_t> locks,unlocks;};
struct Condition {CONDITION_VARIABLE condition;std::atomic<std::uint32_t> waiting,broadcasts;};
Mutex* mutex(void* p){return reinterpret_cast<Mutex*>(static_cast<std::byte*>(p)+0x80);}
Condition* condition(void* p){return reinterpret_cast<Condition*>(static_cast<std::byte*>(p)+0x100);}
bool valid(void* actual,void* expected,void* object){if(actual!=expected)return false;void* value{};std::memcpy(&value,actual,8);return value==object;}
void need(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
std::uint64_t value(std::span<std::byte> b){std::uint64_t v{};std::memcpy(&v,b.data(),8);return v;}
void guard_span(NativeGuardSync& sync,std::span<std::byte> b){const auto page=sync.page();const auto p=reinterpret_cast<std::uintptr_t>(b.data()),base=reinterpret_cast<std::uintptr_t>(page.data());need(b.size()==8&&p>=base+0x200&&p<=base+0x278&&(p-base)%8==0,"Actual aligned owned guard span required");}
}
NativeGuardSync::NativeGuardSync(){page_=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!page_)throw std::runtime_error("Guard synchronization memory unavailable");std::memset(page_,0xa5,4096);std::memset(static_cast<std::byte*>(page_)+0x80,0,32);std::memset(static_cast<std::byte*>(page_)+0x100,0,32);auto m=new(static_cast<std::byte*>(page_)+0x80)Mutex{};auto c=new(static_cast<std::byte*>(page_)+0x100)Condition{};InitializeSRWLock(&m->lock);InitializeConditionVariable(&c->condition);std::memcpy(mutex_cell(),&m,8);std::memcpy(condition_cell(),&c,8);}
NativeGuardSync::~NativeGuardSync(){if(page_){mutex(page_)->~Mutex();condition(page_)->~Condition();VirtualFree(page_,0,MEM_RELEASE);}}
std::span<std::byte> NativeGuardSync::page()const{return {static_cast<std::byte*>(page_),4096};}
void* NativeGuardSync::mutex_cell()const{return static_cast<std::byte*>(page_)+0x40;}
void* NativeGuardSync::condition_cell()const{return static_cast<std::byte*>(page_)+0x48;}
int NativeGuardSync::lock(void* cell)noexcept{auto m=mutex(page_);if(!valid(cell,mutex_cell(),m))return 22;const auto id=GetCurrentThreadId();if(m->owner.load()==id)return 11;AcquireSRWLockExclusive(&m->lock);m->owner.store(id);m->locks.fetch_add(1);return 0;}
int NativeGuardSync::unlock(void* cell)noexcept{auto m=mutex(page_);if(!valid(cell,mutex_cell(),m))return 22;if(m->owner.load()!=GetCurrentThreadId())return 1;m->owner.store(0);m->unlocks.fetch_add(1);ReleaseSRWLockExclusive(&m->lock);return 0;}
int NativeGuardSync::wait(void* cond,void* cell,std::uint32_t timeout)noexcept{auto m=mutex(page_);auto c=condition(page_);if(!valid(cell,mutex_cell(),m)||!valid(cond,condition_cell(),c))return 22;if(m->owner.load()!=GetCurrentThreadId())return 1;c->waiting.fetch_add(1);m->owner.store(0);const BOOL ok=SleepConditionVariableSRW(&c->condition,&m->lock,timeout,0);const auto error=ok?0:GetLastError();m->owner.store(GetCurrentThreadId());c->waiting.fetch_sub(1);return ok?0:error==ERROR_TIMEOUT?110:22;}
int NativeGuardSync::broadcast(void* cond)noexcept{auto c=condition(page_);if(!valid(cond,condition_cell(),c))return 22;c->broadcasts.fetch_add(1);WakeAllConditionVariable(&c->condition);return 0;}
std::uint32_t NativeGuardSync::waiters()const noexcept{return condition(page_)->waiting.load();}
int acquire_guard_without_wait(NativeGuardSync& sync,std::span<std::byte> b){guard_span(sync,b);need(sync.lock(sync.mutex_cell())==0,"Actual guard mutex lock failed");const auto v=value(b);if(v!=0&&v!=1){sync.unlock(sync.mutex_cell());throw std::invalid_argument("Pending/wider guard path excluded");}int result=0;if(!v){const std::uint64_t pending=0x100;std::memcpy(b.data(),&pending,8);result=1;}if(sync.unlock(sync.mutex_cell()))throw std::runtime_error("Actual guard mutex unlock failed");return result;}
void release_pending_guard(NativeGuardSync& sync,std::span<std::byte> b){guard_span(sync,b);need(sync.lock(sync.mutex_cell())==0,"Actual guard mutex lock failed");if(value(b)!=0x100){sync.unlock(sync.mutex_cell());throw std::invalid_argument("Only pending guard release covered");}const std::uint64_t ready=1;std::memcpy(b.data(),&ready,8);if(sync.unlock(sync.mutex_cell())||sync.broadcast(sync.condition_cell()))throw std::runtime_error("Actual unlock/broadcast failed");}
namespace {
void frame(NativeGuardSync& sync,std::span<std::byte> guard,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){guard_span(sync,guard);need(stack.size()==128&&entry[14]==0x2000fff8&&entry[5]==reinterpret_cast<std::uint64_t>(guard.data()),"Explicit owned guard/caller frame required");const auto st=reinterpret_cast<std::uintptr_t>(stack.data()),base=reinterpret_cast<std::uintptr_t>(sync.page().data());need(st>=base+4096||st+128<=base,"Guard stack alias unsupported");}
void save(std::span<std::byte> stack,std::size_t offset,std::uint64_t v){std::memcpy(stack.data()+offset,&v,8);}
}
GuardCallResult invoke_guard_acquire_no_wait(NativeGuardSync& sync,std::span<std::byte> guard,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){frame(sync,guard,entry,stack);const auto status=acquire_guard_without_wait(sync,guard);save(stack,112,entry[15]);save(stack,104,entry[13]);save(stack,96,entry[12]);save(stack,88,entry[10]);save(stack,80,entry[1]);save(stack,72,0x5002ba24);GuardCallResult out{entry};out.registers[0]=static_cast<std::uint64_t>(status);out.registers[5]=reinterpret_cast<std::uint64_t>(sync.mutex_cell());out.registers[14]=0x20010000;return out;}
GuardCallResult invoke_guard_release_pending(NativeGuardSync& sync,std::span<std::byte> guard,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack){frame(sync,guard,entry,stack);release_pending_guard(sync,guard);save(stack,112,entry[15]);save(stack,104,entry[1]);save(stack,96,entry[0]);save(stack,88,0x5002ba9c);GuardCallResult out{entry};out.registers[0]=0;out.registers[5]=reinterpret_cast<std::uint64_t>(sync.condition_cell());out.registers[14]=0x20010000;return out;}

}
