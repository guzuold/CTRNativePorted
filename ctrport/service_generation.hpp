// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <memory>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <condition_variable>
#include <thread>
#include <future>
#include <chrono>
namespace ctrport {
// A generation owns its module until the last in-flight dispatch releases it.
// No loading, guest suspension, instruction rewriting or service success here.
struct ServiceGeneration {
    struct Entry {std::string nid,library,module;const void* target;std::uint32_t signature_id=1;};
    std::shared_ptr<void> module_owner;
    std::vector<Entry> entries;
    std::uint64_t serial;
    std::string eboot_sha256;
};
class ServiceGenerations {
    std::mutex lock;
    std::shared_ptr<const ServiceGeneration> current;
    std::condition_variable changed;
    bool stopped=false;
    unsigned waiters=0;
public:
    std::shared_ptr<const ServiceGeneration> acquire(){
        std::lock_guard<std::mutex> guard(lock);return current;
    }
    unsigned waiting_count(){std::lock_guard<std::mutex> guard(lock);return waiters;}
    void shutdown(){
        std::shared_ptr<const ServiceGeneration> retired;
        {std::lock_guard<std::mutex> guard(lock);stopped=true;retired=std::move(current);}
        changed.notify_all();
        retired.reset(); // acquired calls keep their own module owner until return.
    }
    std::shared_ptr<const ServiceGeneration> wait_for(const std::string& nid,
        const std::string& library,const std::string& module){
        if(nid.empty() || library.empty() || module.empty())
            throw std::runtime_error("Missing service identity incomplete");
        std::unique_lock<std::mutex> guard(lock);
        auto present=[&]{
            if(!current)return false;
            for(const auto& entry:current->entries)
                if(entry.nid==nid && entry.library==library && entry.module==module)return true;
            return false;
        };
        ++waiters;
        try{changed.wait(guard,[&]{return stopped || present();});}
        catch(...){--waiters;throw;}
        --waiters;
        if(stopped)throw std::runtime_error("Missing service wait cancelled by shutdown");
        return current;
    }
    void publish(std::shared_ptr<void> owner,std::vector<ServiceGeneration::Entry> entries,std::uint64_t serial,std::string eboot_sha256={}){
        if(!owner || !serial || entries.empty())throw std::runtime_error("Generation owner/serial/entries missing");
        for(std::size_t i=0;i<entries.size();++i){
            const auto& a=entries[i];
            if(a.nid.empty() || a.library.empty() || a.module.empty() || !a.target || (a.signature_id!=1 && a.signature_id!=2 && a.signature_id!=9))
                throw std::runtime_error("Generation entry incomplete");
            for(std::size_t j=0;j<i;++j){const auto& b=entries[j];
                if(a.nid==b.nid && a.library==b.library && a.module==b.module)
                    throw std::runtime_error("Generation duplicate identity");}
        }
        auto next=std::make_shared<ServiceGeneration>(ServiceGeneration{std::move(owner),std::move(entries),serial,std::move(eboot_sha256)});
        std::shared_ptr<const ServiceGeneration> retired;
        {
            std::lock_guard<std::mutex> guard(lock);
            if(stopped)throw std::runtime_error("Service generation publication after shutdown");
            if(current && serial<=current->serial)throw std::runtime_error("Generation serial not increasing");
            retired=std::move(current);current=std::move(next);
        }
        changed.notify_all();
        // Module finalization may reenter host callbacks; never run it under lock.
        retired.reset();
    }
};
inline int service_wait_control(){
    ServiceGenerations generations;static const int target=7;
    auto wait=[&]{return generations.wait_for("needed","test","test");};
    auto first=std::async(std::launch::async,wait);
    auto second=std::async(std::launch::async,wait);
    auto observed=[&](unsigned count){
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(generations.waiting_count()!=count){
            if(std::chrono::steady_clock::now()>end){generations.shutdown();throw std::runtime_error("Service waiter not observed");}
            std::this_thread::yield();
        }
    };
    observed(2);
    auto owner=std::make_shared<int>(1);
    generations.publish(owner,{{"other","test","test",&target}},1);
    if(first.wait_for(std::chrono::milliseconds(10))!=std::future_status::timeout ||
       second.wait_for(std::chrono::milliseconds(10))!=std::future_status::timeout){
        generations.shutdown();throw std::runtime_error("Unrelated service resumed wait");
    }
    generations.publish(owner,{{"needed","test","test",&target}},2);
    if(first.get()->serial!=2 || second.get()->serial!=2)
        throw std::runtime_error("Matching publication did not resume waiters");
    auto cancelled=std::async(std::launch::async,[&]{
        try{generations.wait_for("absent","test","test");return false;}
        catch(const std::runtime_error&){return true;}
    });
    observed(1);generations.shutdown();
    if(!cancelled.get())throw std::runtime_error("Shutdown returned service success");
    bool rejected=false;
    try{generations.publish(owner,{{"needed","test","test",&target}},3);}
    catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Publication after shutdown accepted");
    return 0;
}
inline int service_generation_control(){
    ServiceGenerations generations;unsigned releases=0,rejected=0;
    auto owner=[&]{return std::shared_ptr<void>(new int(1),[&](void* p){delete static_cast<int*>(p);++releases;});};
    static const int first=1,second=2;
    using Entry=ServiceGeneration::Entry;
    std::vector<Entry> entries{{"synthetic","test","test",&first}};
    if(generations.acquire())throw std::runtime_error("Unexpected initial generation");
    generations.publish(owner(),entries,1);
    auto held=generations.acquire();
    entries[0].target=&second;generations.publish(owner(),entries,2);
    if(releases || held->serial!=1 || held->entries[0].target!=&first ||
       generations.acquire()->entries[0].target!=&second)throw std::runtime_error("In-flight generation changed");
    held.reset();if(releases!=1)throw std::runtime_error("Retired owner lifetime differs");
    auto before=generations.acquire();
    try{generations.publish({},entries,3);}catch(const std::runtime_error&){++rejected;}
    auto duplicate=entries;duplicate.push_back(entries[0]);
    try{generations.publish(owner(),duplicate,3);}catch(const std::runtime_error&){++rejected;}
    auto missing=entries;missing[0].target=nullptr;
    try{generations.publish(owner(),missing,3);}catch(const std::runtime_error&){++rejected;}
    try{generations.publish(owner(),entries,2);}catch(const std::runtime_error&){++rejected;}
    if(rejected!=4 || generations.acquire()!=before)throw std::runtime_error("Rejected generation changed active state");
    return 0;
}
}
