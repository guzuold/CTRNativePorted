#include "ctr/system_pool_grow_reallocation.h"
#include "ctr/mspace_usable_size.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation dispatch_grow_reallocation_system_pool(std::span<std::byte> pool,
 SystemPoolBackend& b,const SystemPoolAllocation& old,std::uint64_t request,
 std::uint32_t alignment,bool poison) {
 auto backing=b.memory.bytes();
 if(pool.size()!=176 || old.payload.empty() || old.payload.size()>256 || request<1 || request>512 || alignment!=8 ||
    old.raw_address<0x60000010 || old.address!=old.raw_address+16 || old.address-0x60000000+old.payload.size()>backing.size() ||
    b.heap.header_token<0x60000000 || b.heap.header_token-0x60000000+0x520>backing.size())
  throw std::invalid_argument("Bounded live alignment8 allocation required");
 auto read=[&](std::size_t at){if(at>backing.size()-8)throw std::invalid_argument("Heap extent missing");std::uint64_t v;std::memcpy(&v,backing.data()+at,8);return v;};
 auto write=[&](std::size_t at,std::uint64_t v){std::memcpy(backing.data()+at,&v,8);};
 auto get=[&](std::size_t at){std::uint64_t v;std::memcpy(&v,pool.data()+at,8);return v;};
 auto put=[&](std::size_t at,std::uint64_t v){std::memcpy(pool.data()+at,&v,8);};
 const auto h=b.heap.header_token-0x60000000,raw=old.raw_address-0x60000000,chunk=raw-16;
 const auto marker=read(chunk+8),size=marker&0xff00fffffffffff8ull;
 const auto requested_raw=request+16,new_size=(requested_raw+0x2f)&~31ull;
 const auto remaining=read(h+0x10),new_top=chunk+new_size;
 std::uint32_t minimum,flags,tracking;std::memcpy(&minimum,pool.data()+0x30,4);
 std::memcpy(&flags,backing.data()+h+0x370,4);std::memcpy(&tracking,backing.data()+h+0x3f0,4);
 if(get(0)!=0x025b9ca8 || get(0xa8)!=b.heap.header_token || get(0x10) || minimum!=8 || pool[0x18]!=std::byte{1} ||
    pool[0x4b]!=std::byte{0} || pool[0x4c]!=std::byte{0} || pool[0x4d]!=std::byte{0} ||
    read(raw)!=((16ull<<11)|1) || read(raw+8)!=old.payload.size() || old.payload.data()!=backing.data()+raw+16 ||
    get(0x50)!=1 || get(0x60)!=old.payload.size() || get(0x70)!=old.usable_size ||
    !(marker&2) || (marker&(1ull<<55)) || new_size<=size || remaining<=new_size-size ||
    read(h+0x28)!=0x60000000+chunk+size || new_top+16>backing.size() ||
    (read(chunk+size+8)&0xff00fffffffffff8ull)!=remaining ||
    read(chunk+size)!=(b.heap.header_token^(poison?0x58585858ull:0ull)) ||
    read(h+0x38)!=(poison?0x58585858ull:0ull) || (tracking&1) || (flags&0x10) ||
    read(h) || read(h+8) || read(h+0x20) || read(h+0x360)<size)
  throw std::invalid_argument("Only untracked growth directly into free top reconstructed");
 struct Lock{NativeMutex* value{};~Lock(){if(value)value->unlock();}void release(){auto* p=value;value=nullptr;if(p&&p->unlock())throw std::runtime_error("Grow unlock failed");}} lock;
 if(flags&2){if(!b.heap.mutex || read(h+0x378)!=reinterpret_cast<std::uint64_t>(&b.heap.mutex->prefix))throw std::invalid_argument("Actual mutex missing");if(b.heap.mutex->lock())throw std::runtime_error("Grow lock failed");lock.value=b.heap.mutex.get();}
 // libc+594ec..595dd grows the existing chunk into top; no payload copy.
 write(chunk+8,new_size|(marker&1)|2);backing[new_top+8]|=std::byte{1};
 write(new_top,b.heap.header_token^(poison?0x58585858ull:0ull));
 const auto low=(requested_raw+16)&31;
 if(low){const auto count=31-low;std::memset(backing.data()+new_top-(32-low),0xaf,static_cast<std::size_t>(count));backing[new_top-1]=std::byte((~count)&255);backing[chunk+8]|=std::byte{4};}
 write(new_top+8,(remaining+size-new_size)|1);write(h+0x28,0x60000000+new_top);write(h+0x10,remaining+size-new_size);
 const auto total=read(h+0x360)+new_size-size;write(h+0x360,total);write(h+0x368,std::max(read(h+0x368),total));lock.release();
 // Concrete014d81d0 rewrites wrapper and updates net pool totals after libc.
 write(raw,(read(raw)&0xffffffff00000000ull)|((16ull<<11)|1));write(raw+8,request);
 const auto usable=mspace_usable_size(backing,static_cast<std::size_t>(raw));
 put(0x60,get(0x60)-old.payload.size()+request);put(0x68,std::max(get(0x68),get(0x60)));
 put(0x70,get(0x70)-old.usable_size+usable);put(0x78,std::max(get(0x78),get(0x70)));
 std::uint32_t calls;std::memcpy(&calls,pool.data()+0x80,4);++calls;std::memcpy(pool.data()+0x80,&calls,4);
 return {old.raw_address,old.address,usable,backing.subspan(static_cast<std::size_t>(raw+16),static_cast<std::size_t>(request))};
}
}
