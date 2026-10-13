#define NOMINMAX
#include "ctr/native_game_dso.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace ctr { namespace {
using Bytes=std::vector<std::byte>;
void need(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
std::span<const std::byte> slice(std::span<const std::byte> b,std::uint64_t off,std::uint64_t length){need(off<=b.size()&&length<=b.size()-off,"Original file range exceeds extent");return b.subspan(static_cast<std::size_t>(off),static_cast<std::size_t>(length));}
template<class T>T read(std::span<const std::byte> b,std::uint64_t off){T v{};auto s=slice(b,off,sizeof(T));std::memcpy(&v,s.data(),sizeof(T));return v;}
Bytes file(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary|std::ios::ate);need(static_cast<bool>(in),"Original file unavailable");const auto n=in.tellg();need(n>0&&n<=0x20000000,"Bounded original file size required");Bytes b(static_cast<std::size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));need(static_cast<bool>(in),"Original file truncated");return b;}
std::string hash(std::span<const std::byte> b){BCRYPT_ALG_HANDLE algorithm{};if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider unavailable");std::array<unsigned char,32> digest{};const auto status=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(b.data())),static_cast<ULONG>(b.size()),digest.data(),static_cast<ULONG>(digest.size()));BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)throw std::runtime_error("Original SHA256 failed");constexpr char alphabet[]="0123456789abcdef";std::string result;for(auto v:digest){result+=alphabet[v>>4];result+=alphabet[v&15];}return result;}
struct Program {std::uint32_t type,flags;std::uint64_t offset,address,filesize,memsize;};
std::vector<Program> programs(std::span<const std::byte> image,std::uint64_t base){need(read<std::uint32_t>(image,base)==0x464c457f&&read<std::uint8_t>(image,base+4)==2&&read<std::uint8_t>(image,base+5)==1,"Original little-endian ELF64 required");const auto off=read<std::uint64_t>(image,base+32);const auto stride=read<std::uint16_t>(image,base+54),count=read<std::uint16_t>(image,base+56);need(stride==56&&count&&count<256,"Invalid original ELF program headers");std::vector<Program> result;for(std::uint64_t i=0;i<count;++i){const auto at=base+off+i*stride;slice(image,at,56);result.push_back({read<std::uint32_t>(image,at),read<std::uint32_t>(image,at+4),read<std::uint64_t>(image,at+8),read<std::uint64_t>(image,at+16),read<std::uint64_t>(image,at+32),read<std::uint64_t>(image,at+40)});}return result;}
struct Segment {std::uint64_t flags,offset,size;};
std::span<const std::byte> self_range(std::span<const std::byte> image,const std::vector<Segment>& segments,const std::vector<Program>& ph,std::uint64_t off,std::uint64_t size){for(auto s:segments){if(!(s.flags&0x800))continue;const auto index=(s.flags>>20)&4095;need(index<ph.size(),"SELF segment header index invalid");const auto h=ph[static_cast<std::size_t>(index)];if(off>=h.offset&&off-h.offset<=h.filesize&&size<=h.filesize-(off-h.offset)){need(!(s.flags&10),"Encrypted/compressed SELF range unsupported");need(off-h.offset<=s.size&&size<=s.size-(off-h.offset),"Plain SELF segment truncated");return slice(image,s.offset+off-h.offset,size);}}throw std::invalid_argument("Original SELF range not plaintext-backed");}
std::uint64_t tag(std::span<const std::byte> dynamic,std::uint64_t key){need(dynamic.size()%16==0,"Dynamic table alignment invalid");for(std::size_t i=0;i<dynamic.size();i+=16)if(read<std::uint64_t>(dynamic,i)==key)return read<std::uint64_t>(dynamic,i+8);throw std::invalid_argument("Required SELF dynamic tag missing");}
Program find(const std::vector<Program>& ph,std::uint32_t type){for(auto h:ph)if(h.type==type)return h;throw std::invalid_argument("Original program header missing");}

unsigned audit_rows(std::span<const std::byte> rows){need(rows.size()%24==0,"RELA table extent invalid");unsigned count=0;for(std::size_t i=0;i<rows.size();i+=24){const auto at=read<std::uint64_t>(rows,i);if(at>=0x2600089&&at<0x2600098){need(at==0x2600090&&read<std::uint64_t>(rows,i+8)==8&&read<std::uint64_t>(rows,i+16)==0x2600090,"Selected DSO relocation differs");++count;}}return count;}
}
NativeGameDso::~NativeGameDso(){if(page_)VirtualFree(page_,0,MEM_RELEASE);}
std::shared_ptr<NativeGameDso> NativeGameDso::load(const std::filesystem::path& path){
 const auto s=file(path);need(hash(s)=="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1","Original main SELF identity mismatch");
 need(read<std::uint32_t>(s,0)==0x1d3d154f,"Original SELF required");const auto n=read<std::uint16_t>(s,24);need(n>0&&n<256,"Invalid SELF segment count");std::vector<Segment> segments;
 for(std::uint64_t i=0;i<n;++i){const auto at=32+i*32;segments.push_back({read<std::uint64_t>(s,at),read<std::uint64_t>(s,at+8),read<std::uint64_t>(s,at+16)});}
 const auto ph=programs(s,32+static_cast<std::uint64_t>(n)*32);unsigned matches=0;
 std::array<std::byte,8> raw{};
 for(auto h:ph)if(h.type==1&&h.address==0x2600000){++matches;need(h.flags==6&&h.offset==0x25e8000&&h.filesize==0x15f90&&h.memsize==0x1ea26d,"Original selected PT_LOAD differs");
 need(h.filesize<=h.memsize&&logical_base>=h.address&&logical_base-h.address+8<=h.filesize,"Selected DSO is not file-backed");
 const auto bytes=self_range(s,segments,ph,h.offset+logical_base-h.address,8);std::memcpy(raw.data(),bytes.data(),8);need(read<std::uint64_t>(raw,0)==logical_base,"Original selected DSO bytes differ");}
 need(matches==1,"Selected PT_LOAD ambiguous or absent");const auto dh=find(ph,2),lh=find(ph,0x61000000);const auto dynamic=self_range(s,segments,ph,dh.offset,dh.filesize),dynlib=self_range(s,segments,ph,lh.offset,lh.filesize);
 const auto relative=audit_rows(slice(dynlib,tag(dynamic,0x6100002f),tag(dynamic,0x61000031)));
 const auto plt=audit_rows(slice(dynlib,tag(dynamic,0x61000029),tag(dynamic,0x6100002d)));need(relative==1&&plt==0,"Selected DSO relocation count differs");
 auto owner=std::shared_ptr<NativeGameDso>(new NativeGameDso);
 owner->page_=VirtualAlloc(nullptr,owner_extent,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!owner->page_)throw std::runtime_error("Owned embedded BSS allocation failed");
 // Eight selected source bytes only; guards are outside the selected extent, not outside PT_LOAD.
 std::memcpy(owner->page_,raw.data(),8);
 const auto actual=reinterpret_cast<std::uintptr_t>(owner->page_);std::memcpy(owner->page_,&actual,8);
 std::memset(static_cast<std::byte*>(owner->page_)+source_extent,0xa5,owner_extent-source_extent);
 return owner;
}
std::span<std::byte> NativeGameDso::page() const{return {static_cast<std::byte*>(page_),owner_extent};}
std::span<std::byte> NativeGameDso::translate(std::uint64_t logical,std::size_t extent) const {need(logical>=logical_base&&logical-logical_base<=source_extent&&extent<=source_extent-(logical-logical_base),"Selected logical translation outside owned page");return page().subspan(static_cast<std::size_t>(logical-logical_base),extent);}
}
