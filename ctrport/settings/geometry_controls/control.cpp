// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "distance_hook.hpp"
#include <array>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#include <atomic>
extern "C" std::uint64_t ctr_call_original_registers(void*,const std::uint64_t*,const std::uint64_t*,std::uint64_t*);
extern "C" void ctr_msvc_probe(){std::abort();}
extern "C" void ctr_sysv_orbis_condition_broadcast(){std::abort();}
extern "C" void ctr_thread_exit_frame(){std::abort();}
namespace gc=ctrport::settings::geometry_controls;
namespace {
std::atomic<unsigned> calls{0},checks{0};
void require(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class T>void put(unsigned char*p,std::size_t off,T v){std::memcpy(p+off,&v,sizeof(v));}
struct Fixture{
 std::array<unsigned char,0x130> scene{};
 std::array<unsigned char,0x420> records{};
 std::uint64_t name=0x123456;
 std::array<float,5> d{40,80,160,320,640},f{4,8,16,32,64};
 void setup(int count=2,bool match=true){
  scene.fill(0);records.fill(0xa5);put(scene.data(),0x108,count);put(scene.data(),0x118,reinterpret_cast<std::uintptr_t>(records.data()));
  put(records.data(),0x1f0,std::uint64_t(99));put(records.data(),0x210+0x1f0,match?name:100);
 }
 void call(unsigned char* image,bool enabled=true,bool null_inputs=false){
  const auto before_d=d,before_f=f;
  std::array<std::uint64_t,6> args{reinterpret_cast<std::uintptr_t>(scene.data()),reinterpret_cast<std::uintptr_t>(&name),enabled?1u:0u,
   null_inputs?0:reinterpret_cast<std::uintptr_t>(d.data()),null_inputs?0:reinterpret_cast<std::uintptr_t>(f.data()),0};
  std::array<std::uint64_t,8> fp{};std::uint64_t result{};
  ctr_call_original_registers(image+0x34f200,args.data(),fp.data(),&result);++calls;
  require(std::memcmp(d.data(),before_d.data(),sizeof(d))==0 && std::memcmp(f.data(),before_f.data(),sizeof(f))==0,"authored input mutated");
 }
};
template<class F>void reject(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}require(rejected,"negative control accepted");}
}
int wmain(int argc,wchar_t**argv){
 unsigned char* image=nullptr;
 try{
  if(argc!=2)return 2;constexpr std::size_t extent=0x2000000;
  image=static_cast<unsigned char*>(VirtualAlloc(nullptr,extent,MEM_RESERVE,PAGE_NOACCESS));require(image!=nullptr,"image reserve");
  std::ifstream in(argv[1],std::ios::binary);std::uint32_t n=0;in.read(reinterpret_cast<char*>(&n),4);require(n==10,"span count");
  for(unsigned i=0;i<n;++i){std::uint32_t a=0,s=0;in.read(reinterpret_cast<char*>(&a),4);in.read(reinterpret_cast<char*>(&s),4);
   require(a<extent && s<=extent-a,"fixture bounds");const auto lo=a&~0xfffu,hi=(a+s+4095)&~0xfffu;
   require(VirtualAlloc(image+lo,hi-lo,MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"commit");in.read(reinterpret_cast<char*>(image+a),s);require(bool(in),"fixture read");
  }
  constexpr auto sha="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
  std::array<unsigned char,508> original{};std::memcpy(original.data(),image+0x34f200,508);
  require(!gc::install(image,extent,sha,gc::DistanceScale::original),"original mode patched");
  reject([&]{gc::install(image,extent,"wrong",gc::DistanceScale::half);});
  reject([&]{gc::install(image,0x34f210,sha,gc::DistanceScale::half);});
  reject([&]{gc::install(image,extent,sha,static_cast<gc::DistanceScale>(99));});
  image[0x34f200]^=1;reject([&]{gc::install(image,extent,sha,gc::DistanceScale::half);});image[0x34f200]^=1;
  DWORD old=0;require(VirtualProtect(image+0x34f200,17,PAGE_EXECUTE_READ,&old)!=0,"readonly setup");
  reject([&]{gc::install(image,extent,sha,gc::DistanceScale::half);});
  DWORD ignored=0;require(VirtualProtect(image+0x34f200,17,PAGE_NOACCESS,&ignored)!=0,"no-access setup");
  reject([&]{gc::install(image,extent,sha,gc::DistanceScale::half);});
  require(VirtualProtect(image+0x34f200,17,old,&ignored)!=0,"writable restore");
  for(auto mode:{gc::DistanceScale::half,gc::DistanceScale::three_quarters}){
   const float scale=mode==gc::DistanceScale::half?0.5f:0.75f;
   // Compare hooked execution against separately executed ORIGINAL setter
   // supplied with scaled inputs. No reimplementation of its math is the oracle.
   for(unsigned test=0;test<16;++test){
    Fixture expected;expected.setup();for(unsigned i=0;i<5;++i){expected.d[i]=(41.25f+test*3.125f)*(i+1)*scale;expected.f[i]=(4.5f+test*.25f)*(i+1)*scale;}expected.call(image);
    auto hook=gc::install(image,extent,sha,mode);
    Fixture actual;actual.setup();for(unsigned i=0;i<5;++i){actual.d[i]=(41.25f+test*3.125f)*(i+1);actual.f[i]=(4.5f+test*.25f)*(i+1);}actual.call(image);
    require(actual.records==expected.records,"scaled original CPU result mismatch");
    require(actual.records[0x210+0x1c4]==1,"culling was disabled");
    hook->uninstall();hook.reset();require(std::memcmp(original.data(),image+0x34f200,508)==0,"original restore mismatch");
   }
   // No-match, empty-scene and disabled paths must not touch null input arrays.
   for(unsigned path=0;path<3;++path){
    Fixture expected;expected.setup(path==0?0:2,path!=1);expected.call(image,path!=2,true);
    auto hook=gc::install(image,extent,sha,mode);Fixture actual;actual.setup(path==0?0:2,path!=1);actual.call(image,path!=2,true);
    require(actual.records==expected.records,"no-op path changed");
   }
   for(float bad:{0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::denorm_min()}){
    Fixture expected;expected.setup();expected.d[2]=bad;expected.call(image);
    auto hook=gc::install(image,extent,sha,mode);Fixture actual;actual.setup();actual.d[2]=bad;actual.call(image);
    require(actual.records==expected.records,"invalid input passthrough changed");
   }
   for(float bad:{0.0f,-2.0f,0.000001f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
    Fixture expected;expected.setup();expected.f[1]=bad;expected.call(image);
    auto hook=gc::install(image,extent,sha,mode);Fixture actual;actual.setup();actual.f[1]=bad;actual.call(image);
    require(actual.records==expected.records,"invalid fade passthrough changed");
   }
   Fixture expected;expected.setup();for(auto&v:expected.d)v*=scale;for(auto&v:expected.f)v*=scale;expected.call(image);
   auto hook=gc::install(image,extent,sha,mode);reject([&]{gc::install(image,extent,sha,mode);});
   std::atomic<unsigned> failures{0};std::vector<std::thread> workers;
   for(unsigned t=0;t<8;++t)workers.emplace_back([&]{try{for(unsigned i=0;i<20;++i){Fixture actual;actual.setup();actual.call(image);if(actual.records!=expected.records)++failures;}}catch(...){++failures;}});
   for(auto&t:workers)t.join();require(failures==0,"concurrent independent scenes changed");
  }
  require(std::memcmp(original.data(),image+0x34f200,508)==0,"final original restore mismatch");
  VirtualFree(image,0,MEM_RELEASE);image=nullptr;
  std::cout<<"GEOMETRY_DISTANCE_CPU_VERIFIED checks="<<checks<<" original_cpu_calls="<<calls<<" threads=8 visual_verified=false\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';if(image)VirtualFree(image,0,MEM_RELEASE);return 1;}
}

