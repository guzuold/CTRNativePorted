#include "ctr/native_direct_memory.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
namespace ctr {
NativeDirectMemory::NativeDirectMemory(std::uint64_t budget):budget_(budget){
 if(!budget || budget>0x4000000 || (budget&0x3fff))throw std::invalid_argument("Bounded native backing budget required");
}
NativeDirectMemory::~NativeDirectMemory(){release();}
int NativeDirectMemory::allocate(std::int64_t start,std::int64_t end,std::uint64_t length,std::uint64_t alignment,int type,std::int64_t* physical){
 if(!physical || start!=0 || end<0 || static_cast<std::uint64_t>(end)>budget_ || !length || (length&0x3fff) || alignment!=0x200000 || type!=0)return 22;
 if(reservation_ || static_cast<std::uint64_t>(end)<length)return 11;
 // Explicit low address policy fits original supplied-backing libc export bounds.
 auto* p=VirtualAlloc(reinterpret_cast<void*>(0x60000000ull),static_cast<SIZE_T>(length),MEM_RESERVE,PAGE_READWRITE);
 if(!p)return 11;
 reservation_=p;length_=length;*physical=0;return 0;
}
int NativeDirectMemory::map(void** address,std::uint64_t length,int protection,int flags,std::int64_t physical,std::uint64_t alignment){
 if(!address || *address || !reservation_ || mapped_ || length!=length_ || protection!=0x33 || flags || physical!=0 || alignment!=0x200000)return 22;
 if(!VirtualAlloc(reservation_,static_cast<SIZE_T>(length_),MEM_COMMIT,PAGE_READWRITE))return 11;
 mapped_=true;*address=reservation_;return 0;
}
int NativeDirectMemory::release(){
 if(!reservation_)return 22;
 if(!VirtualFree(reservation_,0,MEM_RELEASE))return 11;
 reservation_=nullptr;length_=0;mapped_=false;return 0;
}
std::span<std::byte> NativeDirectMemory::bytes()const{
 if(!mapped_)throw std::invalid_argument("Native backing has not been mapped");
 return {static_cast<std::byte*>(reservation_),static_cast<std::size_t>(length_)};
}
}
