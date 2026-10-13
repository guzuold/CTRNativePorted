#include "ctr/asset_support.h"
#include <cstring>
#include <stdexcept>
namespace ctr::assets {
namespace {
template<class T>T read(std::span<const std::byte> s,std::size_t p){
 if(p>s.size()||sizeof(T)>s.size()-p)throw std::invalid_argument("Vertex descriptor truncated");
 T v;std::memcpy(&v,s.data()+p,sizeof(T));return v;
}
}
Mesh decode_ctr_described_mesh(std::span<const std::byte> vd,std::span<const std::byte> id,
 std::span<const std::byte> elements,std::uint32_t vc,std::uint32_t ic,std::uint32_t stride,std::array<float,3> t){
 if(elements.empty()||elements.size()%12||!(stride==40||stride==44||stride==48||stride==52||stride==56))
  throw std::invalid_argument("Uncovered CTR vertex descriptor extent/stride");
 int pos=-1,normal=-1,uv=-1,color=-1;
 for(std::size_t p=0;p<elements.size();p+=12){
  auto format=read<std::uint8_t>(elements,p),usage=read<std::uint8_t>(elements,p+4),layer=read<std::uint8_t>(elements,p+5);
  auto offset=read<std::uint16_t>(elements,p+8);
  if(read<std::uint8_t>(elements,p+1)||read<std::uint8_t>(elements,p+2)||read<std::uint8_t>(elements,p+3)||
     read<std::uint16_t>(elements,p+6)||read<std::uint16_t>(elements,p+10))
   throw std::invalid_argument("Uncovered vertex stream/packing/frequency");
  if(format==44){if(usage||layer||offset)throw std::invalid_argument("Uncovered terminal vertex element");continue;}
  unsigned width=format==2?12u:format==42?4u:format==43?8u:format==4?4u:0u;
  if(!width||offset>stride||width>stride-offset)throw std::invalid_argument("Vertex element outside stride or uncovered format");
  int* target=nullptr;
  if(usage==0&&layer==0&&format==2)target=&pos;
  else if(usage==1&&layer==0&&format==2)target=&normal;
  else if(usage==5&&layer==0&&format==42)target=&uv;
  else if(usage==4&&layer==0&&format==4)target=&color;
  else if(!((usage==2&&layer==0&&format==2)||(usage==5&&layer==1&&(format==43||format==2))))
   throw std::invalid_argument("Uncovered vertex element semantics");
  if(target){if(*target>=0)throw std::invalid_argument("Duplicate primary vertex semantic");*target=offset;}
 }
 if(pos<0||normal<0||uv<0)throw std::invalid_argument("Required position/normal/primary UV descriptor missing");
 if(std::uint64_t(vc)*stride>vd.size())throw std::invalid_argument("Described vertex count exceeds payload");
 // Gather observed semantics into the existing bounded decoder layout.
 const std::uint32_t packed_stride=color>=0?44u:40u;
 std::vector<std::byte> packed(std::size_t(vc)*packed_stride);
 for(std::uint32_t i=0;i<vc;++i){
  auto base=std::size_t(i)*stride,dest=std::size_t(i)*packed_stride;
  std::memcpy(packed.data()+dest,vd.data()+base+pos,12);
  std::memcpy(packed.data()+dest+12,vd.data()+base+normal,12);
  std::memcpy(packed.data()+dest+(color>=0?28:24),vd.data()+base+uv,4);
  if(color>=0)std::memcpy(packed.data()+dest+24,vd.data()+base+color,4);
 }
 return decode_ctr_mesh(packed,id,vc,ic,packed_stride,2,t);
}
}
