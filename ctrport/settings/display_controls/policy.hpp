// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../../online_server/json.hpp"
#include <vector>
#include <utility>
#include <stdexcept>
namespace ctrport::settings::display {
inline std::string value(const ctr_online::Json& d,const char* key){auto p=d.find(key);if(!p)return "original";if(!p->is_string())throw std::invalid_argument(key);return p->text;}
inline std::vector<std::pair<std::string,std::string>> environment(const ctr_online::Json& d,bool extended){
 const char* keys[]={"hdr_output","display_gamma","hdr_peak_nits","hdr_exposure"};
 const char* vars[]={"CTR_SETTINGS_HDR","CTR_SETTINGS_GAMMA","CTR_SETTINGS_HDR_PEAK","CTR_SETTINGS_HDR_EXPOSURE"};
 const std::vector<std::vector<std::string>> domains={{"original","off","native"},{"original","0.8","0.9","1.1","1.2"},{"original","400","600","1000","1500"},{"original","0.75","1.25","1.5"}};
 std::vector<std::pair<std::string,std::string>> result;
 for(unsigned i=0;i<4;++i){auto v=value(d,keys[i]);bool good=false;for(auto& item:domains[i])good|=v==item;if(!good)throw std::invalid_argument(keys[i]);if(!extended&&v!="original")throw std::runtime_error("display setting requires settings GPU display ABI 1");result.emplace_back(vars[i],v=="original"?"":v);}
 // Current temporal and scene-FSR contracts consume SDR buffers. Native HDR uses
 // original game rendering; do not feed PQ through those SDR scene passes.
 const auto up=value(d,"upscaler");
 if(value(d,"hdr_output")=="native"&&up!="original"&&up!="off")throw std::invalid_argument("native HDR requires scene upscaler off");
 return result;
}
}
