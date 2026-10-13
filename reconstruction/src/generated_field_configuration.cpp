#include "ctr/generated_field_configuration.h"
#include "ctr/inline_field_default.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void configure_generated_metaobject_fields(ColdFieldTable& t,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 std::uint64_t metadata,std::uint64_t token,std::span<const std::byte> canonical,
 std::uint64_t element,std::uint64_t attribute,std::uint64_t base){
 if(!metadata||!token||!element||!attribute||!base||canonical.size()!=80||canonical[67]!=std::byte{1})throw std::invalid_argument("Prepared cached metadata/canonical matching ownership required");
 std::uint64_t identity;std::uint32_t refs;std::memcpy(&identity,canonical.data(),8);std::memcpy(&refs,canonical.data()+8,4);
 if(identity!=0x025bba10||!(refs&0x7ffffu)||(refs&0x7ffffu)==0x7ffffu)throw std::invalid_argument("Canonical ObjectRef lifetime required; clone/lookup missing");
 for(auto& f:t.fields){std::uint64_t h,p;std::memcpy(&h,f.payload.data()+48,8);std::memcpy(&p,f.payload.data()+56,8);if(h||p)throw std::invalid_argument("Fresh generated field configuration required");}
 auto flags=[&](unsigned i,std::uint32_t mask,std::uint32_t bits=0){std::uint32_t v;std::memcpy(&v,t.fields[i].payload.data()+32,4);v=(v&mask)|bits;std::memcpy(t.fields[i].payload.data()+32,&v,4);};
 auto put=[&](unsigned i,unsigned offset,std::uint64_t v){std::memcpy(t.fields[i].payload.data()+offset,&v,8);};
 auto scalar=[&](unsigned i,std::uint32_t v,unsigned width){flags(i,0xfffbffff);initialize_fresh_scalar_field_default(t.fields[i].payload,manager,pool,std::span<const std::byte>(reinterpret_cast<const std::byte*>(&v),width),true);};
 scalar(0,0xffffffffu,4);
 //+290 original015534e0 is a real ret. +298 cached matching ownership stores weak canonical binding.
 t.fields[1].payload[67]=std::byte{1};put(1,96,element);put(1,72,token);
 flags(2,0xfffbffff);flags(3,0xfffbffff);
 put(4,72,metadata);t.fields[4].payload[67]=std::byte{0};flags(4,0xffffffc0,0x15);
 for(auto i:{5u,6u}){put(i,72,metadata);t.fields[i].payload[67]=std::byte{0};flags(i,0xfffbffc0,0x15);}
 scalar(7,0xffff,2);scalar(8,0xffff,2);flags(9,0xfffbffff);scalar(10,4,2);flags(11,0xfffbffff);scalar(12,4,2);
 t.fields[13].payload[67]=std::byte{1};put(13,96,attribute);put(13,72,token);
 put(14,72,base);put(15,72,base);t.fields[15].payload[67]=std::byte{0};flags(15,0xfffbffff);
}
}
