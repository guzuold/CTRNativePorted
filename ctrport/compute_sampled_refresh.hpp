// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
namespace ctrport {
// Refresh an existing sampled image from a completed, verified compute guest
// generation. Providers must perform real upload/fence and download/retile.
// Ready is published only after byte comparison and a second guest reread.
// This does not make a frame complete or admit a draw's other dependencies.
using ComputeSampledUpload=int(*)(void*,const unsigned char*,uint64_t);
inline int refresh_compute_sampled(const ComputeGuestWriteback& owner,const CtrGuestCopyApi& memory,
 ComputeSampledUpload upload,ComputeTiledDownload download,void* context,uint64_t& compared){
 compared=0;
 if(!owner.completed || owner.poisoned)return 88;
 if(memory.version!=1 || memory.size!=sizeof(memory) || !memory.read || !owner.address || owner.initial.empty() || owner.initial.size()>16*1024*1024 || owner.address>UINT64_MAX-owner.initial.size())return 60;
 if(!upload || !download)return 88;
 try{
  std::vector<unsigned char> current(owner.initial.size()),returned(owner.initial.size());
  int status=memory.read(memory.context,owner.address,current.data(),current.size());if(status)return status;
  if(current!=owner.initial)return 88;
  status=upload(context,owner.initial.data(),owner.initial.size());if(status)return status;
  status=download(context,owner.initial.data(),owner.initial.size(),returned.data(),returned.size());if(status)return status;
  if(returned!=owner.initial)return 112;
  status=memory.read(memory.context,owner.address,current.data(),current.size());if(status)return status;
  if(current!=owner.initial)return 88;
  compared=owner.initial.size();return 0;
 }catch(...){return 112;}
}
inline int compute_sampled_refresh_control(){
 GuestStack stack(32768,4096);auto* bytes=reinterpret_cast<unsigned char*>(stack.bottom());std::fill_n(bytes,64,static_cast<unsigned char>(0x25));
 ComputeGuestWriteback owner;owner.address=reinterpret_cast<uint64_t>(bytes);owner.initial.assign(bytes,bytes+64);
 const auto memory=make_owned_guest_copy_api();
 struct State{unsigned uploads=0,downloads=0;int upload_status=0,download_status=0;bool corrupt=false;unsigned char* mutate=nullptr;};State state;
 const auto upload=+[](void* raw,const unsigned char*,uint64_t)->int{auto& s=*static_cast<State*>(raw);++s.uploads;return s.upload_status;};
 const auto download=+[](void* raw,const unsigned char* input,uint64_t size,unsigned char* output,uint64_t capacity)->int{auto& s=*static_cast<State*>(raw);++s.downloads;if(s.download_status)return s.download_status;if(size!=capacity)return 60;std::copy_n(input,static_cast<size_t>(size),output);if(s.corrupt)output[0]^=1;if(s.mutate)*s.mutate^=1;return 0;};
 uint64_t compared=99;
 if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=88 || compared || state.uploads)return 1;
 owner.completed=true;owner.poisoned=true;if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=88 || state.uploads)return 2;owner.poisoned=false;
 bytes[0]^=1;if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=88 || state.uploads)return 3;bytes[0]^=1;
 state.upload_status=90;if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=90 || compared || state.downloads)return 4;state.upload_status=0;
 state.download_status=88;if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=88 || compared)return 5;state.download_status=0;
 state.corrupt=true;if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=112 || compared)return 6;state.corrupt=false;
 state.mutate=bytes;if(refresh_compute_sampled(owner,memory,upload,download,&state,compared)!=88 || compared)return 7;bytes[0]^=1;state.mutate=nullptr;
 if(refresh_compute_sampled(owner,memory,upload,download,&state,compared) || compared!=64)return 8;
 std::cout<<"{\"scope\":\"compute_sampled_refresh_control\",\"passed\":true,\"real_owned_guest_read\":true,\"synthetic_GPU_providers\":true,\"failed_or_stale_generation_not_ready\":true,\"byte_mismatch_rejected\":true,\"GPU_execution\":false}\n";return 0;
}
}
