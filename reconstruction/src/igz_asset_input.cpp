#include "ctr/igz_asset_input.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr::assets {
namespace {
template<class T>T read(std::span<const std::byte> s,std::size_t n){
 if(n>s.size()||sizeof(T)>s.size()-n)throw std::invalid_argument("IGZ read outside original file");
 T value;std::memcpy(&value,s.data()+n,sizeof(T));return value;
}
std::span<const std::byte> slice(std::span<const std::byte>s,std::size_t n,std::size_t size){
 if(n>s.size()||size>s.size()-n)throw std::invalid_argument("IGZ extent outside original file");return s.subspan(n,size);
}
std::vector<std::uint32_t> decode(std::span<const std::byte> bytes,std::uint32_t count){
 if(count>bytes.size()*2)throw std::invalid_argument("Impossible IGZ compressed count");
 std::vector<std::uint32_t> result;result.reserve(count);std::uint64_t delta=0,previous=0;unsigned shift=0;
 if(!count)return result;
 for(auto byte:bytes){
  auto b=std::to_integer<unsigned>(byte);
  for(unsigned i=0;i<2;++i){auto nibble=(b>>(i*4))&15;
   if(shift>30)throw std::invalid_argument("Compressed IGZ offset overflow");
   delta|=std::uint64_t(nibble&7)<<shift;shift+=3;
   if(!(nibble&8)){previous+=delta*4;if(previous>std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("IGZ offset overflow");result.push_back(static_cast<std::uint32_t>(previous));
    if(result.size()==count)return result;delta=0;shift=0;}
  }
 }
 throw std::invalid_argument("Truncated compressed IGZ offsets");
}
}
IgzAssetInput::IgzAssetInput(std::span<const std::byte>file):file_(file){
 if(read<std::uint32_t>(file,0)!=0x49475a01||read<std::uint32_t>(file,4)!=10||read<std::uint32_t>(file,12)!=8)throw std::invalid_argument("Only observed CTR IGZ10/platform8 covered");
 auto declared=read<std::uint32_t>(file,16);
 for(std::size_t off=20;off+16<=0x224;off+=16){
  IgzChunk c{read<std::uint32_t>(file,off),read<std::uint32_t>(file,off+4),read<std::uint32_t>(file,off+8),read<std::uint32_t>(file,off+12)};
  if(!c.offset)break;(void)slice(file,c.offset,c.size);chunks_.push_back(c);
 }
 if(chunks_.size()<2)throw std::invalid_argument("IGZ fixup/object chunks missing");
 std::size_t offset=chunks_[0].offset, end=offset+chunks_[0].size;
 for(std::uint32_t i=0;i<declared;++i){
  if(offset>end||end-offset<16)throw std::invalid_argument("IGZ fixup table truncated");
  auto tag=slice(file,offset,4);std::string name(reinterpret_cast<const char*>(tag.data()),4);
  IgzFixup f{read<std::uint32_t>(file,offset+4),static_cast<std::uint32_t>(offset),read<std::uint32_t>(file,offset+8),read<std::uint32_t>(file,offset+12)};
  if(f.header<16||f.size<f.header||f.size>end-offset||!fixups_.emplace(name,f).second)throw std::invalid_argument("Invalid/duplicate IGZ fixup");
  offset+=f.size;
 }
 const auto&tmet=fixups_.at("TMET");auto text=slice(file,tmet.offset+tmet.header,tmet.size-tmet.header);std::size_t cursor=0;
 if(tmet.count>text.size())throw std::invalid_argument("Impossible IGZ type count");
 for(std::uint32_t i=0;i<tmet.count;++i){std::string name;
  while(cursor<text.size()&&text[cursor]!=std::byte{0})name.push_back(char(std::to_integer<unsigned char>(text[cursor++])));
  if(cursor==text.size()||name.empty())throw std::invalid_argument("Invalid IGZ type string");++cursor;
  if(cursor<text.size()&&text[cursor]==std::byte{0})++cursor;types_.push_back(name);
 }
 const auto&rvtb=fixups_.at("RVTB");auto entries=decode(slice(file,rvtb.offset+rvtb.header,rvtb.size-rvtb.header),rvtb.count);
 for(auto encoded:entries){auto global=global_offset(encoded);auto type=read<std::uint32_t>(file,global);if(type>=types_.size())throw std::invalid_argument("IGZ object type out of range");objects_.emplace_back(global,type);}
}
std::size_t IgzAssetInput::global_offset(std::uint32_t encoded)const{
 auto chunk=std::size_t(encoded>>27)+1,offset=std::size_t(encoded&0x7ffffff);
 if(chunk>=chunks_.size()||offset>=chunks_[chunk].size)throw std::invalid_argument("IGZ encoded pointer outside chunk");
 return chunks_[chunk].offset+offset;
}
std::span<const std::byte> IgzAssetInput::memory(std::size_t object,std::size_t field)const{
 auto size=read<std::uint32_t>(file_,object+field),flags=read<std::uint32_t>(file_,object+field+4),ptr=read<std::uint32_t>(file_,object+field+8);
 if(!size)return {};if(!(flags&0x1000000))throw std::invalid_argument("Inactive IGZ memory reference");
 return slice(file_,global_offset(ptr),size);
}
std::vector<std::byte> IgzAssetInput::image_mip0()const{
 std::size_t image=0;unsigned count=0;
 for(const auto&[offset,type]:objects_)if(types_[type]=="igImage2"){image=offset;++count;}
 if(count!=1)throw std::invalid_argument("Exactly one igImage2 required");
 auto index=read<std::uint32_t>(file_,image+40);const auto&f=fixups_.at("EXID");
 if(index>=f.count||std::uint64_t(f.count)*8>f.size-f.header)throw std::invalid_argument("IGZ image EXID missing");
 auto hash=read<std::uint32_t>(file_,f.offset+f.header+index*8);unsigned bpb;
 if(hash==4163974275u)bpb=8;else if(hash==4038686415u||hash==2097684074u)bpb=16;else throw std::invalid_argument("Uncovered original IGZ image format");
 return unswizzle_ps4_blocks(memory(image,56),read<std::uint16_t>(file_,image+24),read<std::uint16_t>(file_,image+26),bpb);
}
std::vector<SerializedReference> IgzAssetInput::external_references()const{
 std::vector<std::string> strings;
 if(auto it=fixups_.find("TSTR");it!=fixups_.end()){
  const auto&f=it->second;auto bytes=slice(file_,f.offset+f.header,f.size-f.header);std::size_t p=0;
  if(f.count>bytes.size())throw std::invalid_argument("Impossible IGZ external string count");
  for(std::uint32_t i=0;i<f.count;++i){
   auto begin=p;while(p<bytes.size()&&bytes[p]!=std::byte{0})++p;
   if(p==bytes.size())throw std::invalid_argument("External string unterminated");
   strings.emplace_back(reinterpret_cast<const char*>(bytes.data()+begin),p-begin);p=(p+2)&~std::size_t(1);
  }
 }
 std::vector<SerializedReference> handles,named_objects,hashed;
 if(auto it=fixups_.find("EXNM");it!=fixups_.end()){
  const auto&f=it->second;
  if(std::uint64_t(f.count)*8>f.size-f.header)throw std::invalid_argument("EXNM table truncated");
  for(std::uint32_t i=0;i<f.count;++i){
   auto p=std::size_t(f.offset)+f.header+std::size_t(i)*8;
   auto name=read<std::uint32_t>(file_,p),space=read<std::uint32_t>(file_,p+4);
   if((name&0x7fffffffu)>=strings.size()||(space&0x7fffffffu)>=strings.size())throw std::invalid_argument("EXNM string index out of range");
   SerializedReference entry{};entry.name=strings[name&0x7fffffffu];entry.name_space=strings[space&0x7fffffffu];
   (space&0x80000000u?handles:named_objects).push_back(entry);
  }
 }
 if(auto it=fixups_.find("EXID");it!=fixups_.end()){
  const auto&f=it->second;
  if(std::uint64_t(f.count)*8>f.size-f.header)throw std::invalid_argument("EXID table truncated");
  for(std::uint32_t i=0;i<f.count;++i){
   auto p=std::size_t(f.offset)+f.header+std::size_t(i)*8;
   SerializedReference entry{};entry.name_hash=read<std::uint32_t>(file_,p);entry.namespace_hash=read<std::uint32_t>(file_,p+4);hashed.push_back(entry);
  }
 }
 std::vector<SerializedReference> result;
 for(const auto*tag:{"RHND","RNEX","REXT"}){
  auto it=fixups_.find(tag);if(it==fixups_.end())continue;const auto&f=it->second;
  for(auto encoded:decode(slice(file_,f.offset+f.header,f.size-f.header),f.count)){
   auto offset=global_offset(encoded);auto raw=read<std::uint32_t>(file_,offset);
   std::uint32_t index;SerializedReferenceKind kind;const std::vector<SerializedReference>*table;
   if(std::string_view(tag)=="RHND"){
    index=raw&0x3fffffffu;
    if(raw&0x80000000u){table=&handles;kind=SerializedReferenceKind::named_handle;}
    else{table=&hashed;kind=SerializedReferenceKind::hashed_handle;}
   }else if(std::string_view(tag)=="RNEX"){index=raw&0x7fffffffu;table=&named_objects;kind=SerializedReferenceKind::named_object;}
   else{index=raw&0x7fffffffu;table=&hashed;kind=SerializedReferenceKind::hashed_object;}
   if(index>=table->size())throw std::invalid_argument("Serialized external reference index out of range");
   auto entry=table->at(index);entry.tag=tag;entry.kind=kind;entry.file_offset=offset;entry.raw=raw;entry.index=index;result.push_back(entry);
  }
 }
 return result;
}
std::vector<SerializedReference> IgzAssetInput::graphics_texture_references()const{
 auto refs=external_references();std::vector<SerializedReference> textures;
 for(const auto&[offset,type]:objects_)if(types_[type]=="igGraphicsTexture"){
  bool found=false;
  for(const auto&r:refs)if(r.file_offset==offset+40&&r.tag=="RHND"){
   textures.push_back(r);found=true;break;
  }
  if(!found)throw std::invalid_argument("Graphics texture serialized image handle missing");
 }
 return textures;
}
std::vector<SerializedMaterialGraph> IgzAssetInput::material_graphs()const{
 const auto&root_fixup=fixups_.at("ROOT");
 if(root_fixup.count!=1)throw std::invalid_argument("Exactly one material root required");
 auto root_bytes=slice(file_,root_fixup.offset+root_fixup.header,root_fixup.size-root_fixup.header);
 auto root_encoded=read<std::uint32_t>(root_bytes,0); // ROOT is a plain integer fixup.
 auto type_at=[&](std::size_t p)->std::string{
  for(const auto&[offset,type]:objects_)if(offset==p)return types_[type];
  throw std::invalid_argument("Material graph pointer absent from object table");
 };
 const auto&f=fixups_.at("ROFS");std::vector<std::size_t> fields;
 for(auto encoded:decode(slice(file_,f.offset+f.header,f.size-f.header),f.count))fields.push_back(global_offset(encoded));
 auto pointer=[&](std::size_t p){
  if(std::find(fields.begin(),fields.end(),p)==fields.end())throw std::invalid_argument("Material graph pointer lacks ROFS");
  if(read<std::uint32_t>(file_,p+4))throw std::invalid_argument("Uncovered material graph pointer high word");
  return global_offset(read<std::uint32_t>(file_,p));
 };
 auto root=global_offset(root_encoded);
 if(type_at(root)!="igObjectList")throw std::invalid_argument("Material root is not object list");
 auto root_count=read<std::uint32_t>(file_,root+16);auto roots=memory(root,24);
 if(roots.size()%8||root_count>roots.size()/8)throw std::invalid_argument("Material root list extent");
 auto refs=external_references();std::vector<SerializedMaterialGraph> graphs;
 auto root_start=roots.empty()?0:std::size_t(roots.data()-file_.data());
 for(std::uint32_t i=0;i<root_count;++i){
  auto material=pointer(root_start+std::size_t(i)*8);auto type=type_at(material);
  if(type=="igTransformSource3")continue; // Nonmaterial root: no transform behavior claimed.
  if(type!="igGraphicsMaterial")throw std::invalid_argument("Uncovered material root class");
  auto set=pointer(material+88);
  if(type_at(set)!="igGraphicsObjectSet")throw std::invalid_argument("Material graphics set type mismatch");
  auto count=read<std::uint32_t>(file_,set+16);auto data=memory(set,24);
  if(data.size()%8||count>data.size()/8)throw std::invalid_argument("Material graphics vector extent");
  SerializedMaterialGraph graph{material,set,{}, {}};
  auto start=data.empty()?0:std::size_t(data.data()-file_.data());
  for(std::uint32_t j=0;j<count;++j){
   auto object=pointer(start+std::size_t(j)*8);auto object_type=type_at(object);
   graph.ordered_objects.push_back({object,object_type});
   if(object_type=="igGraphicsTexture"){
    bool found=false;
    for(const auto&r:refs)if(r.tag=="RHND"&&r.file_offset==object+40){graph.ordered_texture_references.push_back(r);found=true;break;}
    if(!found)throw std::invalid_argument("Material graph image handle missing");
   }
  }
  graphs.push_back(std::move(graph));
 }
 return graphs;
}
std::vector<SerializedShaderBinding> IgzAssetInput::shader_bindings()const{
 std::vector<std::size_t> lists;
 for(const auto&g:material_graphs())for(const auto&o:g.ordered_objects)
  if(o.type=="igShaderConstantValueList"&&std::find(lists.begin(),lists.end(),o.object_offset)==lists.end())lists.push_back(o.object_offset);
 std::sort(lists.begin(),lists.end());
 auto fields=[&](const char*tag){const auto&f=fixups_.at(tag);std::vector<std::size_t> result;
  for(auto v:decode(slice(file_,f.offset+f.header,f.size-f.header),f.count))result.push_back(global_offset(v));return result;};
 auto rofs=fields("ROFS"),rstt=fields("RSTT");
 auto type_at=[&](std::size_t p){for(const auto&[offset,type]:objects_)if(offset==p)return types_[type];throw std::invalid_argument("Shader pointer absent from object table");};
 auto pointer=[&](std::size_t p){if(std::find(rofs.begin(),rofs.end(),p)==rofs.end()||read<std::uint32_t>(file_,p+4))throw std::invalid_argument("Shader pointer metadata missing");return global_offset(read<std::uint32_t>(file_,p));};
 const auto&t=fixups_.at("TSTR");auto text=slice(file_,t.offset+t.header,t.size-t.header);std::vector<std::string> strings;std::size_t p=0;
 if(t.count>text.size())throw std::invalid_argument("Shader string count extent");
 for(std::uint32_t i=0;i<t.count;++i){auto begin=p;while(p<text.size()&&text[p]!=std::byte{0})++p;if(p==text.size())throw std::invalid_argument("Shader string unterminated");strings.emplace_back(reinterpret_cast<const char*>(text.data()+begin),p-begin);p=(p+2)&~std::size_t(1);}
 std::vector<SerializedShaderBinding> result;
 for(auto list:lists){auto count=read<std::uint32_t>(file_,list+24);auto data=memory(list,32);
  if(data.size()%8||count>data.size()/8)throw std::invalid_argument("Shader value vector extent");
  auto start=data.empty()?0:std::size_t(data.data()-file_.data());
  for(std::uint32_t i=0;i<count;++i){auto value=pointer(start+std::size_t(i)*8);auto type=type_at(value);std::size_t offset,size;
   if(type=="igShaderConstantValueBool"){offset=24;size=1;}
   else if(type=="igShaderConstantValueFloat"){offset=24;size=4;}
   else if(type=="igShaderConstantValueVector"){offset=32;size=16;}
   else throw std::invalid_argument("Uncovered shader value type");
   auto constant=pointer(value+16);if(type_at(constant)!="igGraphicsShaderConstant")throw std::invalid_argument("Shader constant type mismatch");
   if(std::find(rstt.begin(),rstt.end(),constant+16)==rstt.end()||read<std::uint32_t>(file_,constant+20))throw std::invalid_argument("Shader constant name metadata missing");
   auto index=read<std::uint32_t>(file_,constant+16);if(index>=strings.size())throw std::invalid_argument("Shader constant name index extent");
   auto bytes=slice(file_,value+offset,size);
   result.push_back({list,value,constant,type,strings[index],{bytes.begin(),bytes.end()}});
  }
 }
 return result;
}
std::vector<Mesh> IgzAssetInput::model_draw_calls()const{
 std::size_t model=0;unsigned count=0;
 for(const auto&[offset,type]:objects_)if(types_[type]=="igModelData"){model=offset;++count;}
 if(count!=1)throw std::invalid_argument("Exactly one original igModelData required");
 auto object=[&](std::uint32_t encoded,const char*expected){
  auto offset=global_offset(encoded);
  for(const auto&[address,type]:objects_)if(address==offset){if(types_[type]!=expected)throw std::invalid_argument("Unexpected IGZ object type");return offset;}
  throw std::invalid_argument("IGZ pointer absent from original object table");
 };
 auto pointer=[&](std::size_t address,std::size_t field,const char*type){return object(read<std::uint32_t>(file_,address+field),type);};
 auto transforms=memory(model,72),hierarchy=memory(model,96),drawcalls=memory(model,120),transform_indices=memory(model,144);
 if(drawcalls.size()%8||transforms.size()%8||hierarchy.size()%4||transform_indices.size()%4||transform_indices.size()/4<drawcalls.size()/8)throw std::invalid_argument("Invalid IGZ model vector extent");
 std::vector<Mesh> result;
 for(std::size_t i=0;i<drawcalls.size()/8;++i){
  if(!fixups_.contains("EXID"))throw std::invalid_argument("Original index EXID format reference missing");
  const auto&exid=fixups_.at("EXID");
  auto d=object(read<std::uint32_t>(drawcalls,i*8),"igModelDrawCallData");
  auto gv=pointer(d,72,"igGraphicsVertexBuffer"),gi=pointer(d,80,"igGraphicsIndexBuffer");
  auto vb=pointer(gv,32,"igVertexBuffer"),ib=pointer(gi,32,"igIndexBuffer"),fmt=pointer(vb,48,"igVertexFormat");
  auto format_index=read<std::uint32_t>(file_,ib+48);
  if(format_index>=exid.count||std::uint64_t(exid.count)*8>exid.size-exid.header||read<std::uint32_t>(file_,exid.offset+exid.header+format_index*8)!=862336675u)throw std::invalid_argument("Only observed original i16_ps4 indices covered");
  std::array<float,3> translation{};auto index=std::int64_t(read<std::int32_t>(transform_indices,i*4))-1;
  if(index>=0&&std::uint64_t(index)<hierarchy.size()/4){
   auto parent=read<std::int32_t>(hierarchy,static_cast<std::size_t>(index)*4);if(parent>0)index=std::int64_t(parent)-1;
   if(index>=0&&std::uint64_t(index)<transforms.size()/8){
    auto t=object(read<std::uint32_t>(transforms,static_cast<std::size_t>(index)*8),"igAnimatedTransform");
    for(unsigned k=0;k<3;++k)translation[k]=read<float>(file_,t+80+k*4);
   }
  }
  result.push_back(decode_ctr_described_mesh(memory(vb,32),memory(ib,32),memory(fmt,16),read<std::uint32_t>(file_,vb+12),read<std::uint32_t>(file_,ib+12),read<std::uint32_t>(file_,fmt+12),translation));
 }
 return result;
}
}
