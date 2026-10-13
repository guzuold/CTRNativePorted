// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Sidecar preserves the DirectMemoryBacking ABI used by the pinned video library.
namespace ctrport {
struct QueryMappingRecord {uint64_t end;int protection;};
inline std::mutex query_mapping_mutex;
inline std::map<uint64_t,QueryMappingRecord> query_mapping_records;
inline void record_query_mapping(uint64_t start,uint64_t bytes,int protection){
 std::lock_guard<std::mutex> lock(query_mapping_mutex);
 auto first=query_mapping_records.upper_bound(start);
 if(first!=query_mapping_records.begin()){auto previous=std::prev(first);if(previous->second.end>start)first=previous;}
 std::vector<std::pair<uint64_t,QueryMappingRecord>> retained;
 while(first!=query_mapping_records.end() && first->first<start+bytes){
  const auto old=*first;
  if(old.first<start)retained.push_back({old.first,{start,old.second.protection}});
  if(old.second.end>start+bytes)retained.push_back({start+bytes,old.second});
  first=query_mapping_records.erase(first);
 }
 for(const auto& item:retained)query_mapping_records.emplace(item);
 query_mapping_records[start]={start+bytes,protection};
}
}
