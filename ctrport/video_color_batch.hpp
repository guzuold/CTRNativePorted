// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include "video_session_owner.hpp"
#include <vector>
#include <stdexcept>
namespace ctrport {
// Undefined-content actual attachment owners. No LOAD or draw admission.
class VideoColorBatch {
 struct Backing{CtrGraphicsColorRequirementV1 demand;uint64_t token;};
 void* session_;std::vector<uint64_t> tokens_;std::vector<Backing> backing_;bool closed_=false;
public:
 explicit VideoColorBatch(void* session):session_(session){tokens_.reserve(128);backing_.reserve(128);}
 VideoColorBatch(const VideoColorBatch&)=delete;VideoColorBatch& operator=(const VideoColorBatch&)=delete;
 ~VideoColorBatch(){const int status=close();if(status)std::cerr<<"color batch cleanup status="<<status<<std::endl;}
 int create(const CtrGraphicsColorRequirementV1& demand,uint64_t* output){
  if(closed_ || !session_ || !output)return 60;if(tokens_.size()>=128)return 88;
  uint64_t token=0;
  // Require blend-capable format even when this permutation might not blend.
  // This is a conservative capability requirement, not inferred blend state.
  const int status=ctr_video_color_attachment_create_v1(session_,&demand,1,&token);
  if(status)return status;if(!token)return 112;tokens_.push_back(token);*output=token;return 0;
 }
 // Reuse only exact guest backing identities within this original submission.
 // Slot is a binding identity, not a resource identity. Partial aliases and
 // changed format/extent/sample interpretations remain unsupported explicitly.
 int resolve(const CtrGraphicsColorRequirementV1& p,uint64_t* output,bool* reused){
  if(closed_ || !output || !reused || p.version!=1 || p.size!=sizeof(p) || p.slot>=8 || !p.bytes || p.guest_address>UINT64_MAX-p.bytes)return 60;
  for(const auto& old:backing_){const auto& q=old.demand;
   const bool overlap=p.guest_address<q.guest_address+q.bytes && q.guest_address<p.guest_address+p.bytes;
   if(!overlap)continue;
   if(p.guest_address!=q.guest_address || p.bytes!=q.bytes || p.pitch!=q.pitch || p.height!=q.height ||
    p.layers!=q.layers || p.samples!=q.samples || p.tile_mode!=q.tile_mode || p.data_format!=q.data_format ||
    p.number_format!=q.number_format || p.version!=q.version || p.size!=q.size)return 88;
   *output=old.token;*reused=true;return 0;
  }
  if(backing_.size()>=128)return 88;uint64_t token=0;const int status=create(p,&token);if(status)return status;
  backing_.push_back({p,token});*output=token;*reused=false;return 0;
 }
 size_t size()const{return tokens_.size();}
 int close(){if(closed_)return 0;closed_=true;int first=0;
  for(auto i=tokens_.rbegin();i!=tokens_.rend();++i){const int s=ctr_video_color_attachment_release_v1(session_,*i);if(s && !first)first=s;}
  tokens_.clear();return first;
 }
};
}
