// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "world_hook.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
extern "C" std::uint64_t ctr_call_original_registers(void*,const std::uint64_t*,const std::uint64_t*,std::uint64_t*);
extern "C" void ctr_msvc_probe(){std::abort();}
extern "C" void ctr_sysv_orbis_condition_broadcast(){std::abort();}
extern "C" void ctr_thread_exit_frame(){std::abort();}
extern "C" void quality_sysv_nonvolatile_probe();
namespace qc=ctrport::settings::quality_controls;
std::atomic<unsigned> checks{0},calls{0};unsigned char* image=nullptr;
void require(bool b,const char*t){++checks;if(!b)throw std::runtime_error(t);}
template<class T>void put(unsigned char*p,std::size_t off,T v){std::memcpy(p+off,&v,sizeof(v));}
template<class T>T get(const unsigned char*p,std::size_t off){T v;std::memcpy(&v,p+off,sizeof(v));return v;}
template<class F>void reject(F f){bool b=false;try{f();}catch(const std::invalid_argument&){b=true;}require(b,"negative accepted");}
void call(std::size_t address,std::uint64_t a,std::uint64_t b=0){std::array<std::uint64_t,6>args{a,b,0,0,0,0};std::array<std::uint64_t,8>fp{};std::uint64_t result{};ctr_call_original_registers(image+address,args.data(),fp.data(),&result);++calls;}
void boundary(std::size_t at){require(VirtualAlloc(image+(at&~std::size_t(4095)),4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"boundary page");const unsigned char zero[]{0x31,0xc0,0xc3};std::memcpy(image+at,zero,3);}
struct Fixture{
 std::array<unsigned char,0x200> encoder{},draw{},data{},alt{},material{},instance{},pass{},stream{},vertices{},indices{},vertex_desc{},index_desc{},vertex_res{},format_res{},index_res{},index_format{};
 std::array<std::uintptr_t,32> methods{};alignas(16) std::array<unsigned char,4096> commands{};
 Fixture(){
  put(draw.data(),0x10,reinterpret_cast<std::uintptr_t>(data.data()));put(draw.data(),0x38,reinterpret_cast<std::uintptr_t>(material.data()));put(draw.data(),0x60,reinterpret_cast<std::uintptr_t>(instance.data()));
  put(data.data(),0x58,reinterpret_cast<std::uintptr_t>(alt.data()));put(data.data(),0x68,1200u);put(data.data(),0x6c,24u);put(data.data(),0x78,-1);
  put(data.data(),0x80,std::uint64_t(101));put(data.data(),0x88,std::uint64_t(102));put(data.data(),0x90,std::uint64_t(103));
  put(encoder.data(),0x18,reinterpret_cast<std::uintptr_t>(stream.data()));put(encoder.data(),0x60,reinterpret_cast<std::uintptr_t>(pass.data()));
  put(pass.data(),0x11c,-1);put(instance.data(),0x100,-1);
  // Original alternate-resource publisher, with explicit pre-created synthetic
  // buffer resources. Its initializer boundary neither creates nor frees them.
  methods[0x16]=reinterpret_cast<std::uintptr_t>(image+0x149c340);
  put(vertices.data(),0,reinterpret_cast<std::uintptr_t>(methods.data()));put(indices.data(),0,reinterpret_cast<std::uintptr_t>(methods.data()));
  put(vertices.data(),0x20,reinterpret_cast<std::uintptr_t>(vertex_desc.data()));put(indices.data(),0x20,reinterpret_cast<std::uintptr_t>(index_desc.data()));
  put(vertex_desc.data(),0xc,36u);put(index_desc.data(),0xc,36u);put(vertex_desc.data(),0x38,3u);put(index_desc.data(),0x38,3u);
  put(index_desc.data(),0x30,reinterpret_cast<std::uintptr_t>(index_format.data()));put(index_format.data(),0xc,0u);
  put(vertices.data(),0x28,reinterpret_cast<std::uintptr_t>(vertex_res.data()));put(vertices.data(),0x30,reinterpret_cast<std::uintptr_t>(format_res.data()));put(indices.data(),0x28,reinterpret_cast<std::uintptr_t>(index_res.data()));
  put(vertex_res.data(),0x10,std::uint64_t(201));put(format_res.data(),0x10,std::uint64_t(102));put(index_res.data(),0x10,std::uint64_t(203));
  put(alt.data(),0x10,reinterpret_cast<std::uintptr_t>(vertices.data()));put(alt.data(),0x18,reinterpret_cast<std::uintptr_t>(indices.data()));
  call(0x3492e0,reinterpret_cast<std::uintptr_t>(alt.data()));require(get<unsigned>(alt.data(),0x20)==12,"native alternate primitive count");require(get<std::uint64_t>(alt.data(),0x30)==201&&get<std::uint64_t>(alt.data(),0x38)==102&&get<std::uint64_t>(alt.data(),0x40)==203,"native alternate resource publish");
 }
 void select_original_inputs(){std::memcpy(data.data()+0x68,alt.data()+0x20,4);std::memcpy(data.data()+0x80,alt.data()+0x30,8);std::memcpy(data.data()+0x90,alt.data()+0x40,8);}
 std::vector<unsigned char> run(bool empty=false){commands.fill(0);put(stream.data(),0x10,reinterpret_cast<std::uintptr_t>(commands.data()));put(stream.data(),0x20,reinterpret_cast<std::uintptr_t>(commands.data()+commands.size()));auto d=draw,a=data;
  call(0x348350,reinterpret_cast<std::uintptr_t>(encoder.data()),reinterpret_cast<std::uintptr_t>(draw.data()));require(draw==d&&data==a,"authored draw mutated");
  auto n=get<std::uintptr_t>(stream.data(),0x10)-reinterpret_cast<std::uintptr_t>(commands.data());require(n<=commands.size()&&(empty||n>0),"commands bounds");return {commands.begin(),commands.begin()+n};
 }
};
int wmain(int argc,wchar_t**argv){try{
 require(argc==2,"arguments");constexpr std::size_t extent=0x2b00000;image=static_cast<unsigned char*>(VirtualAlloc(nullptr,extent,MEM_RESERVE,PAGE_NOACCESS));require(image!=nullptr,"reserve");
 std::ifstream in(argv[1],std::ios::binary);std::uint32_t n{};in.read(reinterpret_cast<char*>(&n),4);require(n==18,"span count");for(unsigned k=0;k<n;++k){std::uint32_t a{},s{};in.read(reinterpret_cast<char*>(&a),4);in.read(reinterpret_cast<char*>(&s),4);require(a<extent&&s<=extent-a,"span bounds");require(VirtualAlloc(image+(a&~0xfffu),((a+s+4095)&~0xfffu)-(a&~0xfffu),MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"commit");in.read(reinterpret_cast<char*>(image+a),s);require(bool(in),"read");}
 // Only independent material/lighting/animation boundaries are synthetic.
 // Actual native command emission, cache publisher and primitive-count helper
 // execute unmodified. Production contains none of these fixture bindings.
 for(std::size_t at:{0x13e44e0,0x13f1260,0x13e46b0,0x13e8c90,0x13f11c0,0x13e8c70,0x13f13e0,0x156aa40,0x13f14a0,0x348b10,0x13f1ab0,0x348c40,0x13f1c80,0x3491b0,0x149c340})boundary(at);
 constexpr auto sha="39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
 std::array<unsigned char,17> original{};std::memcpy(original.data(),image+0x348350,17);
 Fixture reference;reference.select_original_inputs();auto gold=reference.run();
 require(gold.size()==72&&get<unsigned>(gold.data(),0)==2&&get<std::uint64_t>(gold.data(),8)==201&&get<std::uint64_t>(gold.data(),16)==102&&get<unsigned>(gold.data(),32)==3&&get<std::uint64_t>(gold.data(),40)==203&&get<unsigned>(gold.data(),56)==0x2c&&get<unsigned>(gold.data(),64)==12,"decoded native geometry commands");Fixture positive;auto full=positive.run();require(gold!=full,"positive reduction absent");
 auto hook=qc::install_world(image,extent,sha,qc::WorldLod::reduced);Fixture actual;require(actual.run()==gold,"original reduced command mismatch");hook->uninstall();require(actual.run()==full,"rollback behavior");
 for(unsigned variant=0;variant<12;++variant){Fixture f;
  if(variant==0)put(f.data.data(),0x58,std::uintptr_t(0));if(variant==1)put(f.alt.data(),0x30,std::uint64_t(0));if(variant==2)put(f.alt.data(),0x38,std::uint64_t(999));if(variant==3)put(f.alt.data(),0x40,std::uint64_t(0));if(variant==4)put(f.alt.data(),0x20,0u);if(variant==5)put(f.alt.data(),0x20,1201u);if(variant==6)put(f.alt.data(),0x24,1u);if(variant==7)put(f.draw.data(),0x28,std::uint64_t(1));if(variant==8)put(f.draw.data(),0x30,std::uint64_t(1));if(variant==9)put(f.data.data(),0x78,0);if(variant==10)put(f.data.data(),0x58,std::uintptr_t(1));if(variant==11)put(f.alt.data(),0x20,1200u);
  auto expected=f.run();auto h=qc::install_world(image,extent,sha,qc::WorldLod::reduced);require(f.run()==expected,"incompatible fallback differs");h->uninstall();
 }
 for(bool reset:{false,true}){
  Fixture expected;expected.select_original_inputs();put(expected.pass.data(),0x11c,5);expected.pass[0x160]=static_cast<unsigned char>(reset);auto wanted=expected.run();
  Fixture f;put(f.pass.data(),0x11c,5);f.pass[0x160]=static_cast<unsigned char>(reset);auto h=qc::install_world(image,extent,sha,qc::WorldLod::reduced);require(f.run()==wanted,"stencil exit branch differs");h->uninstall();
  require(get<std::uint64_t>(expected.encoder.data(),0x68)==get<std::uint64_t>(f.encoder.data(),0x68)&&get<unsigned>(expected.encoder.data(),0x78)==get<unsigned>(f.encoder.data(),0x78),"encoder state lost");
 }
 // The native missing-technique exit must remain safe even when no data exists.
 const unsigned char missing[]{0xb8,0xff,0xff,0xff,0xff,0xc3};std::memcpy(image+0x13e44e0,missing,6);
 for(std::uintptr_t pointer:{std::uintptr_t(0),std::uintptr_t(1)}){Fixture f;put(f.draw.data(),0x10,pointer);require(f.run(true).empty(),"original early exit");auto h=qc::install_world(image,extent,sha,qc::WorldLod::reduced);require(f.run(true).empty(),"hook early exit");h->uninstall();}
 boundary(0x13e44e0);
 for(bool enabled:{false,true}){
  Fixture f;f.run();put(f.stream.data(),0x10,reinterpret_cast<std::uintptr_t>(f.commands.data()));std::unique_ptr<qc::WorldHook> h;if(enabled)h=qc::install_world(image,extent,sha,qc::WorldLod::reduced);
  std::array<std::uint64_t,8> observed{},fp{};std::array<std::uint64_t,6> args{reinterpret_cast<std::uintptr_t>(image+0x348350),reinterpret_cast<std::uintptr_t>(f.encoder.data()),reinterpret_cast<std::uintptr_t>(f.draw.data()),reinterpret_cast<std::uintptr_t>(observed.data()),0,0};std::uint64_t r{};
  ctr_call_original_registers(reinterpret_cast<void*>(&quality_sysv_nonvolatile_probe),args.data(),fp.data(),&r);++calls;
  const std::array<std::uint64_t,8> wanted{0x1111,0x2222,0x3333,0x4444,0x5555,0x6666,0,0x12345678};require(observed==wanted,"SysV nonvolatile or stack corruption");
 }
 reject([&]{qc::install_world(image,extent,"bad",qc::WorldLod::reduced);});reject([&]{qc::install_world(image,0x348355,sha,qc::WorldLod::reduced);});reject([&]{qc::install_world(image,extent,sha,static_cast<qc::WorldLod>(99));});
 image[0x348350]^=1;reject([&]{qc::install_world(image,extent,sha,qc::WorldLod::reduced);});image[0x348350]^=1;
 DWORD old{},ignored{};require(VirtualProtect(image+0x348350,17,PAGE_EXECUTE_READ,&old)!=0,"readonly");reject([&]{qc::install_world(image,extent,sha,qc::WorldLod::reduced);});require(VirtualProtect(image+0x348350,17,old,&ignored)!=0,"restore protection");
 hook=qc::install_world(image,extent,sha,qc::WorldLod::reduced);reject([&]{qc::install_world(image,extent,sha,qc::WorldLod::reduced);});std::atomic<unsigned> failures{0};std::vector<std::thread> threads;
 for(unsigned t=0;t<8;++t)threads.emplace_back([&]{try{for(unsigned j=0;j<20;++j){Fixture f;if(f.run()!=gold)++failures;}}catch(...){++failures;}});for(auto&t:threads)t.join();require(failures==0,"parallel independent draws");hook->uninstall();hook.reset();require(std::memcmp(original.data(),image+0x348350,17)==0,"byte restore");
 VirtualFree(image,0,MEM_RELEASE);image=nullptr;std::cout<<"QUALITY_WORLD_CPU_VERIFIED checks="<<checks<<" original_cpu_calls="<<calls<<" threads=8 visual_verified=false\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
