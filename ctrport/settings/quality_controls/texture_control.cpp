// SPDX-License-Identifier: GPL-2.0-only
// Native original CPU oracle with explicitly synthetic format and allocator
// boundaries. No graphics service is stubbed in the production implementation.
#define NOMINMAX
#include <windows.h>
#include "texture_hook.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#define SYSV __attribute__((sysv_abi))
unsigned checks=0,calls=0,allocations=0,frees=0;unsigned char* image=nullptr;
std::map<void*,std::size_t> memory;
std::array<unsigned char,0x100> pool{},context{},bootstrap{},core{};
void require(bool b,const char*t){++checks;if(!b)throw std::runtime_error(t);}
template<class T>void put(unsigned char*p,std::size_t off,T v){std::memcpy(p+off,&v,sizeof(v));}
template<class T>T get(const unsigned char*p,std::size_t off){T v;std::memcpy(&v,p+off,sizeof(v));return v;}
void* alloc(std::size_t n){auto*p=std::malloc(n);if(!p)throw std::bad_alloc();memory[p]=n;++allocations;return p;}
void release(void*p){if(!p)return;require(memory.erase(p)==1,"free unknown pointer");std::free(p);++frees;}
extern "C" void SYSV fixture_registry(void*,const char*,int*,bool){}
extern "C" void* SYSV fixture_pool(void*,void*,bool){return pool.data();}
extern "C" void* SYSV fixture_allocate(void*,void*p,std::size_t n,unsigned,bool,std::size_t){if(n==0){release(p);return nullptr;}require(!p,"unexpected realloc");return alloc(n);}
extern "C" void* SYSV fixture_copy(void*d,const void*s,std::size_t n){require(memory.count(d)&&memory[d]>=n,"copy destination bounds");bool bounded=false;for(const auto&entry:memory){const auto start=reinterpret_cast<std::uintptr_t>(entry.first),source=reinterpret_cast<std::uintptr_t>(s);if(source>=start&&source-start<=entry.second&&n<=entry.second-(source-start))bounded=true;}require(bounded,"copy source bounds");return std::memcpy(d,s,n);}
extern "C" bool SYSV fixture_format_valid(void*,unsigned,unsigned,unsigned,bool){return true;}
extern "C" std::size_t SYSV fixture_format_size(unsigned char*f,unsigned w,unsigned h,unsigned d,unsigned levels,unsigned count,std::uint64_t target,std::uint64_t slice){
 const unsigned bw=f[0x19],bh=f[0x1a],bytes=f[0x1b];std::size_t total=0,offset=0;
 for(unsigned m=0;m<levels;++m){if(m==target)offset=total;total+=std::size_t((w+bw-1)/bw)*((h+bh-1)/bh)*d*bytes;w=std::max(1u,w>>1);h=std::max(1u,h>>1);d=std::max(1u,d>>1);}
 return target==~std::uint64_t(0)?total*count:total*slice+offset;
}
void jump(std::size_t at,void*p){auto*dest=image+at;auto lo=at&~std::size_t(4095);require(VirtualAlloc(image+lo,4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"callback page");unsigned char op[]{0xff,0x25,0,0,0,0};std::memcpy(dest,op,6);std::memcpy(dest+6,&p,8);}
using Post=void(SYSV *)(void*,void*);
LONG CALLBACK exception_log(EXCEPTION_POINTERS*e){if(e->ExceptionRecord->ExceptionCode==0xe06d7363)return EXCEPTION_CONTINUE_SEARCH;std::cerr<<"CPU_EXCEPTION "<<std::hex<<e->ExceptionRecord->ExceptionCode<<" rva="<<(e->ContextRecord->Rip-reinterpret_cast<std::uintptr_t>(image))<<" addr="<<e->ExceptionRecord->ExceptionInformation[1]<<"\n";return EXCEPTION_CONTINUE_SEARCH;}
template<class F>void reject(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}require(rejected,"negative accepted");}
struct Fixture {
 std::array<unsigned char,0x100> img{},dir{},params{},format{},list{};std::array<std::uintptr_t,32> vtable{};std::array<std::uint64_t,2> work{};std::vector<unsigned char> initial;
 Fixture(unsigned w,unsigned h,unsigned levels,unsigned authored,bool borrowed,unsigned block=1){
  put(img.data(),0x18,static_cast<std::uint16_t>(w));put(img.data(),0x1a,static_cast<std::uint16_t>(h));put(img.data(),0x1c,std::uint16_t(1));put(img.data(),0x1e,static_cast<std::uint16_t>(levels));put(img.data(),0x20,std::uint16_t(1));
  put(format.data(),0,reinterpret_cast<std::uintptr_t>(vtable.data()));vtable[0x17]=reinterpret_cast<std::uintptr_t>(&fixture_format_size);put(format.data(),8,100u);format[0x19]=static_cast<unsigned char>(block);format[0x1a]=static_cast<unsigned char>(block);format[0x1b]=static_cast<unsigned char>(block==1?4:8);put(img.data(),0x28,reinterpret_cast<std::uintptr_t>(format.data()));
  auto size=fixture_format_size(format.data(),w,h,1,levels,1,~std::uint64_t(0),0);initial.resize(size);for(std::size_t n=0;n<size;++n)initial[n]=static_cast<unsigned char>((n*17+n/1024)&255);auto*p=alloc(size);std::memcpy(p,initial.data(),size);
  put(img.data(),0x38,std::uint64_t(size)|(borrowed?(1ull<<56):0));put(img.data(),0x40,reinterpret_cast<std::uintptr_t>(p));
  params[0x63]=static_cast<unsigned char>(authored);put(dir.data(),0x90,reinterpret_cast<std::uintptr_t>(params.data()));
  work={get<std::uint64_t>(img.data(),0x38),reinterpret_cast<std::uintptr_t>(p)};put(list.data(),0xc,1);put(list.data(),0x20,reinterpret_cast<std::uintptr_t>(work.data()));put(dir.data(),0x58,reinterpret_cast<std::uintptr_t>(list.data()));
 }
 void call(bool null_dir=false){const auto before=params;reinterpret_cast<Post>(image+0x141be40)(img.data(),null_dir?nullptr:dir.data());++calls;require(params==before,"file parameters mutated");}
 std::vector<unsigned char> bytes()const{auto*p=get<unsigned char*>(img.data(),0x40);auto n=get<std::uint64_t>(img.data(),0x38)&0xfffffffffffffull;return {p,p+n};}
 void cleanup(){release(get<void*>(img.data(),0x40));put(img.data(),0x40,std::uintptr_t(0));}
};
int wmain(int argc,wchar_t**argv){try{
 require(argc==2,"arguments");constexpr std::size_t extent=0x2b00000;image=static_cast<unsigned char*>(VirtualAlloc(nullptr,extent,MEM_RESERVE,PAGE_NOACCESS));require(image!=nullptr,"reserve");AddVectoredExceptionHandler(1,exception_log);
 std::ifstream in(argv[1],std::ios::binary);std::uint32_t n{};in.read(reinterpret_cast<char*>(&n),4);require(n==18,"span count");
 for(unsigned k=0;k<n;++k){std::uint32_t a{},s{};in.read(reinterpret_cast<char*>(&a),4);in.read(reinterpret_cast<char*>(&s),4);require(a<extent&&s<=extent-a,"span bounds");require(VirtualAlloc(image+(a&~0xfffu),((a+s+4095)&~0xfffu)-(a&~0xfffu),MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"commit");in.read(reinterpret_cast<char*>(image+a),s);require(bool(in),"read");}
 for(auto a:{0x2807000,0x2af1000,0x2aee000})require(VirtualAlloc(image+a,4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"global page");
 std::uint64_t guard=0x98765432;put(image,0x2807098,reinterpret_cast<std::uintptr_t>(&guard));put(image,0x2af1fa8,reinterpret_cast<std::uintptr_t>(context.data()));put(context.data(),0x18,reinterpret_cast<std::uintptr_t>(pool.data()));put(pool.data(),0x30,16u);put(pool.data(),0x1c,0u);
 put(image,0x2aee178,reinterpret_cast<std::uintptr_t>(bootstrap.data()));put(bootstrap.data(),0x30,std::uint64_t(1));put(image,0x2af1f78,reinterpret_cast<std::uintptr_t>(core.data()));
 jump(0x156e130,reinterpret_cast<void*>(&fixture_registry));jump(0x14a3c60,reinterpret_cast<void*>(&fixture_pool));jump(0x14a00b0,reinterpret_cast<void*>(&fixture_allocate));jump(0x1be9be8,reinterpret_cast<void*>(&fixture_copy));jump(0x1457570,reinterpret_cast<void*>(&fixture_format_valid));
 constexpr auto sha="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";namespace qc=ctrport::settings::quality_controls;
 require(!qc::install_texture(image,extent,sha,qc::TextureMips::original),"original patched");
 reject([&]{qc::install_texture(image,extent,"bad",qc::TextureMips::half);});
 reject([&]{qc::install_texture(image,0x141bea0,sha,qc::TextureMips::half);});
 reject([&]{qc::install_texture(image,extent,sha,static_cast<qc::TextureMips>(99));});
 image[0x141be9c]^=1;reject([&]{qc::install_texture(image,extent,sha,qc::TextureMips::half);});image[0x141be9c]^=1;
 DWORD old{},ignored{};require(VirtualProtect(image+0x141be9c,16,PAGE_EXECUTE_READ,&old)!=0,"readonly");
 reject([&]{qc::install_texture(image,extent,sha,qc::TextureMips::half);});require(VirtualProtect(image+0x141be9c,16,old,&ignored)!=0,"write restore");
 std::array<unsigned char,407> original{};std::memcpy(original.data(),image+0x141be40,original.size());
 for(unsigned drop:{1u,2u})for(bool borrowed:{false,true})for(unsigned block:{1u,4u})for(unsigned authored:{0u,1u,3u}){
  const unsigned chosen=std::max(drop,authored);Fixture expected(512,256,6,chosen,borrowed,block);expected.call();auto gold=expected.bytes();require(gold.size()<expected.initial.size(),"original positive residency reduction");require(std::equal(gold.begin(),gold.end(),expected.initial.end()-gold.size()),"original payload not exact mip suffix");
  const auto w=get<std::uint16_t>(expected.img.data(),0x18),h=get<std::uint16_t>(expected.img.data(),0x1a),levels=get<std::uint16_t>(expected.img.data(),0x1e);
  auto hook=qc::install_texture(image,extent,sha,drop==1?qc::TextureMips::half:qc::TextureMips::quarter);Fixture actual(512,256,6,authored,borrowed,block);actual.call();
  require(actual.bytes()==gold,"native payload mismatch");require(get<std::uint16_t>(actual.img.data(),0x18)==w&&get<std::uint16_t>(actual.img.data(),0x1a)==h&&get<std::uint16_t>(actual.img.data(),0x1e)==levels,"native dimensions mismatch");
  if(borrowed)require(actual.work[1]==get<std::uint64_t>(actual.img.data(),0x40)&&((actual.work[0]&0xffffffff)==gold.size()),"directory ownership mismatch");
  reject([&]{qc::install_texture(image,extent,sha,qc::TextureMips::half);});hook->uninstall();
  require(std::memcmp(original.data(),image+0x141be40,original.size())==0,"rollback bytes");
  Fixture restored(512,256,6,0,borrowed,block);restored.call();require(restored.bytes()==restored.initial,"rollback behavioral");
  restored.cleanup();expected.cleanup();actual.cleanup();require(memory.empty(),"leaked allocation");
 }
 for(unsigned variant=0;variant<8;++variant){
  Fixture f(variant==0?128:512,variant==1?128:256,variant==2?1:6,0,true);
  if(variant==3)put(f.img.data(),0x1c,std::uint16_t(4));
  if(variant==4)put(f.img.data(),0x20,std::uint16_t(6));
  if(variant==7)f.params[0x63]=255;
  if(variant==5)put(f.dir.data(),0x90,std::uintptr_t(0));
  auto h=qc::install_texture(image,extent,sha,qc::TextureMips::quarter);const auto fields=f.img;f.call(variant==6);
  require(f.img==fields&&f.bytes()==f.initial,"excluded image changed");h->uninstall();f.cleanup();require(memory.empty(),"excluded image leak");
 }
 VirtualFree(image,0,MEM_RELEASE);image=nullptr;std::cout<<"QUALITY_TEXTURE_CPU_VERIFIED checks="<<checks<<" original_cpu_calls="<<calls<<" allocations="<<allocations<<" frees="<<frees<<" visual_verified=false\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
