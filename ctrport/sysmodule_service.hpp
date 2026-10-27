// SPDX-License-Identifier: GPL-2.0-only
// New runtime state implementation. Public module0xba identity and loader contract:
// pinned shadPS4 files/revision/license in ctrport_sysmodule_random_contract.json.
#pragma once
#include <mutex>
#include <stdexcept>
#include <functional>
namespace ctrport {
// Genuine retail module initialization; no successful binding without execution.
class OriginalModuleState {
    enum class Phase {absent,ready,initializing,active,failed};
    std::mutex lock;Phase phase=Phase::absent;std::function<int()> initializer;int failure=0;
public:
    void configure(std::function<int()> callback){
        std::lock_guard<std::mutex> guard(lock);
        if(phase!=Phase::absent || !callback)throw std::runtime_error("Original module configuration invalid");
        initializer=std::move(callback);phase=Phase::ready;
    }
    int load(){
        std::function<int()> callback;
        {
            std::lock_guard<std::mutex> guard(lock);
            if(phase==Phase::active)return 0;
            if(phase==Phase::failed){if(failure)return failure;throw std::runtime_error("Original module initializer previously threw");}
            if(phase!=Phase::ready)throw std::runtime_error("Original module absent or recursive/concurrent initialization");
            callback=initializer;phase=Phase::initializing;
        }
        int result;
        try{result=callback();}catch(...){std::lock_guard<std::mutex> guard(lock);phase=Phase::failed;throw;}
        std::lock_guard<std::mutex> guard(lock);failure=result;phase=result?Phase::failed:Phase::active;return result;
    }
};
inline OriginalModuleState nptoolkit_module_state;
class RandomModuleState {
    std::mutex lock;
    bool exports_ready=false;
    bool active=false;
public:
    void configure_verified_exports(bool verified) {
        std::lock_guard<std::mutex> guard(lock);
        if(active) throw std::runtime_error("Cannot reconfigure active Random module");
        exports_ready=verified;
    }
    int load(unsigned short id) {
        std::lock_guard<std::mutex> guard(lock);
        if(id!=0xba) throw std::runtime_error("System module id lacks concrete implementation");
        if(!exports_ready) throw std::runtime_error("Random module exports not verified");
        active=true;
        return 0;
    }
    bool loaded() {std::lock_guard<std::mutex> guard(lock);return active;}
};
inline RandomModuleState random_module_state;
inline std::mutex save_module_lock;
inline bool save_module_exports_ready=false,save_module_active=false;
inline int load_save_dialog_module() {
    std::lock_guard<std::mutex> guard(save_module_lock);
    if(!save_module_exports_ready) throw std::runtime_error("SaveDialog lifecycle exports not verified");
    save_module_active=true;return 0;
}
}
