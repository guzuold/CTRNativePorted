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
inline int original_module_state_control(){
    unsigned rejected=0,calls=0;OriginalModuleState state;
    try{state.load();}catch(const std::runtime_error&){++rejected;}
    state.configure([&]{++calls;return 0;});
    if(state.load() || state.load() || calls!=1)throw std::runtime_error("Original initializer repeat differs");
    try{state.configure([]{return 0;});}catch(const std::runtime_error&){++rejected;}
    OriginalModuleState failed;unsigned failures=0;failed.configure([&]{++failures;return -7;});
    if(failed.load()!=-7 || failed.load()!=-7 || failures!=1)throw std::runtime_error("Initializer failure was hidden/retried");
    OriginalModuleState recursive;recursive.configure([&]{return recursive.load();});
    try{recursive.load();}catch(const std::runtime_error&){++rejected;}
    try{recursive.load();}catch(const std::runtime_error&){++rejected;}
    if(rejected!=4)throw std::runtime_error("Original module negative controls differ");
    std::cout<<"{\"scope\":\"synthetic_original_module_lifecycle\",\"initializer_called_once\":true,\"failure_preserved\":true,\"negative_rejections\":4,\"retail_execution\":false}\n";return 0;
}
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
inline int sysmodule_state_control() {
    RandomModuleState state;
    unsigned rejected=0;
    try {state.load(0xba);} catch(const std::runtime_error&) {++rejected;}
    if(state.loaded()) throw std::runtime_error("Unavailable module became active");
    state.configure_verified_exports(true);
    try {state.load(0xbb);} catch(const std::runtime_error&) {++rejected;}
    if(state.loaded()) throw std::runtime_error("Unknown module became active");
    if(state.load(0xba) || !state.loaded() || state.load(0xba)) throw std::runtime_error("Random activation state failed");
    try {state.configure_verified_exports(false);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=3) throw std::runtime_error("Module negatives differ");
    std::cout<<"{\"scope\":\"synthetic_module_state\",\"module_id\":186,\"activation_and_repeat\":true,\"negative_rejections\":3,\"runtime_bound\":false}\n";
    return 0;
}
}
