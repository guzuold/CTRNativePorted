// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_allocate_explicit.h"
#include <cstring>
#include "ctr/core/allocation_context.h"
#include <stdexcept>
namespace ctr {std::uintptr_t metadata_allocate_explicit(std::span<std::byte> m,std::uintptr_t id,std::uint64_t size,std::uintptr_t pool,std::uintptr_t e0,std::uintptr_t v130,const PoolFreshOriginal& provider,std::uintptr_t manager,std::int32_t selector,std::int32_t tls_key){
 if(!pool){
 if(manager<id||manager-id>m.size()||m.size()-(manager-id)<0x6d)throw std::runtime_error("Missing bounded implicit manager");
 auto mo=static_cast<std::size_t>(manager-id);
 if(m[mo+0x6c]!=std::byte{}){
 auto read=[&](std::uintptr_t a,auto& v){if(a<id||a-id>m.size()||sizeof(v)>m.size()-(a-id))throw std::runtime_error("Missing bounded context hierarchy");std::memcpy(&v,m.data()+a-id,sizeof(v));};
 auto context=allocation_context(m.subspan(mo),tls_key);std::int32_t depth;read(context+0x18,depth);if(depth<0||depth>16||selector<0)throw std::runtime_error("Missing context failure/domain");
 std::uintptr_t frames,table;read(context+0x10,frames);read(frames+0x20,table);
 for(int pass=0;pass<2&&!pool;++pass){auto key=pass?6:selector;for(auto index=depth;index>=0;--index){std::uintptr_t frame,descriptor;read(table+static_cast<std::uintptr_t>(index)*8,frame);read(frame+0x18,descriptor);std::int32_t count;read(descriptor+0xc,count);if(key<count){std::uintptr_t slots;read(descriptor+0x20,slots);read(slots+static_cast<std::uintptr_t>(key)*8,pool);if(pool)break;}}if(selector==6)break;}
 if(!pool)throw std::runtime_error("Missing original NULL pool failure");
 }else{
 auto slot=selector==1?0x10:selector==2?0x20:selector==4?0x28:0x18;std::memcpy(&pool,m.data()+mo+slot,8);
 }
 }
 if(pool<id||pool-id>m.size()||m.size()-(pool-id)<0x34)throw std::runtime_error("Missing bounded explicit pool");
 auto offset=static_cast<std::size_t>(pool-id);std::uint32_t alignment;std::memcpy(&alignment,m.data()+offset+0x30,4);
 return allocation_dispatch_fresh(m,id,offset,0,size,alignment,1,0,e0,v130,provider);
}}
