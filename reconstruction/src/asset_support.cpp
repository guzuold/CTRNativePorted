#include "ctr/asset_support.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ctr::assets {
namespace {
template<class T>T read(std::span<const std::byte> s,std::size_t offset){
 if(offset>s.size()||sizeof(T)>s.size()-offset)throw std::invalid_argument("Asset field outside payload");
 T result;std::memcpy(&result,s.data()+offset,sizeof(T));return result;
}
float half(std::uint16_t h){
 auto sign=std::uint32_t(h&0x8000)<<16;auto exp=(h>>10)&31;auto mant=h&1023;
 if(exp==31)return std::bit_cast<float>(sign|0x7f800000u|(std::uint32_t(mant)<<13));
 if(exp==0){float value=std::ldexp(float(mant),-24);return sign?-value:value;}
 return std::bit_cast<float>(sign|(std::uint32_t(exp+112)<<23)|(std::uint32_t(mant)<<13));
}
}
Mesh decode_ctr_mesh(std::span<const std::byte> vd,std::span<const std::byte> id,
 std::uint32_t vc,std::uint32_t ic,std::uint32_t stride,std::uint32_t iw,std::array<float,3> t){
 if((stride!=40&&stride!=44)||iw!=2)throw std::invalid_argument("Only observed CTR stride40/44 and i16 layout covered");
 if(std::uint64_t(vc)*stride>vd.size()||std::uint64_t(ic)*iw>id.size()||ic%3)throw std::invalid_argument("Mesh counts exceed original payload or triangle list");
 Mesh mesh;mesh.vertices.reserve(vc);mesh.indices.reserve(ic);
 for(std::uint32_t i=0;i<vc;++i){
  std::size_t b=std::size_t(i)*stride;Vertex v{};
  for(unsigned k=0;k<3;++k){v.position[k]=read<float>(vd,b+k*4)+t[k];v.normal[k]=read<float>(vd,b+12+k*4);}
  const std::size_t uv= b+(stride==44?28:24);
  for(unsigned k=0;k<2;++k)v.uv[k]=half(read<std::uint16_t>(vd,uv+k*2));
  for(auto value:v.position)if(!std::isfinite(value))throw std::invalid_argument("Unresolved nonfinite vertex position layout");
  for(auto value:v.normal)if(!std::isfinite(value))throw std::invalid_argument("Unresolved nonfinite vertex normal layout");
  for(auto value:v.uv)if(!std::isfinite(value))throw std::invalid_argument("Unresolved nonfinite vertex UV layout");
  for(unsigned k=0;k<3;++k)v.color[k]=stride==44?float(std::to_integer<unsigned>(vd[b+24+k]))/255.0f:1.0f;
  mesh.vertices.push_back(v);
 }
 for(std::uint32_t i=0;i<ic;++i){auto index=read<std::uint16_t>(id,std::size_t(i)*2);if(index>=vc)throw std::invalid_argument("Original index outside vertices");mesh.indices.push_back(index);}
 return mesh;
}
std::vector<std::byte> unswizzle_ps4_blocks(std::span<const std::byte> data,
 std::uint32_t width,std::uint32_t height,std::uint32_t bpb){
 const bool small=width==height&&(width==4||width==8);
 if(!width||!height||(!small&&(width%32||height%32))||(bpb!=8&&bpb!=16))throw std::invalid_argument("Uncovered image tile layout");
 auto bx=std::size_t(small?32:width)/4,by=std::size_t(small?32:height)/4;
 if(bx>data.size()/bpb/by)throw std::invalid_argument("Mip0 exceeds original image payload");
 std::vector<std::byte> out(bx*by*bpb);auto tiles_x=bx/8;
 for(std::size_t tile=0;tile<tiles_x*(by/8);++tile){
  for(unsigned morton=0;morton<64;++morton){unsigned x=0,y=0;
   for(unsigned bit=0;bit<3;++bit){x|=((morton>>(2*bit))&1)<<bit;y|=((morton>>(2*bit+1))&1)<<bit;}
   auto dst=((tile/tiles_x*8+y)*bx+tile%tiles_x*8+x)*bpb;
   auto src=(tile*64+morton)*bpb;std::memcpy(out.data()+dst,data.data()+src,bpb);
  }
 }
 if(small){
  auto original_bx=std::size_t(width)/4,original_by=std::size_t(height)/4;
  std::vector<std::byte> trimmed(original_bx*original_by*bpb);
  for(std::size_t y=0;y<original_by;++y)std::memcpy(trimmed.data()+y*original_bx*bpb,out.data()+y*bx*bpb,original_bx*bpb);
  return trimmed;
 }
 return out;
}
}
