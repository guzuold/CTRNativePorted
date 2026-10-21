// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "launcher_online.hpp"
#include <functional>
#include <fstream>
#include <map>
#include <set>
#include <windows.h>
namespace ctr_online {
// Single election-worker owner. All I/O is injectable; only signed identities may
// be returned by Identify. Saved rows are hints, never authenticated candidates.
class HubElection {
public:
    static constexpr unsigned failure_limit=3;
    static constexpr std::uint64_t max_age=300;
    static constexpr std::size_t capacity=16;
    using Clock=std::function<std::uint64_t()>;
    using Search=std::function<std::vector<std::string>()>;
    using Identify=std::function<std::optional<MatchHub>(const std::string&)>;
    using Publish=std::function<void(std::optional<MatchHub>,const std::string&,std::size_t)>;
    struct Candidate { MatchHub hub; std::uint64_t verified=0; unsigned failures=0; bool active=false; std::string source; };
    MatchHub self;
    bool reachable;
    std::map<std::string,std::string> losses;
    std::map<std::string,Candidate> candidates;
    HubElection(MatchHub s,bool r):self(std::move(s)),reachable(r){}
    static std::string endpoint(const MatchHub& h){return h.ip+":"+std::to_string(h.port);}
    static bool recent(std::uint64_t then,std::uint64_t now){return then<=now && now-then<max_age;}
    void load(const std::filesystem::path& path,std::uint64_t now) {
        candidates.clear();
        try {
            if(!std::filesystem::exists(path) || std::filesystem::file_size(path)>65536) return;
            std::ifstream f(path);std::string text((std::istreambuf_iterator<char>(f)),{});
            const auto rows=parse_json(text);if(!rows.is_array())return;
            for(const auto& row:rows.items){
                Candidate c;c.hub={row.at("code").text,row.at("ip").text,8780};
                const auto port=row.at("port").as_u64();c.verified=row.at("verified").as_u64();
                if(c.hub.code.empty() || c.hub.code.size()>256 || c.hub.ip.size()>15 || !port || port>65535 || !recent(c.verified,now))continue;
                c.hub.port=static_cast<std::uint16_t>(port);c.source="saved";
                if(candidates.size()<capacity)candidates.emplace(c.hub.code,std::move(c));
            }
        }catch(...){candidates.clear();}
    }
    bool save(const std::filesystem::path& path) const {
        Json rows=Json::array();
        for(const auto& [code,c]:candidates){
            Json row=Json::object();row.set("code",Json::of(code));row.set("ip",Json::of(c.hub.ip));
            row.set("port",Json::of(c.hub.port));row.set("verified",Json::of(c.verified));rows.items.push_back(std::move(row));
        }
        std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
        auto tmp=path;tmp+=L".tmp";std::ofstream f(tmp,std::ios::trunc);f<<dump_json(rows);f.close();
        return bool(f) && MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    }
    void publish(const Publish& out) const {
        std::vector<MatchHub> verified;for(const auto& [code,c]:candidates)if(c.active)verified.push_back(c.hub);
        auto best=choose_hub(self,reachable,verified);
        const auto i=best?candidates.find(best->code):candidates.end();
        out(best,i!=candidates.end()?i->second.source:(best?"self":"none"),verified.size()+(reachable?1:0));
    }
    void round(std::uint64_t now,const Search& search,const Identify& identify,const Publish& out,const Clock& clock={}) {
        losses.clear();std::set<std::string> attempted;
        // Known endpoints go first, independently of DHT availability. Each gets
        // at most one direct failure per round, even if also advertised by DHT.
        for(auto i=candidates.begin();i!=candidates.end();){
            auto& c=i->second;
            if(!recent(c.verified,clock?clock():now)){losses[i->first]="verification_expired";i=candidates.erase(i);continue;}
            const auto ep=endpoint(c.hub);attempted.insert(ep);auto h=identify(ep);
            if(h && h->code==i->first){c.hub=*h;c.verified=clock?clock():now;c.failures=0;c.source=c.active?"direct":"saved";c.active=true;publish(out);}
            else if(++c.failures>=failure_limit){losses[i->first]="direct_failures";i=candidates.erase(i);continue;}
            ++i;
        }
        publish(out); // Saved candidates become usable before waiting for DHT.
        std::set<std::string> ips;std::size_t count=0;
        for(const auto& ep:search()){
            const auto ip=ep.substr(0,ep.find(':'));
            if(!ips.insert(ip).second)continue;
            if(count++>=16)break;
            if(attempted.count(ep))continue;
            auto h=identify(ep);if(!h || h->code==self.code)continue;
            auto i=candidates.find(h->code);
            if(i==candidates.end() && candidates.size()>=capacity)continue;
            candidates[h->code]=Candidate{*h,clock?clock():now,0,true,"dht"};publish(out);
        }
        publish(out);
    }
};
// The same transition recorder is used by Host and deterministic tests.
class HubEvents {
    std::optional<MatchHub> previous;
public:
    using Record=std::function<void(const char*,Json)>;
    void observe(const HubElection& election,const std::optional<MatchHub>& chosen,
                 const std::string& source,std::size_t count,const Record& record){
        const bool changed=bool(chosen)!=bool(previous) || (chosen && previous &&
            (chosen->code!=previous->code || chosen->ip!=previous->ip || chosen->port!=previous->port));
        if(!changed)return;
        if(previous){
            Json f=Json::object();f.set("code",Json::of(previous->code.substr(0,16)));
            f.set("address",Json::of(HubElection::endpoint(*previous)));
            const auto reason=election.losses.find(previous->code);
            f.set("reason",Json::of(reason==election.losses.end()?"lower_code_or_endpoint_changed":reason->second));
            record("hub_lost",std::move(f));
        }
        if(chosen){
            Json f=Json::object();f.set("code",Json::of(chosen->code.substr(0,16)));
            f.set("address",Json::of(HubElection::endpoint(*chosen)));f.set("source",Json::of(source));
            f.set("candidates",Json::of(static_cast<unsigned>(count)));record("hub_elected",std::move(f));
        }
        previous=chosen;
    }
};

}
