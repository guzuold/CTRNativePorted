// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_slot_admission.hpp"
#include "service_integer_dispatch.hpp"
#include "kernel_mutex_attributes.hpp"
#include "service_integer_host_api.h"
#include "service_dll.hpp"
#include <thread>
#include <atomic>
namespace ctrport {
// Process-resident ownership: original workers may outlive the entry launcher.
inline std::vector<std::unique_ptr<ServiceIntegerThunk>> startup_mutex_thunks;
extern "C" void ctr_sysv_mutex_init();
inline std::atomic<unsigned> startup_mutex_calls{0};
inline std::uint32_t startup_mutex_callback(std::uint64_t a,std::uint64_t b,std::uint64_t c,std::uint64_t* out){
 try{
  const auto value=kernel_mutex_init(reinterpret_cast<std::uint64_t*>(a),reinterpret_cast<const std::uint64_t*>(b),reinterpret_cast<const char*>(c));
  *out=static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
  const auto count=++startup_mutex_calls;
  if(count<=2)std::cout<<"{\"scope\":\"original_startup_mutex_dll_return\",\"call\":"<<count<<",\"result\":"<<value<<"}\n"<<std::flush;
  return 0;
 }catch(const std::exception&){return 1;}
}
inline void startup_mutex_log(void*,const char* bytes,std::uint32_t size){std::cout.write(bytes,size);std::cout.flush();}
inline CtrIntegerHostApi startup_mutex_api{};
inline CtrHostApi startup_mutex_host{};
struct StartupMutexPublisher {
    std::atomic<bool> stop{false};
    std::thread worker;
    ~StartupMutexPublisher(){stop.store(true);if(worker.joinable())worker.join();}
};
// Destroy/join publisher before destroying the contexts declared above.
inline StartupMutexPublisher startup_mutex_publisher;
inline void install_startup_mutex_reload(std::vector<MappedImage>& images,
    const std::vector<ImageMetadata>& metadata,const std::string& pin){
    char directory[32768]{};
    const auto size=GetEnvironmentVariableA("CTR_STARTUP_MUTEX_DLL_DIR",directory,sizeof(directory));
    if(!size)return;
    if(size>=sizeof(directory))throw std::runtime_error("Startup DLL directory too long");
    const std::filesystem::path root=std::filesystem::canonical(directory);
    startup_mutex_api={sizeof(CtrIntegerHostApi),1,&startup_mutex_callback};
    startup_mutex_host={sizeof(CtrHostApi),CTRPORT_SERVICE_ABI_VERSION,&startup_mutex_api,&startup_mutex_log};
    unsigned installed=0;
    for(std::size_t i=0;i<metadata.size();++i){
        std::uint64_t extent=0;for(const auto& span:metadata[i].spans)extent=(std::max)(extent,span.second);
        for(const auto& symbol:metadata[i].imports){
            if(symbol.nid!="cmo1RIYva9o" || symbol.library!="libkernel" || symbol.module!="libkernel")continue;
            ServiceImportRecord record{pin,metadata[i].sha256,symbol.nid,symbol.library,symbol.module,symbol.slot,2};
            const auto token=integer_dispatch_records.add(record,pin,metadata[i].sha256,extent);
            auto thunk=std::make_unique<ServiceIntegerThunk>(token);
            admit_service_slot(images[i],metadata[i],pin,record,reinterpret_cast<const void*>(&ctr_sysv_mutex_init),thunk->target());
            startup_mutex_thunks.push_back(std::move(thunk));++installed;
        }
    }
    if(!installed)throw std::runtime_error("Startup reload write import absent");
    // This bounded publisher is a bring-up control, not a general file watcher.
    // All context and thunk storage remains resident until process exit.
    if(startup_mutex_publisher.worker.joinable())throw std::runtime_error("Startup write publisher already started");
    startup_mutex_publisher.worker=std::thread([root,pin]{
        try{
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
            while(!integer_dispatch_generations.waiting_count()){
                if(startup_mutex_publisher.stop.load())return;
                if(std::chrono::steady_clock::now()>deadline){
                    std::cerr<<"startup write DLL: original call not observed within20s\n"<<std::flush;return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout<<"{\"scope\":\"original_startup_mutex_wait_observed\"}\n"<<std::flush;
            load_service_generation(integer_dispatch_generations,root,root/L"service_mutex_1.dll",startup_mutex_host,pin,1);
            std::cout<<"{\"scope\":\"original_startup_mutex_dll_published\",\"serial\":1}\n"<<std::flush;
            while(!startup_mutex_calls.load()){
                if(startup_mutex_publisher.stop.load() || std::chrono::steady_clock::now()>deadline)return;
                std::this_thread::yield();
            }
            load_service_generation(integer_dispatch_generations,root,root/L"service_mutex_2.dll",startup_mutex_host,pin,2);
            std::cout<<"{\"scope\":\"original_startup_mutex_dll_published\",\"serial\":2}\n"<<std::flush;
        }catch(const std::exception& error){
            std::cerr<<"startup write DLL: "<<error.what()<<'\n'<<std::flush;
            integer_dispatch_generations.shutdown();
        }
    });
    std::cout<<"{\"scope\":\"startup_mutex_reload_admitted\",\"slots\":"<<installed<<"}\n"<<std::flush;
}
}
