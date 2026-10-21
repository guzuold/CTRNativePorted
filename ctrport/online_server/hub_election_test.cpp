// SPDX-License-Identifier: GPL-2.0-or-later
#include "hub_election.hpp"
#include <cstdio>
#include <map>
#include <thread>
#include <future>
using namespace ctr_online;
int failures=0;
void check(bool b,const char* s){std::printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)++failures;}
int main(int argc,char** argv){
 if(argc==5 && (std::string(argv[1])=="--member" || std::string(argv[1])=="--hub")){
    HubGate gate;gate.reset(std::nullopt);
    ServerConfig c;c.host="127.0.0.3";c.public_host=c.host;c.relay_port=0;c.tcp_record_ports={};c.udp_record_ports={};
    c.state_dir=std::filesystem::path(argv[2])/"runtime/user/online";
    c.log_path=std::filesystem::path(argv[2])/"runtime/user/logs/member.jsonl";
    const bool is_hub=std::string(argv[1])=="--hub";
    c.host=is_hub?"127.0.0.2":"127.0.0.3";c.public_host=c.host;
    c.http_port=29780;c.lobby_port=29774;c.discovery_port=29781;
    c.friend_network=true;c.hub_reachable=false;
    c.match_hub=[&]{return gate.current();};
    Server* recording=nullptr;
    const bool assigned=std::string(argv[4])=="assigned";
    if(assigned)gate.publish(MatchHub{argv[3],"127.0.0.2",29780});
    const bool appears=std::string(argv[4])=="appears";
    c.await_match_hub=[&](auto budget){
        auto arrival=std::async(std::launch::async,[&]{
            if(gate.wait_until(std::chrono::steady_clock::now()+std::chrono::seconds(2)) && appears){
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                HubElection election(MatchHub{"SELF","127.0.0.3",29780},false);HubEvents events;
                election.round(5000,[]{return std::vector<std::string>{"127.0.0.2:29780"};},
                    [&](const std::string& ep)->std::optional<MatchHub>{
                        std::string code;auto v=identify_endpoint(ep,code,1000);
                        if(v.status!=VerifyStatus::Verified)return std::nullopt;
                        return MatchHub{code,v.ip,v.port};
                    },[&](auto h,const std::string& source,std::size_t count){
                        events.observe(election,h,source,count,[&](const char* name,Json f){recording->record_event(name,std::move(f));});
                        gate.publish(h);
                    });
            }
        });
        auto result=gate.acquire(budget);arrival.get();return result;
    };
    // late: no hub for the first public task; the hub is published 100 ms after that wait expires,
    // in the middle of the player's matchmaking sequence.
    std::thread late_publisher;
    if(std::string(argv[4])=="late") c.await_match_hub=[&](auto budget){
        auto result=gate.acquire(budget);
        if(!result && !late_publisher.joinable())
            late_publisher=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(100));gate.publish(MatchHub{argv[3],"127.0.0.2",29780});});
        return result;
    };
    if(is_hub){c.match_hub={};c.await_match_hub={};}
    Server service(c);recording=&service;service.start();
    if(!service.failures().empty())return 2;std::this_thread::sleep_for(std::chrono::seconds(45));gate.stop();service.stop();
    if(late_publisher.joinable())late_publisher.join();return 0;
 }

 const MatchHub self{"M","192.0.2.9"}, a{"A","192.0.2.1"}, b{"B","192.0.2.2"};
 std::map<std::string,MatchHub> live{{"192.0.2.1:8780",a},{"192.0.2.2:8780",b}};
 std::vector<std::string> ads;
 auto search=[&]{return ads;};
 auto identify=[&](const std::string& ep)->std::optional<MatchHub>{auto i=live.find(ep);return i==live.end()?std::nullopt:std::optional<MatchHub>(i->second);};
 std::optional<MatchHub> chosen;
 auto publish=[&](auto h,const std::string&,std::size_t){chosen=h;};
 HubElection e(self,false);
 bool stable=true;
 for(unsigned i=0;i<10;++i){ads=i%5==0?std::vector<std::string>{"192.0.2.1:8780"}:std::vector<std::string>{};e.round(1000+i*60,search,identify,publish);stable &= chosen && chosen->code=="A";}
 check(stable,"intermittent DHT: known live hub survives four empty searches");
 ads={"192.0.2.2:8780"};e.round(1600,search,identify,publish);ads.clear();live.erase("192.0.2.1:8780");
 e.round(1660,search,identify,publish);check(chosen && chosen->code=="A","one direct failure retains hub");
 e.round(1720,search,identify,publish);check(chosen && chosen->code=="A","two direct failures retain hub");
 e.round(1780,search,identify,publish);check(chosen && chosen->code=="B","third direct failure selects remaining live hub");
 live["192.0.2.1:8780"]=a;
 HubElection x(self,false),y(self,false);ads={"192.0.2.1:8780"};x.round(2000,search,identify,publish);
 ads={"192.0.2.2:8780"};x.round(2060,search,identify,publish);const auto xc=chosen;
 y.round(2000,search,identify,publish);ads={"192.0.2.1:8780"};y.round(2060,search,identify,publish);
 check(xc && chosen && xc->code==chosen->code && chosen->code=="A","partial views converge on lowest verified code");
 const auto file=std::filesystem::temp_directory_path()/("ctr-hub-election-"+std::to_string(GetCurrentProcessId())+".json");
 check(x.save(file),"persist verified candidates");HubElection restarted(self,false);restarted.load(file,2070);ads.clear();chosen.reset();
 restarted.round(2070,[&]{check(chosen && chosen->code=="A","saved hub published before DHT wait");return ads;},[&](const std::string& ep){
     if(ep=="192.0.2.2:8780")check(chosen && chosen->code=="A","verified saved hub published before probing other cached endpoints");
     return identify(ep);
 },publish);check(chosen && chosen->code=="A","restart directly verifies saved hub without DHT");
 live.clear();HubElection dead(self,false);dead.load(file,2080);dead.round(2080,search,identify,publish);check(!chosen,"saved unresponsive candidate is never elected");
 dead.save(file);HubElection retry(self,false);retry.load(file,2081);
 check(retry.candidates.size()==2,"unverified saved hints survive a transient restart failure");
 HubElection stale(self,false);stale.load(file,2500);live["192.0.2.1:8780"]=a;stale.round(2500,search,identify,publish);check(!chosen,"expired saved candidate ignored");
 HubElection diverse(self,false);ads.assign(16,"192.0.2.8:1111");ads.push_back("192.0.2.1:8780");diverse.round(3000,search,identify,publish);check(chosen && chosen->code=="A","deduplicate IP before sixteen-address limit");
 HubElection local(self,true);ads.clear();local.round(3000,search,identify,publish);check(chosen && chosen->code=="M","reachable self immediately eligible");
 // Hard expiry even when the next round is delayed; identity change is not renewal.
 ads={"192.0.2.1:8780"};HubElection expiry(self,false);expiry.round(4000,search,identify,publish);
 ads.clear();expiry.round(4300,search,identify,publish);check(!chosen,"five-minute absolute expiry removes overdue candidate");
 ads={"192.0.2.1:8780"};HubElection mismatch(self,false);mismatch.round(5000,search,identify,publish);
 mismatch.save(file);live["192.0.2.1:8780"]=b;HubElection replaced(self,false);replaced.load(file,5001);ads.clear();
 replaced.round(5001,search,identify,publish);check(!chosen,"different signed identity cannot activate saved code");
 HubGate gate;gate.publish(a,std::chrono::seconds(0));check(!gate.current(),"expired publication is not returned between rounds");gate.reset(std::nullopt);
 auto worker=std::async(std::launch::async,[&]{bool woke=gate.wait_until(std::chrono::steady_clock::now()+std::chrono::seconds(2));
     std::this_thread::sleep_for(std::chrono::milliseconds(30));gate.publish(a);return woke;});
 auto acquired=gate.acquire(std::chrono::milliseconds(1000));check(acquired && acquired->code=="A" && worker.get(),"task demand wakes election and waits for hub publication");
 gate.reset(std::nullopt);auto start=std::chrono::steady_clock::now();auto none=gate.acquire(std::chrono::milliseconds(40));
 check(!none && std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(35),"no-hub wait obeys declared deadline");
 auto waiting=std::async(std::launch::async,[&]{return gate.acquire(std::chrono::seconds(10));});
 gate.stop();check(waiting.wait_for(std::chrono::milliseconds(100))==std::future_status::ready && !waiting.get(),"stop releases matchmaking waiters");
 gate.publish(a);check(!gate.current(),"late election cannot republish after shutdown");
 HubEvents events;std::vector<std::pair<std::string,Json>> recorded;
 auto record=[&](const char* name,Json f){recorded.emplace_back(name,std::move(f));};
 events.observe(expiry,a,"dht",1,record);expiry.losses["A"]="direct_failures";events.observe(expiry,std::nullopt,"none",0,record);
 check(recorded.size()==2 && recorded[0].first=="hub_elected" && recorded[0].second.at("source").text=="dht" &&
       recorded[0].second.at("address").text=="192.0.2.1:8780" && recorded[0].second.at("candidates").as_u64()==1 &&
       recorded[1].first=="hub_lost" && recorded[1].second.at("reason").text=="direct_failures","transition logs contain election fields and loss reason");
 {
    // Every round reports what the DHT returned and what answered, so a PC that never
    // finds a hub shows it in its log.
    HubElection counted(self,false);
    std::map<std::string,MatchHub> answering{{"192.0.2.1:8780",a}};
    ads={"192.0.2.1:8780","192.0.2.2:8780","192.0.2.1:9999"};
    counted.round(5000,search,[&](const std::string& ep)->std::optional<MatchHub>{auto i=answering.find(ep);return i==answering.end()?std::nullopt:std::optional<MatchHub>(i->second);},publish);
    Json fields=counted.round_fields(chosen);
    check(fields.at("dht_results").as_u64()==3 && fields.at("verified").as_u64()==1 && fields.at("candidates").as_u64()==1 &&
          fields.at("hub").text==a.code,"round fields: DHT results, verified answers, candidates and hub");
    ads.clear();
    counted.round(5060,search,[&](const std::string&)->std::optional<MatchHub>{return std::nullopt;},publish);
    fields=counted.round_fields(std::nullopt);
    check(fields.at("dht_results").as_u64()==0 && fields.at("verified").as_u64()==0 && fields.at("hub").is_null(),"round fields: an empty DHT round shows zero results and no hub");
 }
 std::filesystem::remove(file);std::printf("failures=%d\n",failures);return failures?1:0;
}
