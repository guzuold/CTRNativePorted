// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "mesh_override.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
extern "C" std::uint64_t ctr_call_original_registers(void*,const std::uint64_t*,const std::uint64_t*,std::uint64_t*);
extern "C" void ctr_msvc_probe(){std::abort();}
extern "C" void ctr_sysv_orbis_condition_broadcast(){std::abort();}
extern "C" void ctr_thread_exit_frame(){std::abort();}
namespace qc=ctrport::settings::quality_controls;
unsigned checks=0,calls=0;
void require(bool b,const char* text){++checks;if(!b)throw std::runtime_error(text);}
template<class T>void put(unsigned char*p,std::size_t off,T v){std::memcpy(p+off,&v,sizeof(v));}
template<class F>void reject(F f){bool b=false;try{f();}catch(const std::invalid_argument&){b=true;}require(b,"negative accepted");}
struct Fixture {
 std::array<unsigned char,0x100> racer{},camera{},settings{},handle{},entity{};
 Fixture(float distance,int highest,int near_lod,int far_lod,float range){
  put(racer.data(),0x20,reinterpret_cast<std::uintptr_t>(handle.data()));put(handle.data(),0x28,reinterpret_cast<std::uintptr_t>(entity.data()));
  put(entity.data(),0x20,distance);put(settings.data(),0xc,highest);put(settings.data(),0x18,far_lod);put(settings.data(),0x1c,near_lod);put(settings.data(),0x20,range);
 }
 int call(unsigned char* image,bool forced){
  auto r=racer,c=camera,s=settings,h=handle,e=entity;
  std::array<std::uint64_t,6>a{reinterpret_cast<std::uintptr_t>(racer.data()),reinterpret_cast<std::uintptr_t>(camera.data()),reinterpret_cast<std::uintptr_t>(settings.data()),forced?1u:0u,0,0};
  std::array<std::uint64_t,8>fp{};std::uint64_t result{};auto value=ctr_call_original_registers(image+0xeadf00,a.data(),fp.data(),&result);++calls;
  require(r==racer&&c==camera&&s==settings&&h==handle&&e==entity,"input mutation");return static_cast<int>(value);
 }
};
int wmain(int argc,wchar_t**argv){unsigned char*image=nullptr;try{
 require(argc==2,"arguments");constexpr std::size_t extent=0x2b00000;
 image=static_cast<unsigned char*>(VirtualAlloc(nullptr,extent,MEM_RESERVE,PAGE_NOACCESS));require(image!=nullptr,"reserve");
 std::ifstream in(argv[1],std::ios::binary);std::uint32_t n=0;in.read(reinterpret_cast<char*>(&n),4);require(n==18,"span count");
 for(unsigned i=0;i<n;++i){std::uint32_t a=0,s=0;in.read(reinterpret_cast<char*>(&a),4);in.read(reinterpret_cast<char*>(&s),4);require(a<extent&&s<=extent-a,"span bounds");require(VirtualAlloc(image+(a&~0xfffu),((a+s+4095)&~0xfffu)-(a&~0xfffu),MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"commit");in.read(reinterpret_cast<char*>(image+a),s);require(bool(in),"read");}
 require(VirtualAlloc(image+0x2807000,4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"guard slot");std::uint64_t guard=0x12345678;put(image,0x2807098,reinterpret_cast<std::uintptr_t>(&guard));
 constexpr auto sha="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
 reject([&]{qc::apply_mesh_highest(image,extent,"bad");});reject([&]{qc::apply_mesh_highest(image,qc::mesh_site+5,sha);});
 image[qc::mesh_site]^=1;reject([&]{qc::apply_mesh_highest(image,extent,sha);});image[qc::mesh_site]^=1;
 unsigned different=0;
 for(int high=0;high<3;++high)for(float distance:{0.f,1.f,10.f,50.f,100.f,500.f})for(float range:{10.f,100.f}){
  Fixture f(distance,high,high,high+3,range);const auto before=f.call(image,false),expected=f.call(image,true);
  qc::apply_mesh_highest(image,extent,sha);require(f.call(image,false)==expected,"patched differs from original forced path");require(f.call(image,true)==expected,"forced path changed");
  reject([&]{qc::apply_mesh_highest(image,extent,sha);});qc::rollback_mesh(image,extent,sha);require(f.call(image,false)==before,"rollback changed original");
  if(before!=expected)++different;
 }
 require(different>0,"positive control unchanged");qc::verify_mesh(image,extent,sha,false);VirtualFree(image,0,MEM_RELEASE);
 std::cout<<"QUALITY_MESH_CPU_VERIFIED checks="<<checks<<" original_cpu_calls="<<calls<<" changed_positive_cases="<<different<<" visual_verified=false\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';if(image)VirtualFree(image,0,MEM_RELEASE);return 1;}}
