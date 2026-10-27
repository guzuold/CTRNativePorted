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
}
