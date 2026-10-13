// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/pm4_decode.hpp"
#include <utility>
namespace ctrport {
// Lossless owned command cursor. Completion means the handler has completed the
// whole packet, including its effects. Pending/unsupported never advances it.
// This layer assigns no semantics to NOPs, barriers, DMA or GPU operations.
class GnmCommandCursor {
 std::vector<uint32_t> words_;
 std::vector<CtrPm4::Packet> packets_;
 size_t next_=0;uint64_t submission_=0;uint32_t buffer_=0,kind_=0;
 bool captured_=false,running_=false;
public:
 GnmCommandCursor()=default;
 GnmCommandCursor(const GnmCommandCursor&)=delete;
 GnmCommandCursor& operator=(const GnmCommandCursor&)=delete;
 int matches(uint64_t submission,uint32_t buffer,uint32_t kind,uint32_t word,const uint32_t* data,size_t count)const{
  if(!captured_ || submission!=submission_ || buffer!=buffer_ || kind!=kind_ || !data)return 60;
  for(const auto& p:packets_)if(p.word==word){
   if(p.words!=count)return 60;
   return std::equal(words_.begin()+word,words_.begin()+word+count,data)?0:112;
  }
  return 60;
 }
 int capture(uint64_t submission,uint32_t buffer,uint32_t kind,const uint32_t* words,size_t count){
  if(captured_ || !submission || kind>1)return 60;
  std::vector<CtrPm4::Packet> packets;const int decoded=CtrPm4::DecodePackets(words,count,packets);if(decoded)return decoded;
  try{std::vector<uint32_t> copy(words,words+count);words_=std::move(copy);packets_=std::move(packets);}
  catch(...){return 112;}
  submission_=submission;buffer_=buffer;kind_=kind;captured_=true;return 0;
 }
 template<class Handler> int resume(Handler&& handler){
  if(!captured_)return 60;if(running_)return 91;
  running_=true;struct Reset{bool& value;~Reset(){value=false;}}reset{running_};
  while(next_<packets_.size()){
   const auto& packet=packets_[next_];int status=112;
   try{status=handler(submission_,buffer_,kind_,packet,words_.data()+packet.word);}
   catch(...){return 112;}
   if(status)return status;
   ++next_;
  }
  return 0;
 }
 const CtrPm4::Packet* pending()const{return captured_ && next_<packets_.size()?&packets_[next_]:nullptr;}
 size_t completed()const{return next_;}size_t count()const{return packets_.size();}
 size_t words()const{return words_.size();}
};
inline int gnm_command_cursor_control(){
 GnmCommandCursor cursor;uint32_t stream[]={0xc0001000,17,0xc0012d00,3,0,0xc0001000,19};
 if(cursor.capture(7,2,0,stream,7))return 1;stream[1]=99;
 const uint32_t original[]={0xc0001000,17};
 if(cursor.matches(7,2,0,0,original,2) || cursor.matches(8,2,0,0,original,2)!=60 ||
 cursor.matches(7,2,0,1,original,2)!=60 || cursor.matches(7,2,0,0,stream,2)!=112)return 10;
 unsigned calls=0;auto first=[&](uint64_t s,uint32_t b,uint32_t k,const CtrPm4::Packet& p,const uint32_t* data){
  ++calls;if(s!=7 || b!=2 || k || (p.word==0 && data[1]!=17))return 112;
  if(p.word==2)return 88;return 0;};
 if(cursor.resume(first)!=88 || calls!=2 || cursor.completed()!=1 || cursor.pending()->word!=2)return 2;
 if(cursor.resume(first)!=88 || calls!=3 || cursor.completed()!=1)return 3;
 unsigned retry=0;auto complete=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet& p,const uint32_t*){++retry;return p.word==2 || p.word==5?0:112;};
 if(cursor.resume(complete) || retry!=2 || cursor.completed()!=3 || cursor.pending())return 4;
 if(cursor.resume(complete) || retry!=2)return 5;
 GnmCommandCursor bad;const uint32_t truncated[]={0xc0012d00,3};
 if(bad.capture(7,0,0,truncated,2)!=60 || bad.count() || bad.words())return 6;
 if(bad.capture(7,0,0,stream,7))return 7;
 auto throwing=[](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet&,const uint32_t*)->int{throw 1;};
 if(bad.resume(throwing)!=112 || bad.completed())return 8;
 auto reentrant=[&](uint64_t,uint32_t,uint32_t,const CtrPm4::Packet&,const uint32_t*){return bad.resume(complete);};
 if(bad.resume(reentrant)!=91 || bad.completed())return 9;
 std::cout<<"{\"scope\":\"authored_gnm_command_cursor\",\"passed\":true,\"synthetic\":true,\"GPU_execution\":false,\"failure_keeps_cursor\":true,\"resume_without_duplicate_completion\":true,\"mutation_isolated\":true,\"exception_and_reentry_guarded\":true}\n";return 0;
}
}
