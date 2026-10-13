#include "ctr/bitmap_align16_suffix_free.h"
#include <cstring>
namespace ctr { namespace {
template<class T>T read(std::span<std::byte> s,std::size_t n){T v;std::memcpy(&v,s.data()+n,sizeof(v));return v;}
template<class T>void write(std::span<std::byte> s,std::size_t n,T v){std::memcpy(s.data()+n,&v,sizeof(v));}
}
std::uint32_t free_normal_bitmap_align16_suffix(std::span<std::byte> p,SystemPoolBackend& b,std::uint64_t pointer){
 // Existing exact five-live getter proves ownership, chain, requested footer, maps and guards.
 const auto requested=bitmap_align16_suffix_requested_size(p,b,pointer);
 auto bytes=b.memory.bytes();const auto base=reinterpret_cast<std::uintptr_t>(bytes.data()),h=read<std::uint64_t>(p,0xa8);auto s=bytes.subspan(static_cast<std::size_t>(h-base),8192);
 const auto chunk=static_cast<std::size_t>(pointer-h-16),marker=read<std::uint64_t>(s,chunk+8),size=marker&~std::uint64_t{3},next=chunk+static_cast<std::size_t>(size)+8;const auto index=static_cast<unsigned>(size/8);
 // Original nonlast small insertion; actual large tail and four live neighbors retained.
 write(s,next,h+chunk);write(s,next+8,read<std::uint64_t>(s,next+8)|2);write(s,chunk+8,marker|1);
 write(s,chunk+16,h);write(s,chunk+24,h);write(s,0x18,h+chunk);write(s,0x88+index*8,h+chunk);write(s,0x24,1u<<index);write(s,0x20,read<std::uint32_t>(s,0x20)|1u);
 write(p,0x70,read<std::uint64_t>(p,0x70)-size-8);write(p,0x60,read<std::uint64_t>(p,0x60)-requested);write(p,0x50,std::uint64_t{4});return 0;
}
}
