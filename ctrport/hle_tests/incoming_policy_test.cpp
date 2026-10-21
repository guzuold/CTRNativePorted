// SPDX-License-Identifier: GPL-2.0-or-later
// Flood test of the production policy with an injected authority transport.
#include "../hle/online_config.cpp"
#include <cassert>
std::atomic<unsigned> queries{0};
namespace ctr_hle::online {
bool http_exchange(const std::string&,const std::string&,const std::vector<std::pair<std::string,std::string>>&,
                   const std::string&,HttpResponse* response) {
    ++queries; std::this_thread::sleep_for(std::chrono::milliseconds(40)); response->status=403; return true;
}
}
int main() {
    WSADATA wd{}; WSAStartup(MAKEWORD(2,2),&wd);
    ctr_hle::online::IncomingPolicy policy;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<20000;++i) assert(!policy.check(htonl(0xc6330000u+i)));
    assert(std::chrono::steady_clock::now()-start<std::chrono::seconds(2));
    {std::lock_guard lock(policy.mutex);
     assert(policy.entries.size()<=256 && policy.queue.size()<=256);
     assert(policy.logged.size()<=4096 && policy.dropped==20000);}
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    assert(queries.load()<=static_cast<unsigned>(elapsed/250+1));
    std::printf("SECURITY policy: 20000 unknown sources denied; cache/queue <=256, log set <=4096, queries=%u elapsed_ms=%lld; PASS\n",queries.load(),elapsed);
}
