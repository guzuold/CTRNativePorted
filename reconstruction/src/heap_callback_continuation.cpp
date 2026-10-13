#define NOMINMAX
#include "ctr/heap_callback_continuation.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace ctr {namespace {
using Bytes=std::vector<std::byte>;
void need(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
std::span<const std::byte> slice(std::span<const std::byte> b,std::uint64_t off,std::uint64_t length){need(off<=b.size()&&length<=b.size()-off,"Original file range exceeds extent");return b.subspan(static_cast<std::size_t>(off),static_cast<std::size_t>(length));}
template<class T>T read(std::span<const std::byte> b,std::uint64_t off){T v{};auto s=slice(b,off,sizeof(T));std::memcpy(&v,s.data(),sizeof(T));return v;}
Bytes file(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary|std::ios::ate);need(static_cast<bool>(in),"Original file unavailable");const auto n=in.tellg();need(n>0&&n<=0x20000000,"Bounded original file size required");Bytes b(static_cast<std::size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));need(static_cast<bool>(in),"Original file truncated");return b;}
std::string hash(std::span<const std::byte> b){BCRYPT_ALG_HANDLE algorithm{};if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider unavailable");std::array<unsigned char,32> digest{};const auto status=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(b.data())),static_cast<ULONG>(b.size()),digest.data(),static_cast<ULONG>(digest.size()));BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)throw std::runtime_error("Original SHA256 failed");constexpr char alphabet[]="0123456789abcdef";std::string result;for(auto v:digest){result+=alphabet[v>>4];result+=alphabet[v&15];}return result;}
struct Program {std::uint32_t type;std::uint64_t offset,address,filesize,memsize;};
std::vector<Program> programs(std::span<const std::byte> image,std::uint64_t base){need(read<std::uint32_t>(image,base)==0x464c457f&&read<std::uint8_t>(image,base+4)==2&&read<std::uint8_t>(image,base+5)==1,"Original little-endian ELF64 required");const auto off=read<std::uint64_t>(image,base+32);const auto stride=read<std::uint16_t>(image,base+54),count=read<std::uint16_t>(image,base+56);need(stride==56&&count&&count<256,"Invalid original ELF program headers");std::vector<Program> result;for(std::uint64_t i=0;i<count;++i){const auto at=base+off+i*stride;slice(image,at,56);result.push_back({read<std::uint32_t>(image,at),read<std::uint64_t>(image,at+8),read<std::uint64_t>(image,at+16),read<std::uint64_t>(image,at+32),read<std::uint64_t>(image,at+40)});}return result;}
struct Segment {std::uint64_t flags,offset,size;};
std::span<const std::byte> self_range(std::span<const std::byte> image,const std::vector<Segment>& segments,const std::vector<Program>& ph,std::uint64_t off,std::uint64_t size){for(auto s:segments){if(!(s.flags&0x800))continue;const auto index=(s.flags>>20)&4095;need(index<ph.size(),"SELF segment header index invalid");const auto h=ph[static_cast<std::size_t>(index)];if(off>=h.offset&&off-h.offset<=h.filesize&&size<=h.filesize-(off-h.offset)){need(!(s.flags&10),"Encrypted/compressed SELF range unsupported");need(off-h.offset<=s.size&&size<=s.size-(off-h.offset),"Plain SELF segment truncated");return slice(image,s.offset+off-h.offset,size);}}throw std::invalid_argument("Original SELF range not plaintext-backed");}
std::uint64_t tag(std::span<const std::byte> dynamic,std::uint64_t key){need(dynamic.size()%16==0,"Dynamic table alignment invalid");for(std::size_t i=0;i<dynamic.size();i+=16)if(read<std::uint64_t>(dynamic,i)==key)return read<std::uint64_t>(dynamic,i+8);throw std::invalid_argument("Required SELF dynamic tag missing");}
Program find(const std::vector<Program>& ph,std::uint32_t type){for(auto h:ph)if(h.type==type)return h;throw std::invalid_argument("Original program header missing");}
}
HeapInitializerBinding HeapInitializerBinding::load(const std::filesystem::path& path){
 const auto s=file(path);need(hash(s)=="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816","Original libc source identity mismatch");
 need(read<std::uint32_t>(s,0)==0x1d3d154f,"Original SELF required");const auto n=read<std::uint16_t>(s,24);need(n>0&&n<256,"SELF count invalid");std::vector<Segment> segments;
 for(std::uint64_t i=0;i<n;++i){const auto at=32+i*32;segments.push_back({read<std::uint64_t>(s,at),read<std::uint64_t>(s,at+8),read<std::uint64_t>(s,at+16)});}
 const auto ph=programs(s,32+static_cast<std::uint64_t>(n)*32);const auto dh=find(ph,2),lh=find(ph,0x61000000);const auto dynamic=self_range(s,segments,ph,dh.offset,dh.filesize),blob=self_range(s,segments,ph,lh.offset,lh.filesize);
 const auto rows=slice(blob,tag(dynamic,0x6100002f),tag(dynamic,0x61000031));need(rows.size()%24==0,"RELA extent invalid");unsigned count=0;HeapInitializerBinding result;
 for(std::size_t i=0;i<rows.size();i+=24){if(read<std::uint64_t>(rows,i)!=0x11bde8)continue;++count;const auto info=read<std::uint64_t>(rows,i+8);need(static_cast<std::uint32_t>(info)==1&&read<std::uint64_t>(rows,i+16)==0,"Selected defined relocation differs");const auto symbol=tag(dynamic,0x61000039)+(info>>32)*24;
 need(read<std::uint16_t>(blob,symbol+6)==3&&read<std::uint64_t>(blob,symbol+8)==0xa96d0&&read<std::uint64_t>(blob,symbol+16)==536,"Defined initializer differs");constexpr char name[]="KNNNbyRieqQ#E#A";const auto text=slice(blob,tag(dynamic,0x61000035)+read<std::uint32_t>(blob,symbol),sizeof(name));need(!std::memcmp(text.data(),name,sizeof(name)),"Defined symbol name differs");
 bool backed=false;for(auto h:ph)if((h.type==1||h.type==0x61000010)&&0x11bde8>=h.address&&0x11bde8-h.address+8<=h.filesize){const auto cell=self_range(s,segments,ph,h.offset+0x11bde8-h.address,8);need(read<std::uint64_t>(cell,0)==0xa96d0,"Original GOT plaintext differs");backed=true;}need(backed,"Original GOT PT_LOAD missing");result.pointer_=0x50000000+read<std::uint64_t>(blob,symbol+8);
 }need(count==1,"Selected defined relocation missing/duplicated");return result;
}
HeapCallbackRegistration continue_game_heap_callbacks(const GameHeapApiContinuation& prior,const HeapInitializerBinding& binding,std::shared_ptr<const NativeProcParam> owner,std::span<std::byte> slots){
 need(binding.pointer()==0x500a96d0&&prior.boundary_registers[12]==0x02200148&&prior.boundary_registers[14]==0x2000ffc0&&prior.boundary_registers[15]==0x2000fff0,"Unsupported caller/module boundary");
 need(prior.installation.source&&read<std::uint64_t>(prior.installation.source->bytes(),232+0x10)==0x014377d0,"Custom initializer identity required");auto registers=prior.boundary_registers;registers[5]=registers[12];registers[14]-=8;
 auto result=register_game_heap_callbacks(std::move(owner),slots,registers);result.return_registers[1]=0;result.return_registers[0]=0x50058ef0;result.return_registers[5]=0x50125558;return result;
}
}

