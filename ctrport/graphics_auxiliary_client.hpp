// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_key_client.hpp"
namespace ctrport {
// Own the output of the whole public shadPS4 auxiliary emitter, revision
// e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
// This is transport validation, not an independent SPIR-V semantic validator.
struct GraphicsAuxiliaryOwner {std::vector<uint32_t> control,evaluation;};
struct GraphicsAuxiliarySink {uint32_t kind=0,calls=0;std::vector<uint32_t> words;};
inline int graphics_auxiliary_sink(void* raw,uint32_t kind,const uint32_t* words,uint32_t count){
 if(!raw || !words || kind>1 || count<5 || count>1024*1024)return 60;
 auto& sink=*static_cast<GraphicsAuxiliarySink*>(raw);
 if(sink.calls || sink.kind!=kind || words[0]!=0x07230203 || words[1]!=0x00010500 || !words[3] || words[4])return 112;
 unsigned entries=0;
 for(size_t at=5;at<count;){const uint32_t size=words[at]>>16,opcode=words[at]&65535;
  if(!size || size>count-at)return 112;
  if(opcode==15){if(size<4 || words[at+1]!=kind+1 || ++entries!=1)return 112;}
  at+=size;
 }
 if(entries!=1)return 112;
 try{std::vector<uint32_t> owned(words,words+count);sink.words.swap(owned);++sink.calls;return 0;}catch(...){return 112;}
}
inline int compile_owned_vertex_auxiliary(GraphicsAuxiliaryOwner& output){
 return graphics_metadata_host_call([&](){
  wchar_t path[4096]{};const auto length=GetFullPathNameW(L"build/CTRPort-infra/graphics_companions/ctr_graphics_companion_v29.dll",4096,path,nullptr);if(!length || length>=4096)return 88;
  HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!module)return 88;
  struct Loaded{HMODULE module;~Loaded(){FreeLibrary(module);}} loaded{module};
  const auto call=reinterpret_cast<decltype(&ctr_graphics_auxiliary_vertex_only_v1)>(GetProcAddress(module,"ctr_graphics_auxiliary_vertex_only_v1"));if(!call)return 88;
  GraphicsAuxiliaryOwner prepared;
  for(uint32_t kind=0;kind<2;++kind){GraphicsAuxiliarySink sink;sink.kind=kind;
   const int status=call(kind,graphics_auxiliary_sink,&sink);if(status)return status;if(sink.calls!=1)return 112;
   (kind?prepared.evaluation:prepared.control).swap(sink.words);
  }
  output=std::move(prepared);return 0;
 });
}
inline int graphics_auxiliary_transport_control(){
 // Authored structural transport only, not executable shader fixtures.
 uint32_t words[]={0x07230203,0x00010500,0,2,0,(4u<<16)|15,1,1,0};
 GraphicsAuxiliarySink sink;
 if(graphics_auxiliary_sink(&sink,0,words,9) || sink.calls!=1 || sink.words.size()!=9)return 1;
 const auto saved=sink.words;words[7]=99;if(sink.words!=saved || graphics_auxiliary_sink(&sink,0,words,9)!=112 || sink.words!=saved)return 2;
 GraphicsAuxiliarySink rejected;words[5]=15;
 if(graphics_auxiliary_sink(&rejected,0,words,9)!=112 || rejected.calls || !rejected.words.empty())return 3;
 words[5]=(5u<<16)|15;if(graphics_auxiliary_sink(&rejected,0,words,9)!=112 || rejected.calls)return 4;
 words[5]=(4u<<16)|15;words[6]=2;if(graphics_auxiliary_sink(&rejected,0,words,9)!=112 || rejected.calls)return 5;
 if(graphics_auxiliary_sink(nullptr,0,words,9)!=60 || graphics_auxiliary_sink(&rejected,2,words,9)!=60)return 6;
 GraphicsAuxiliaryOwner actual;const int status=compile_owned_vertex_auxiliary(actual);if(status)return status;
 if(actual.control.empty() || actual.evaluation.empty())return 7;
 std::cout<<"{\"scope\":\"graphics_auxiliary_transport_control\",\"passed\":true,\"control_words\":"<<actual.control.size()<<",\"evaluation_words\":"<<actual.evaluation.size()<<",\"owned_after_DLL_unload\":true,\"duplicate_rejected\":true,\"invalid_atomic\":true,\"synthetic_transport_negatives\":true,\"public_emitter_compiled\":true,\"GPU_execution\":false}\n";return 0;
}
}
