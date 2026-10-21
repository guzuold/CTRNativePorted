// SPDX-License-Identifier: GPL-2.0-or-later
// Tests of the production incoming policy with an injected authority transport:
// flood bounds, steady traffic of authorized peers across renewals, revocation and
// an unreachable service.
#include "../hle/online_config.cpp"
#include <cassert>
std::atomic<unsigned> queries{0};
std::atomic<int> answer_status{403}; // 0: service unreachable
std::atomic<int> answer_delay_ms{40};
std::atomic<std::uint32_t> denied_address{0}; // answered 403 whatever answer_status says
namespace ctr_hle::online {
bool http_exchange(const std::string&,const std::string& path,const std::vector<std::pair<std::string,std::string>>&,
                   const std::string&,HttpResponse* response) {
    ++queries; std::this_thread::sleep_for(std::chrono::milliseconds(answer_delay_ms.load()));
    const int status=answer_status.load();
    if(!status) return false;
    response->status=status;
    if(denied_address && path.find(format_address(denied_address))!=std::string::npos) response->status=403;
    return true;
}
}
namespace {
using Clock=std::chrono::steady_clock;
int failures=0;
void check(bool ok,const char* what){std::printf("%s %s\n",ok?"PASS":"FAIL",what);if(!ok)++failures;}
std::uint32_t peer(unsigned i){return htonl(0xcb007100u+i);} // 203.0.113.x
// All `count` peers send every 16 ms (60 Hz), as in a race, until each is authorized;
// then counts refused checks over `seconds`.
unsigned steady(ctr_hle::online::IncomingPolicy& policy,unsigned count,int seconds,std::uint32_t flood_base=0){
    const auto deadline=Clock::now()+std::chrono::seconds(10);
    for(std::vector<bool> ok(count,false);Clock::now()<deadline;std::this_thread::sleep_for(std::chrono::milliseconds(16))){
        bool all=true;
        for(unsigned i=0;i<count;++i) all&=(ok[i]=policy.check(peer(i)) || ok[i]);
        if(all) break;
    }
    unsigned refused=0,flood=0;
    const auto end=Clock::now()+std::chrono::seconds(seconds);
    while(Clock::now()<end){
        for(unsigned i=0;i<count;++i) refused+=!policy.check(peer(i));
        if(flood_base) for(unsigned k=0;k<50;++k) policy.check(htonl(flood_base+(flood++%60000)));
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return refused;
}
// Milliseconds until `address` is refused, polling every 10 ms, at most `limit_ms`.
long long until_refused(ctr_hle::online::IncomingPolicy& policy,std::uint32_t address,int limit_ms){
    const auto start=Clock::now();
    while(policy.check(address)){
        if(Clock::now()-start>std::chrono::milliseconds(limit_ms)) return -1;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count();
}
}
int main(int argc,char** argv) {
    WSADATA wd{}; WSAStartup(MAKEWORD(2,2),&wd);
    const std::string mode=argc>1?argv[1]:"all";
    if(mode=="all" || mode=="flood"){
        ctr_hle::online::IncomingPolicy policy;
        const auto start=Clock::now();
        bool denied=true;
        for(unsigned i=0;i<20000;++i) denied&=!policy.check(htonl(0xc6330000u+i));
        check(denied && Clock::now()-start<std::chrono::seconds(2),"flood: 20000 unknown sources denied without blocking");
        {std::lock_guard lock(policy.mutex);
         check(policy.entries.size()<=256 && policy.queue.size()<=256 && policy.logged.size()<=4096 && policy.dropped==20000,
               "flood: cache, queue and log set bounded");}
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-start).count();
        check(queries.load()<=static_cast<unsigned>(elapsed/250+1),"flood: unknown-source queries rate limited");
    }
    if(mode=="all" || mode=="steady"){
        for(const int delay:{0,30,100}){
            answer_status=200; answer_delay_ms=delay;
            ctr_hle::online::IncomingPolicy policy;
            const unsigned refused=steady(policy,1,5);
            std::printf("steady: 1 peer, service %d ms, refused=%u\n",delay,refused);
            check(refused==0,"steady: one authorized peer never refused across renewals");
        }
        answer_status=200; answer_delay_ms=100;
        {ctr_hle::online::IncomingPolicy policy;
         const unsigned refused=steady(policy,7,5);
         std::printf("steady: 7 peers, service 100 ms, refused=%u\n",refused);
         check(refused==0,"steady: seven authorized peers never refused across renewals");}
        {ctr_hle::online::IncomingPolicy policy;
         const unsigned refused=steady(policy,7,5,0xc6340000u);
         std::printf("steady: 7 peers during an unknown-source flood, refused=%u\n",refused);
         check(refused==0,"steady: renewals of authorized peers not delayed by a flood");}
    }
    if(mode=="all" || mode=="revoke"){
        answer_status=200; answer_delay_ms=30; denied_address=0;
        {ctr_hle::online::IncomingPolicy policy;
         steady(policy,1,1);
         denied_address=peer(0);
         const auto ms=until_refused(policy,peer(0),5000);
         std::printf("revoke: refused after %lld ms\n",ms);
         check(ms>=0 && ms<=1500,"revoke: a peer the service stops authorizing is refused within 1.5 s");
         denied_address=0;}
        {ctr_hle::online::IncomingPolicy policy;
         steady(policy,1,1);
         denied_address=peer(0);
         std::this_thread::sleep_for(std::chrono::milliseconds(1200));
         check(!policy.check(peer(0)),"revoke: a peer quiet for over a second is not accepted before its renewal answers");
         std::this_thread::sleep_for(std::chrono::milliseconds(200));
         check(!policy.check(peer(0)),"revoke: the refusal then holds");
         denied_address=0;}
        {ctr_hle::online::IncomingPolicy policy;
         steady(policy,1,1);
         answer_status=0;
         const auto ms=until_refused(policy,peer(0),8000);
         std::printf("unreachable: refused after %lld ms\n",ms);
         check(ms>=0 && ms<=3500,"unreachable: authorization ends within the declared 3 s validity");
         answer_status=200;}
    }
    if(mode=="all" || mode=="grant"){
        answer_status=403; answer_delay_ms=100;
        ctr_hle::online::IncomingPolicy policy;
        const unsigned before=queries.load();
        policy.grant(peer(9));
        check(policy.check(peer(9)) && queries.load()==before,"grant: the answer to the game's own datagram is accepted at once, without a query");
        // Granted, then quiet: after the renewal the service is asked and its refusal holds.
        std::this_thread::sleep_for(std::chrono::milliseconds(2200));
        check(!policy.check(peer(9)),"grant: a quiet granted peer waits for its renewal");
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        check(!policy.check(peer(9)),"grant: the service's refusal then holds");
        answer_status=200;
    }
    std::printf("failures=%d\n",failures);
    return failures?1:0;
}
