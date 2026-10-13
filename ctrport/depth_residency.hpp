// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gpu_resident_memory.hpp"
#include "gnm_acquire_mem.hpp"
#include "video_session_owner.hpp"
#include "video/graphics_companion.h"
namespace ctrport {
// Genuine GPU image leases, exact public logical intervals, conservative page
// exclusion. Padding is CPU exclusion only, never a stencil sample layout.
class DepthResidency {
 struct Entry {void* context;uint64_t token,lease,generation;uint32_t writer;CtrDepthBackingSpansV1 spans;CtrDepthMS2LayoutV1 layout;uint64_t guard_begin,guard_bytes;std::shared_ptr<void> owner;std::shared_ptr<const std::vector<unsigned char>> prior;};
 // Accessed only on the retained presentation owner thread; process teardown
 // occurs after guest workers joined. The guard owns an independent lease copy.
 std::vector<Entry> entries_;
public:
 int retain(const CtrVideoSessionApiV1* api,std::shared_ptr<RuntimeVideoSession> session,uint64_t token,uint32_t writer,const CtrDepthMS2LayoutV1& layout,uint64_t stencil){
  if(!api || !api->context || !session || !token)return 60;
  CtrDepthBackingSpansV1 spans{};const int metadata=layout.samples==1?ctr_video_depth_single_spans_v1(&layout,stencil,&spans):ctr_video_depth_ms2_spans_v1(&layout,stencil,&spans);if(metadata)return metadata;
  for(auto& entry:entries_)if(entry.context==api->context && entry.token==token){
   if(entry.spans.depth_address!=spans.depth_address || entry.spans.depth_bytes!=spans.depth_bytes || entry.spans.stencil_address!=spans.stencil_address || entry.spans.stencil_bytes!=spans.stencil_bytes)return 88;
   // The same allocation token cannot silently change its sampled view.
   const auto& prior=entry.layout;
   if(prior.width!=layout.width || prior.height!=layout.height || prior.pitch!=layout.pitch ||
      prior.samples!=layout.samples || prior.z_format!=layout.z_format || prior.stencil_format!=layout.stencil_format ||
      prior.tile_mode!=layout.tile_mode || prior.slice_tile_max!=layout.slice_tile_max ||
      prior.slice_start!=layout.slice_start || prior.slice_max!=layout.slice_max ||
      prior.alt_tile!=layout.alt_tile || prior.bank_swizzle!=layout.bank_swizzle)return 88;
   if(entry.writer==writer)return 0;const int status=runtime_gpu_resident_memory.advance(entry.guard_begin,entry.guard_bytes,entry.lease,entry.generation,entry.generation+1);
   if(status)return status;++entry.generation;entry.writer=writer;return 0;
  }

  // This reached backing has adjacent depth/stencil intervals. Other placement
  // must establish separately validated page ownership rather than fill a gap.
  const bool depth_only=layout.samples==1 && layout.z_format==1 && !layout.stencil_format;
  if(depth_only){if(stencil || spans.stencil_address || spans.stencil_bytes)return 88;}
  else if(spans.depth_address+spans.depth_bytes!=spans.stencil_address)return 88;
  if(!spans.depth_address || !spans.depth_bytes || spans.depth_address>UINT64_MAX-spans.depth_bytes)return 60;
  SYSTEM_INFO info{};GetSystemInfo(&info);const uint64_t page=info.dwPageSize,end=depth_only?spans.depth_address+spans.depth_bytes:spans.stencil_address+spans.stencil_bytes;
  if(!page || end>UINT64_MAX-(page-1))return 60;
  const uint64_t begin=spans.depth_address/page*page,guard_end=(end+page-1)/page*page;
  uint64_t lease=0;const int retained=ctr_video_depth_attachment_retain_v1(api->context,token,&lease);if(retained)return retained;if(!lease || lease==token)return 112;
  auto owner=std::shared_ptr<void>(new uint64_t(lease),[session](void* value){const auto lease=*static_cast<uint64_t*>(value);try{session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_depth_attachment_release_v1(api->context,lease);});}catch(...){OutputDebugStringA("Depth residency lease shutdown unavailable\n");}delete static_cast<uint64_t*>(value);});
  try{entries_.reserve(entries_.size()+1);}catch(...){return 112;}
  for(auto& entry:entries_)if(entry.context==api->context && entry.spans.depth_address==spans.depth_address && entry.spans.depth_bytes==spans.depth_bytes && entry.spans.stencil_address==spans.stencil_address && entry.spans.stencil_bytes==spans.stencil_bytes){
   const auto& prior=entry.layout;
   if(prior.width!=layout.width || prior.height!=layout.height || prior.pitch!=layout.pitch || prior.samples!=layout.samples || prior.z_format!=layout.z_format || prior.stencil_format!=layout.stencil_format || prior.tile_mode!=layout.tile_mode || prior.slice_tile_max!=layout.slice_tile_max || prior.slice_start!=layout.slice_start || prior.slice_max!=layout.slice_max || prior.alt_tile!=layout.alt_tile || prior.bank_swizzle!=layout.bank_swizzle)return 88;
   const int covered=covers(api->context,entry.token,entry.writer);if(covered)return covered;
   const int replaced=runtime_gpu_resident_memory.replace(begin,guard_end-begin,entry.lease,entry.generation,lease,entry.generation+1,owner);if(replaced)return replaced;
   entry.token=token;entry.lease=lease;++entry.generation;entry.writer=writer;entry.layout=layout;entry.owner=std::move(owner);return 0;
  }
  std::shared_ptr<std::vector<unsigned char>> prior;try{prior=std::make_shared<std::vector<unsigned char>>();}catch(...){return 112;}
  // Exact shifted reuse reached after bounded compute publication: the new
  // whole GPU owner replaces ALL remaining old guard pages, never a fake suffix view.
  for(auto& entry:entries_)if(entry.context==api->context && begin>entry.guard_begin && begin<entry.guard_begin+entry.guard_bytes){
   uint64_t old_image=0,old_generation=0,old_begin=0,old_bytes=0;
   if(!runtime_gpu_resident_memory.identify(begin,old_image,old_generation,old_begin,old_bytes) || old_begin!=begin || old_image!=entry.lease || old_generation!=entry.generation+1 || old_generation==UINT64_MAX || old_begin+old_bytes!=entry.guard_begin+entry.guard_bytes || guard_end<=old_begin+old_bytes || !entry.prior || entry.prior->size()!=entry.guard_bytes)return 88;
   if(entry.layout.width!=layout.width || entry.layout.height!=layout.height || entry.layout.pitch!=layout.pitch || entry.layout.samples!=layout.samples || entry.layout.z_format!=layout.z_format || entry.layout.stencil_format!=layout.stencil_format || entry.layout.tile_mode!=layout.tile_mode || entry.spans.depth_bytes!=spans.depth_bytes || entry.spans.stencil_bytes!=spans.stencil_bytes)return 88;
   std::vector<unsigned char> suffix;try{suffix.assign(entry.prior->begin()+static_cast<size_t>(begin-entry.guard_begin),entry.prior->end());}catch(...){return 112;}
   const int replaced=runtime_gpu_resident_memory.replace_extend(begin,guard_end-begin,old_image,old_generation,lease,old_generation+1,owner,suffix,*prior);if(replaced)return replaced;
   // Shifted layout padding can overlap old GPU-written logical cells.
   // Old CPU prior bytes are NOT authority for that padding; require a real
   // backing extraction before any future retile/publication of this surface.
   entry={api->context,token,lease,old_generation+1,writer,spans,layout,begin,guard_end-begin,std::move(owner),nullptr};return 0;
  }
  const int protected_pages=runtime_gpu_resident_memory.protect(begin,guard_end-begin,lease,1,owner,prior.get());if(protected_pages)return protected_pages;
  entries_.push_back({api->context,token,lease,1,writer,spans,layout,begin,guard_end-begin,std::move(owner),std::move(prior)});return 0;
 }
 int covers(void* context,uint64_t token,uint32_t writer)const{
  for(const auto& entry:entries_)if(entry.context==context && entry.token==token && entry.writer==writer){
   uint64_t image=0,generation=0,begin=0,bytes=0;
   if(!entry.owner || !runtime_gpu_resident_memory.identify(entry.guard_begin,image,generation,begin,bytes) || image!=entry.lease || generation!=entry.generation || begin!=entry.guard_begin || bytes!=entry.guard_bytes)return 88;
   for(uint64_t cursor=begin;cursor<begin+bytes;){MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region)) || region.State!=MEM_COMMIT || region.Protect!=PAGE_NOACCESS)return 88;
    const uint64_t next=reinterpret_cast<uint64_t>(region.BaseAddress)+region.RegionSize;
    if(next<=cursor)return 88;cursor=next;
   }
   return 0;
  }return 88;
 }
 // GPU consumers supply exact logical plane identity, never rounded guard
 // padding. Outputs retain the actual owner only after generation validation.
 int resolve(void* context,const CtrDepthBackingSpansV1& spans,uint64_t expected,uint64_t& lease,std::shared_ptr<void>& owner)const{
  if(!context || spans.version!=1 || spans.size!=sizeof(spans) || !expected)return 60;
  for(const auto& entry:entries_)if(entry.context==context &&
    entry.spans.depth_address==spans.depth_address && entry.spans.depth_bytes==spans.depth_bytes &&
    entry.spans.stencil_address==spans.stencil_address && entry.spans.stencil_bytes==spans.stencil_bytes){
   if(entry.generation!=expected)return 88;const int status=covers(context,entry.token,entry.writer);if(status)return status;
   lease=entry.lease;owner=entry.owner;return 0;
  }return 88;
 }
 // Resolve only a complete depth interval at the generation observed by the
 // caller. Full depth+stencil identity and retained guard are checked by resolve.
 int resolve_sampled_interval(void* context,uint64_t address,uint64_t bytes,uint64_t generation,
                              uint64_t& lease,std::shared_ptr<void>& owner)const{
  const Entry* found=nullptr;
  for(const auto& entry:entries_)if(entry.context==context && entry.spans.depth_address==address && entry.spans.depth_bytes==bytes){
   if(found)return 88;found=&entry;
  }
  if(!found)return 88;
  return resolve(context,found->spans,generation,lease,owner);
 }
 static bool tall_d16_view(const CtrGraphicsImageParametersV1& p,const CtrGraphicsImageExtentV1& e){
  return p.version==1 && p.size==sizeof(p) && e.version==1 && e.size==sizeof(e) &&
   p.type==9 && p.width==2048 && p.height==8192 && p.pitch==2048 && p.depth==1 && p.layers==1 && p.levels==1 && p.samples==1 &&
   p.data_format==2 && !p.number_format && !p.tile_mode && !p.pow2pad && !p.base_level && !p.base_array && !p.last_array && !p.compression && !p.alt_tile &&
   p.swizzle[0]==4 && p.swizzle[1]==4 && p.swizzle[2]==4 && p.swizzle[3]==4 && p.guest_address==e.guest_address && e.pitch==2048 && e.padded_height==8192 && e.bytes==33554432;
 }
 // Exact D32 sampled view; backing interval alone is insufficient authority.
 int resolve_sampled_view(void* context,const CtrGraphicsImageParametersV1& p,const CtrGraphicsImageExtentV1& extent,
                          uint64_t generation,uint64_t& lease,std::shared_ptr<void>& owner)const{
  if(p.version!=1 || p.size!=sizeof(p) || extent.version!=1 || extent.size!=sizeof(extent))return 60;
  if(tall_d16_view(p,extent)){
   const Entry* found=nullptr;for(const auto& e:entries_)if(e.context==context && e.spans.depth_address==p.guest_address){if(found)return 88;found=&e;}
   if(!found)return 88;const auto& l=found->layout;
   if(l.width!=2048 || l.height!=8192 || l.pitch!=2048 || l.samples!=1 || l.z_format!=1 || l.stencil_format || l.tile_mode || l.slice_tile_max!=262143 || l.slice_start || l.slice_max || l.alt_tile || l.bank_swizzle)return 88;
   CtrDepthBackingSpansV1 spans{};const int decoded=ctr_video_depth_single_spans_v1(&l,0,&spans);if(decoded)return decoded;
   if(spans.depth_bytes!=extent.bytes || spans.stencil_address || spans.stencil_bytes)return 88;
   return resolve(context,spans,generation,lease,owner);
  }
  if(p.type!=14 || p.depth!=1 || p.layers!=1 || p.levels!=1 || p.samples!=2 || p.data_format!=4 || p.number_format!=7 ||
     p.tile_mode!=1 || p.pow2pad || p.base_level || p.base_array || p.last_array || p.compression || p.alt_tile ||
     p.swizzle[0]!=4 || p.swizzle[1]!=4 || p.swizzle[2]!=4 || p.swizzle[3]!=4 || p.guest_address!=extent.guest_address)return 88;
  const Entry* found=nullptr;
  for(const auto& entry:entries_)if(entry.context==context && entry.spans.depth_address==p.guest_address){if(found)return 88;found=&entry;}
  if(!found || found->layout.width!=p.width || found->layout.height!=p.height || found->layout.pitch!=p.pitch ||
     found->layout.samples!=p.samples || found->layout.z_format!=3 || found->layout.tile_mode!=p.tile_mode || extent.pitch!=p.pitch)return 88;
  CtrGraphicsImageExtentV1 expected{};
  const int decoded=ctr_video_depth_ms2_extent_v1(&found->layout,&expected);if(decoded)return decoded;
  if(expected.bytes!=extent.bytes || expected.padded_height!=extent.padded_height)return 88;
  return resolve_sampled_interval(context,p.guest_address,extent.bytes,generation,lease,owner);
 }
 // Exact attachment reuse preserves original GPU contents across submissions.
 int resolve_attachment(void* context,const CtrDepthMS2LayoutV1& layout,uint64_t stencil,uint64_t& lease)const{
  const Entry* found=nullptr;
  for(const auto& e:entries_)if(e.context==context && e.layout.guest_address==layout.guest_address){
   if(found)return 88;found=&e;
  }
  if(!found)return 62;
  const auto& p=found->layout;
  if(p.width!=layout.width || p.height!=layout.height || p.pitch!=layout.pitch || p.samples!=layout.samples || p.z_format!=layout.z_format || p.stencil_format!=layout.stencil_format || p.tile_mode!=layout.tile_mode || p.slice_tile_max!=layout.slice_tile_max || p.slice_start!=layout.slice_start || p.slice_max!=layout.slice_max || p.alt_tile!=layout.alt_tile || p.bank_swizzle!=layout.bank_swizzle || found->spans.stencil_address!=stencil)return 88;
  const int status=covers(context,found->token,found->writer);if(status)return status;
  lease=found->lease;return 0;
 }
 // Original pre-guard CPU bytes, usable only for unaffected backing/padding.
 // GPU logical samples still require real extraction/conversion and a fence.
 int prior_backing(void* context,uint64_t lease,uint64_t generation,std::shared_ptr<const std::vector<unsigned char>>& output)const{
  for(const auto& entry:entries_)if(entry.context==context && entry.lease==lease){
   if(entry.generation!=generation || !entry.prior || entry.prior->size()!=entry.guard_bytes)return 88;
   const int status=covers(context,entry.token,entry.writer);if(status)return status;output=entry.prior;return 0;
  }return 88;
 }
 // Read-only diagnostic: complete original layout for actual overlapping retained guards.
 template<class Observer>void observe_overlap(void* context,uint64_t address,uint64_t bytes,Observer&& observer)const{
  if(!bytes || address>UINT64_MAX-bytes)return;
  for(const auto& entry:entries_)if(entry.context==context && address<entry.guard_begin+entry.guard_bytes && entry.guard_begin<address+bytes)
   observer(entry.token,entry.lease,entry.writer,entry.generation,entry.layout,entry.spans,entry.guard_begin,entry.guard_bytes,covers(context,entry.token,entry.writer),entry.prior?entry.prior->size():0);
 }
 template<class Observer>void observe(void* context,Observer&& observer)const{for(const auto& entry:entries_)if(entry.context==context)observer(entry.token,entry.lease,entry.writer,entry.generation,entry.spans,entry.guard_begin,entry.guard_bytes);}
};
inline DepthResidency runtime_depth_residency;
inline int depth_d16_residency_control(){
 GuestStack storage(36*1024*1024,4096);auto* bytes=static_cast<unsigned char*>(storage.bottom());
 const uint64_t address=reinterpret_cast<uint64_t>(bytes),size=33554432;
 std::fill_n(bytes,size+4096,static_cast<unsigned char>(0x25));
 if(runtime_video_open(255,0,0,0)!=1)return 1;auto session=runtime_video_session;
 const int result=session->invoke([&](const CtrVideoSessionApiV1* api){
  DepthResidency resident;struct Cleanup{~Cleanup(){runtime_gpu_resident_memory.shutdown();}}cleanup;
  CtrDepthAttachmentParametersV1 p{1,sizeof(p),2048,8192,1,1,0};uint64_t token=0;
  if(ctr_video_depth_attachment_create_v1(api->context,&p,&token))return 2;
  struct Original{void* context;uint64_t token;~Original(){if(token)ctr_video_depth_attachment_release_v1(context,token);}}original{api->context,token};
  if(ctr_video_depth_attachment_clear_v1(api->context,token,0x3e800000,0))return 3;
  CtrDepthMS2LayoutV1 layout{1,sizeof(layout),2048,8192,2048,1,1,0,0,262143,0,0,0,0,address};
  CtrDepthBackingSpansV1 spans{};
  if(ctr_video_depth_single_spans_v1(&layout,0,&spans) || spans.depth_bytes!=size || spans.stencil_address || spans.stencil_bytes)return 4;
  if(resident.retain(api,session,token,4980,layout,0) || resident.covers(api->context,token,4980))return 5;
  uint64_t lease=0;std::shared_ptr<void> owner;
  if(resident.resolve(api->context,spans,1,lease,owner) || !owner || lease==token)return 6;
  auto wrong=spans;wrong.stencil_address=address+size;uint64_t untouched=123;std::shared_ptr<void> absent;
  if(resident.resolve(api->context,wrong,1,untouched,absent)!=88 || untouched!=123 || absent)return 7;
  if(resident.resolve(api->context,spans,2,untouched,absent)!=88 || untouched!=123 || absent)return 8;
  if(bytes[size]!=0x25)return 9;
  MEMORY_BASIC_INFORMATION region{};
  if(!VirtualQuery(bytes+size-1,&region,sizeof(region)) || region.Protect!=PAGE_NOACCESS)return 10;
  if(ctr_video_depth_attachment_release_v1(api->context,token))return 11;original.token=0;
  if(resident.retain(api,session,token,5091,layout,0) || resident.covers(api->context,token,5091))return 12;
  if(resident.resolve(api->context,spans,1,untouched,absent)!=88)return 13;
  if(resident.resolve(api->context,spans,2,lease,owner))return 14;
  CtrGraphicsImageParametersV1 view{};view.version=1;view.size=sizeof(view);view.type=9;view.width=2048;view.height=8192;view.pitch=2048;view.depth=1;view.layers=1;view.levels=1;view.samples=1;view.data_format=2;view.guest_address=address;for(auto& c:view.swizzle)c=4;
  CtrGraphicsImageExtentV1 extent{};extent.version=1;extent.size=sizeof(extent);extent.guest_address=address;extent.pitch=2048;extent.padded_height=8192;extent.bytes=size;
  if(resident.resolve_sampled_view(api->context,view,extent,2,lease,owner))return 18;
  for(unsigned bad=0;bad<8;++bad){auto v=view;auto e=extent;uint64_t unchanged=123;std::shared_ptr<void> none;
   if(bad==0)--v.width;if(bad==1)v.swizzle[1]=5;if(bad==2)v.number_format=7;if(bad==3)--e.bytes;if(bad==4)v.tile_mode=1;if(bad==5)v.samples=2;
   if(resident.resolve_sampled_view(bad==7?nullptr:api->context,v,e,bad==6?1:2,unchanged,none)==0 || unchanged!=123 || none)return 19;
  }
  for(auto offset:{uint64_t(0),size-1})for(bool write:{false,true}){uint64_t fault=0,caller=0,operation=9;
   if(gpu_resident_access_probe(bytes+offset,write,&runtime_gpu_resident_memory,&fault,&caller,&operation)!=62 || fault!=address+offset || !caller || operation!=uint64_t(write))return 20;
  }
  uint64_t alias=0;if(ctr_video_depth_sampled_alias_v1(api->context,lease,&alias))return 15;
  if(ctr_video_sampled_image_release_v1(api->context,alias))return 16;
  runtime_gpu_resident_memory.shutdown();
  if(bytes[0]!=0x25 || bytes[size-1]!=0x25 || resident.covers(api->context,token,5091)!=88)return 17;
  return 0;
 });
 runtime_video_close(1);std::cout<<"{\"scope\":\"D16_depth_only_residency_control\",\"status\":"<<result<<",\"passed\":"<<(result?"false":"true")<<"}\n";return result;
}
inline int depth_single_residency_control(){
 GuestStack storage(8*1024*1024,4096);auto* bytes=static_cast<unsigned char*>(storage.bottom());std::fill_n(bytes,4*1024*1024,static_cast<unsigned char>(0x25));
 const uint64_t address=reinterpret_cast<uint64_t>(bytes),depth_bytes=2621440,stencil_bytes=552960,protected_bytes=depth_bytes+stencil_bytes;
 if(runtime_video_open(255,0,0,0)!=1)return 1;auto session=runtime_video_session;
 const int result=session->invoke([&](const CtrVideoSessionApiV1* api){
  const uint32_t acquire[]={0xc0055800,0x88c40000,1,0,0,0,10};
  const auto visibility=+[](void* context,const GnmAcquireMemRequest& request)->int{
   return acquire_queue_visibility_supported(request)?ctr_video_queue_memory_visibility_v1(context):88;
  };
  if(complete_acquire_mem(acquire,7,visibility,api->context))return 22;
  const uint32_t all_address_acquire[]={0xc0055800,0x2ec47fc0,UINT32_MAX,0,0,0,10};
  if(complete_acquire_mem(all_address_acquire,7,visibility,api->context))return 23;

  DepthResidency resident;
  struct Cleanup {~Cleanup(){runtime_gpu_resident_memory.shutdown();}}cleanup;
  CtrDepthAttachmentParametersV1 parameters{1,sizeof(parameters),960,540,1,3,1};uint64_t token=0;
  if(ctr_video_depth_attachment_create_v1(api->context,&parameters,&token))return 2;
  struct Original {void* context;uint64_t token;~Original(){if(token)ctr_video_depth_attachment_release_v1(context,token);}}original{api->context,token};
  if(ctr_video_depth_attachment_clear_v1(api->context,token,0x3f800000,7))return 3;
  CtrDepthMS2LayoutV1 layout{1,sizeof(layout),960,540,1024,1,3,1,0,10239,0,0,0,0,address};
  CtrDepthBackingSpansV1 spans{};if(ctr_video_depth_single_spans_v1(&layout,address+depth_bytes,&spans) || spans.depth_bytes!=depth_bytes || spans.stencil_bytes!=stencil_bytes)return 4;
  if(resident.retain(api,session,token,5851,layout,address+depth_bytes) || resident.covers(api->context,token,5851))return 5;
  auto changed_layout=layout;changed_layout.width=959;
  if(resident.retain(api,session,token,5851,changed_layout,address+depth_bytes)!=88 || resident.covers(api->context,token,5851))return 21;
  if(ctr_video_depth_attachment_release_v1(api->context,token))return 6;original.token=0;
  uint64_t lease=0;std::shared_ptr<void> owner;if(resident.resolve(api->context,spans,1,lease,owner) || !lease || !owner || lease==token)return 7;
  uint64_t attachment_lease=0;
  if(resident.resolve_attachment(api->context,layout,address+depth_bytes,attachment_lease) || attachment_lease!=lease)return 26;
  for(unsigned bad=0;bad<8;++bad){auto wrong=layout;uint64_t untouched=123;
   if(bad==0)--wrong.width;if(bad==1)--wrong.pitch;if(bad==2)wrong.samples=2;if(bad==3)wrong.tile_mode=1;
   if(bad==4)wrong.bank_swizzle=1;if(bad==5)wrong.slice_max=1;
   const int rejected=resident.resolve_attachment(bad==7?nullptr:api->context,wrong,address+depth_bytes+(bad==6?1:0),untouched);
   if(!rejected || untouched!=123)return 27;
  }
  std::shared_ptr<const std::vector<unsigned char>> prior;
  if(resident.prior_backing(api->context,lease,1,prior) || !prior || prior->size()!=protected_bytes || !std::all_of(prior->begin(),prior->end(),[](unsigned char b){return b==0x25;}))return 24;
  auto prior_unchanged=prior;if(resident.prior_backing(api->context,lease,2,prior_unchanged)!=88 || prior_unchanged!=prior)return 25;
  uint64_t sampled_lease=0;std::shared_ptr<void> sampled_owner;
  if(resident.resolve_sampled_interval(api->context,address,depth_bytes,1,sampled_lease,sampled_owner) || sampled_lease!=lease || !sampled_owner)return 19;
  for(unsigned bad=0;bad<4;++bad){uint64_t untouched=123;std::shared_ptr<void> absent;
   if(resident.resolve_sampled_interval(bad==3?nullptr:api->context,address+(bad==0?1:0),depth_bytes-(bad==1?1:0),bad==2?2:1,untouched,absent)!=88 || untouched!=123 || absent)return 20;
  }
  for(auto offset:{uint64_t(0),depth_bytes,protected_bytes-1})for(bool write:{false,true}){
   uint64_t fault=0,caller=0,operation=9;if(gpu_resident_access_probe(bytes+offset,write,&runtime_gpu_resident_memory,&fault,&caller,&operation)!=62 || fault!=address+offset || !caller || operation!=uint64_t(write))return 8;
  }
  if(bytes[protected_bytes]!=0x25 || bytes[protected_bytes+102399]!=0x25)return 9;
  uint64_t alias=0;const int aliased=ctr_video_depth_sampled_alias_v1(api->context,lease,&alias);std::cerr<<"sample1 retained sampled alias status="<<aliased<<" token="<<alias<<"\n";if(aliased || !alias || ctr_video_sampled_image_release_v1(api->context,alias) || ctr_video_depth_attachment_restore_v1(api->context,lease))return 10;
  if(ctr_video_depth_attachment_clear_v1(api->context,lease,0x3f800000,9))return 14;
  uint64_t generation=0,provider_lease=0;if(ctr_video_depth_single_residency_register_v1(api->context,lease,&layout,address+depth_bytes,&generation) || !generation)return 15;
  if(ctr_video_depth_residency_acquire_v1(api->context,&spans,generation,&provider_lease) || !provider_lease || provider_lease==lease)return 16;
  if(ctr_video_depth_attachment_clear_v1(api->context,provider_lease,0x3f800000,0)!=91)return 17;
  if(ctr_video_depth_residency_remove_v1(api->context,generation) || ctr_video_depth_attachment_clear_v1(api->context,provider_lease,0x3f800000,7) || ctr_video_depth_attachment_release_v1(api->context,provider_lease))return 18;

  auto partial=spans;--partial.stencil_bytes;uint64_t unchanged=123;std::shared_ptr<void> missing;
  if(resident.resolve(api->context,partial,1,unchanged,missing)!=88 || unchanged!=123 || missing || resident.resolve(api->context,spans,2,unchanged,missing)!=88)return 11;
  if(runtime_gpu_resident_memory.shutdown() || bytes[0]!=0x25 || bytes[depth_bytes]!=0x25)return 12;
  uint64_t stale_lease=123;if(resident.resolve_attachment(api->context,layout,address+depth_bytes,stale_lease)!=88 || stale_lease!=123)return 28;
  return 0;
 });session.reset();const int closed=runtime_video_close(1);if(result)return result;if(closed)return 13;
 std::cout<<"{\"scope\":\"sample1_depth_GPU_residency_control\",\"passed\":true,\"real_GPU_clear\":true,\"original_owner_released\":true,\"retained_GPU_clear_after_release\":true,\"retained_sampled_alias\":true,\"provider_generation_acquired\":true,\"resident_clear_rejected\":true,\"remove_restores_real_GPU_clear\":true,\"read_write_SEH_probes\":6,\"depth_bytes\":2621440,\"stencil_bytes\":552960,\"guard_bytes\":3174400,\"neighbor_padding_bytes\":102400,\"guest_publication\":false,\"synthetic_guest_memory\":true,\"R3\":false}\n";return 0;
}

}
