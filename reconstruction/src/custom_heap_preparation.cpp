#define NOMINMAX
#include "ctr/custom_heap_preparation.h"
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

}
CustomHeapPreparation::CustomHeapPreparation(std::span<const std::byte> bytes){need(bytes.size()==4096,"Embedded module page extent required");page_=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!page_)throw std::runtime_error("Embedded page allocation failed");std::memcpy(page_,bytes.data(),4096);}
CustomHeapPreparation::~CustomHeapPreparation(){if(page_)VirtualFree(page_,0,MEM_RELEASE);}
std::span<const std::byte> CustomHeapPreparation::page()const{return {static_cast<const std::byte*>(page_),4096};}
HeapCallbackRegistration CustomHeapPreparation::prepare(const HeapCallbackRegistration& prior,std::span<std::byte> stack,std::uint64_t cookie,const std::filesystem::path& original){
 need(stack.size()==256&&prior.parameters&&prior.return_registers[5]==0x50125558&&prior.return_registers[14]==0x2000ffc0&&prior.return_registers[15]==0x2000fff0,"Unsupported preparation caller frame");const auto selection=read<std::uint64_t>(page(),0xa58);need(selection==0||selection==0x50125558,"Different selected embedded heap unsupported");
 const auto root=prior.parameters->section(prior.parameters->query_logical_root());need(read<std::uint64_t>(root,0)==88&&read<std::uint64_t>(root,0x38)==0x02200000,"Actual ProcParam query graph differs");const auto source=prior.parameters->section(read<std::uint64_t>(root,0x38));need(read<std::uint64_t>(source,0)==144&&read<std::uint32_t>(source,8)==12&&read<std::uint64_t>(source,0x70)==0,"Genuine version12/zero optional preparation required");
 const auto image=file(original);need(hash(image)=="1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816","Original constant identity differs");const auto n=read<std::uint16_t>(image,24);need(n&&n<256,"SELF count invalid");std::vector<Segment> segments;for(std::uint64_t i=0;i<n;++i){const auto a=32+i*32;segments.push_back({read<std::uint64_t>(image,a),read<std::uint64_t>(image,a+8),read<std::uint64_t>(image,a+16)});}const auto ph=programs(image,32+static_cast<std::uint64_t>(n)*32);std::array<std::byte,32> constant{};bool found=false;
 for(auto h:ph)if((h.type==1||h.type==0x61000010)&&0xd6860>=h.address&&0xd6860-h.address+32<=h.filesize){auto data=self_range(image,segments,ph,h.offset+0xd6860-h.address,32);std::memcpy(constant.data(),data.data(),32);found=true;}need(found,"Original preparation constant unavailable");
 const auto st=reinterpret_cast<std::uintptr_t>(stack.data()),base=reinterpret_cast<std::uintptr_t>(page_);need(st>base+4096||st+stack.size()<base,"Stack/page alias unsupported");
 auto result=prior;auto& r=result.return_registers;const std::uint64_t selected=0x50125558,ret=0x50058d68,queryret=0x5005eb0f,oldrbp=r[15],oldr14=r[12],oldrbx=r[1];
 if(!selection)std::memcpy(static_cast<std::byte*>(page_)+0xa58,&selected,8);
 std::memcpy(stack.data()+0xb8,&ret,8);std::memcpy(stack.data()+0xb0,&oldrbp,8);std::memcpy(stack.data()+0xa8,&oldr14,8);std::memcpy(stack.data()+0xa0,&oldrbx,8);std::memcpy(stack.data()+0x98,&cookie,8);std::memcpy(stack.data()+0x58,&queryret,8);std::memcpy(stack.data()+0x60,constant.data(),32);
 r[0]=0;r[5]=0x2000ff60;r[12]=0x40008000;r[14]=0x2000ff60;r[15]=0x2000ffb0;return result;
}
}
