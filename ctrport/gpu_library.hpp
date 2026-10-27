#include "settings/renderer_startup.hpp"
#include "retail_profiles.hpp"
#include "save_import.hpp"
// SPDX-License-Identifier: GPL-2.0-only
// Optional GPU library (ctrport/gpu, build/CTRPort-gpu/ctr_gpu.dll): shadPS4 v0.18.0's
// Liverpool command processor, Vulkan renderer, GnmDriver, VideoOut and kernel event
// queues behind a C interface (ctrport/gpu/ctr_gpu.h). Enabled with CTR_GPU_LIBRARY=1;
// then every guest import of libSceGnmDriver and libSceVideoOut, and the libkernel event
// queue functions, bind to the library instead of ctrport's own graphics path.
// Imports the library does not provide keep their ctrport binding or missing trap.
// CTR_GPU_BACKEND=d3d11 selects ctr_d3d.dll (ctrport/d3d) instead: the engine-state Direct3D 11
// backend with the same C interface and settings ABIs, which needs no shadPS4 code.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "missing_import.hpp"
#include "gpu_hooks.hpp"
#include "kernel_file.hpp"
#include "common_dialog.hpp"

namespace ctrport {
struct CtrGpuConfigV1 {
    std::uint32_t version;
    const char* title;
    const char* serial;
    const char* user_dir;
    std::uint32_t sdk_version;
    std::uint32_t psf_attributes;
    std::int32_t width, height;
};
struct GpuLibrary {
    HMODULE module=nullptr;
    int (*init)(const CtrGpuConfigV1*)=nullptr;
    std::uintptr_t (*resolve)(const char*,const char*)=nullptr;
    void (*map_memory)(std::uint64_t,std::uint64_t)=nullptr;
    void (*unmap_memory)(std::uint64_t,std::uint64_t)=nullptr;
    void (*invalidate_memory)(std::uint64_t,std::uint64_t)=nullptr;
    unsigned (*symbol_count)()=nullptr;
    std::uint64_t (*flip_count)()=nullptr;
    std::uint64_t (*submit_count)()=nullptr;
};
inline GpuLibrary gpu_library;
inline bool gpu_backend_d3d11(){
    static const bool selected=[]{char value[8]{};return GetEnvironmentVariableA("CTR_GPU_BACKEND",value,sizeof(value))==5 && std::string(value)=="d3d11";}();
    return selected;
}
inline bool gpu_library_requested(){
    char value[4]{};return gpu_backend_d3d11() || (GetEnvironmentVariableA("CTR_GPU_LIBRARY",value,sizeof(value))==1 && value[0]=='1');
}
inline bool gpu_library_active(){return gpu_library.module!=nullptr;}
// CPU writes to guest memory done by ctrport services (file reads) must reach the
// GPU caches and lift page tracking before the write, as shadPS4's ReadFile does.
inline void gpu_invalidate_guest(const void* address,std::uint64_t bytes){
    if(gpu_library.invalidate_memory && address && bytes)gpu_library.invalidate_memory(reinterpret_cast<std::uint64_t>(address),bytes);
}
inline void gpu_map_guest(std::uint64_t address,std::uint64_t bytes){
    if(gpu_library.map_memory && bytes)gpu_library.map_memory(address,bytes);
}
inline std::wstring gpu_library_path;
inline void load_gpu_library(std::uint32_t sdk_version){
    settings::prepare_renderer_environment();
    const auto candidates=gpu_backend_d3d11()?std::vector<const wchar_t*>{L"ctr_d3d.dll",L"build/CTRPort-d3d/ctr_d3d.dll"}
                                             :std::vector<const wchar_t*>{L"ctr_gpu.dll",L"build/CTRPort-gpu/ctr_gpu.dll"};
    for (const wchar_t* candidate : candidates) {
        gpu_library.module = LoadLibraryW(candidate);
        if (gpu_library.module) { gpu_library_path = candidate; break; }
    }
    if(!gpu_library.module)throw std::runtime_error(gpu_backend_d3d11()?"CTR_GPU_BACKEND=d3d11 but ctr_d3d.dll did not load (searched . and build/CTRPort-d3d/)"
                                                       :"CTR_GPU_LIBRARY=1 but ctr_gpu.dll did not load (searched . and build/CTRPort-gpu/)");
    settings::confirm_renderer_extensions(gpu_library.module);
    auto get=[&](const char* name){auto* p=GetProcAddress(gpu_library.module,name);if(!p)throw std::runtime_error(std::string("ctr_gpu export missing: ")+name);return p;};
    gpu_library.init=reinterpret_cast<int(*)(const CtrGpuConfigV1*)>(get("ctr_gpu_init"));
    gpu_library.resolve=reinterpret_cast<std::uintptr_t(*)(const char*,const char*)>(get("ctr_gpu_resolve"));
    gpu_library.map_memory=reinterpret_cast<void(*)(std::uint64_t,std::uint64_t)>(get("ctr_gpu_map_memory"));
    gpu_library.unmap_memory=reinterpret_cast<void(*)(std::uint64_t,std::uint64_t)>(get("ctr_gpu_unmap_memory"));
    gpu_library.invalidate_memory=reinterpret_cast<void(*)(std::uint64_t,std::uint64_t)>(get("ctr_gpu_invalidate_memory"));
    gpu_library.symbol_count=reinterpret_cast<unsigned(*)()>(get("ctr_gpu_symbol_count"));
    gpu_library.flip_count=reinterpret_cast<std::uint64_t(*)()>(get("ctr_gpu_flip_count"));
    gpu_library.submit_count=reinterpret_cast<std::uint64_t(*)()>(get("ctr_gpu_submit_count"));
    gpu_fault_hook=reinterpret_cast<int(*)(void*)>(get("ctr_gpu_handle_fault"));
    int window_width = 1280;
    int window_height = 720;
    char env_dim[32]{};
    if (GetEnvironmentVariableA("CTR_GPU_WIDTH", env_dim, sizeof(env_dim)) > 0) {
        const int val = std::atoi(env_dim);
        if (val > 0) window_width = val;
    }
    if (GetEnvironmentVariableA("CTR_GPU_HEIGHT", env_dim, sizeof(env_dim)) > 0) {
        const int val = std::atoi(env_dim);
        if (val > 0) window_height = val;
    }
    const CtrGpuConfigV1 config{1,"Crash Team Racing Nitro-Fueled (CTRPort)","CUSA14876","runtime/user/gpu",sdk_version,0,window_width,window_height};
    CreateDirectoryW(L"runtime/user/gpu",nullptr);
    const int status=gpu_library.init(&config);
    if(status)throw std::runtime_error("ctr_gpu_init failed with "+std::to_string(status));
    std::printf("{\"scope\":\"gpu_library_loaded\",\"library\":\"%ls\",\"sdk_version\":%u,\"registered_symbols\":%u}\n",gpu_library_path.c_str(),sdk_version,gpu_library.symbol_count());std::fflush(stdout);
}
// Rebinds the guest imports that the library provides. Every rebinding is reported.
inline void route_gpu_imports(const std::vector<ImageMetadata>& metadata,std::vector<ObjectBinding>& services){
    unsigned routed=0,replaced=0;std::vector<std::string> seen;
    for(const auto& image:metadata)for(const auto& symbol:image.imports){
        if(symbol.type!=2)continue;
        const bool graphics=symbol.library=="libSceGnmDriver" || symbol.library=="libSceVideoOut";
        const bool kernel=symbol.library=="libkernel" || symbol.library=="libScePosix";
        if(!graphics && !kernel)continue;
        const auto address=gpu_library.resolve(symbol.nid.c_str(),graphics?symbol.library.c_str():"libkernel");
        if(!address)continue;
        const auto key=symbol.nid+"#"+symbol.library+"#"+symbol.module;
        bool duplicate=false;for(const auto& k:seen)if(k==key)duplicate=true;
        if(duplicate)continue;seen.push_back(key);
        for(auto it=services.begin();it!=services.end();)
            if(it->nid==symbol.nid && it->library==symbol.library && it->module==symbol.module){it=services.erase(it);++replaced;}else ++it;
        services.push_back({symbol.nid,symbol.library,symbol.module,reinterpret_cast<const void*>(address)});++routed;
        std::printf("{\"scope\":\"gpu_library_binding\",\"nid\":\"%s\",\"library\":\"%s\",\"module\":\"%s\"}\n",symbol.nid.c_str(),symbol.library.c_str(),symbol.module.c_str());
    }
    std::printf("{\"scope\":\"gpu_library_routing\",\"routed_imports\":%u,\"replaced_ctrport_bindings\":%u}\n",routed,replaced);std::fflush(stdout);
}
// ctr_d3d reads the engine's render state from the eboot (the image with the largest executable
// span): its address and SHA256 select the recorded engine addresses.
template<class Images,class Metadata>
inline void configure_gpu_backend(const Images& images,const Metadata& metadata){
    if(!gpu_backend_d3d11())return;
    const auto set=reinterpret_cast<int(*)(void*,std::uint64_t,const char*)>(GetProcAddress(gpu_library.module,"ctr_d3d_set_eboot"));
    if(!set)throw std::runtime_error("ctr_d3d export missing: ctr_d3d_set_eboot");
    std::size_t best=0;std::uint64_t best_size=0,extent=0;
    for(std::size_t i=0;i<metadata.size();++i)for(const auto& s:metadata[i].executable_spans)if(s.second-s.first>best_size){best_size=s.second-s.first;best=i;}
    for(const auto& s:metadata[best].spans)if(s.second>extent)extent=s.second;
    const int status=set(images[best].get(),extent,metadata[best].sha256.c_str());
    if(status)throw std::runtime_error("STOP: Direct3D 11 does not support this game version (eboot "+metadata[best].sha256+", status "+std::to_string(status)+
                                       "): choose Vulkan as the renderer in Settings");
    std::printf("{\"scope\":\"gpu_backend_d3d11\",\"eboot_sha256\":\"%s\",\"image_extent\":%llu}\n",metadata[best].sha256.c_str(),static_cast<unsigned long long>(extent));std::fflush(stdout);
}
// ctr_hle.dll (ctrport/hle): system library functions written in the guest SysV ABI.
// Loaded together with the GPU library; every import it provides is bound to it.
inline HMODULE hle_library=nullptr;
inline std::wstring hle_library_path;
inline void route_hle_imports(const std::vector<ImageMetadata>& metadata,std::vector<ObjectBinding>& services){
    for (const wchar_t* candidate : {L"ctr_hle.dll", L"build/CTRPort-gpu/ctr_hle.dll"}) {
        hle_library = LoadLibraryW(candidate);
        if (hle_library) { hle_library_path = candidate; break; }
    }
    if(!hle_library)throw std::runtime_error("ctr_hle.dll did not load (searched . and build/CTRPort-gpu/)");
    const auto resolve=reinterpret_cast<std::uintptr_t(*)(const char*,const char*)>(GetProcAddress(hle_library,"ctr_hle_resolve"));
    const auto count=reinterpret_cast<unsigned(*)()>(GetProcAddress(hle_library,"ctr_hle_count"));
    if(!resolve || !count)throw std::runtime_error("ctr_hle exports missing");
    // Host services for ctr_hle: save data mount points in ctrport's file system.
    struct CtrHleHostV1{std::uint32_t version;const char* title_id;int(*mount)(const char*,const char*);int(*unmount)(const char*);};
    // Save data under the running game's title: EU CUSA14876 or US CUSA13795 (retail_profiles.hpp).
    static const char* const title=[&]{
        for(const auto& m:metadata) if(const auto* profile=retail_profile(m.sha256)) return profile->title_id;
        return "CUSA14876";
    }();
    save_import::run(title);
    static const CtrHleHostV1 host{1,title,
        [](const char* point,const char* root)->int{try{kernel_files.mount(point,std::filesystem::u8path(root));return 0;}catch(const std::exception& e){std::fprintf(stderr,"ctr_hle mount %s failed: %s\n",point,e.what());return -1;}},
        [](const char* point)->int{kernel_files.unmount(point);return 0;}};
    const auto set_host=reinterpret_cast<int(*)(const CtrHleHostV1*)>(GetProcAddress(hle_library,"ctr_hle_set_host"));
    if(!set_host || set_host(&host))throw std::runtime_error("ctr_hle host services rejected");
    const auto set_invitation_owner = reinterpret_cast<void(*)(int(*)(int))>(GetProcAddress(hle_library,"ctr_hle_invitation_set_owner"));
    if (set_invitation_owner) set_invitation_owner([](int op)->int {
        if (op == 0) return common_dialog.ready();
        if (op == 1) return common_dialog.claim();
        if (op == 2) { common_dialog.release(); return 1; }
        return 0;
    });
    unsigned routed=0,replaced=0;std::vector<std::string> seen;
    for(const auto& image:metadata)for(const auto& symbol:image.imports){
        if(symbol.type!=2)continue;
        const auto address=resolve(symbol.nid.c_str(),symbol.library.c_str());
        if(!address)continue;
        const auto key=symbol.nid+"#"+symbol.library+"#"+symbol.module;
        bool duplicate=false;for(const auto& k:seen)if(k==key)duplicate=true;
        if(duplicate)continue;seen.push_back(key);
        for(auto it=services.begin();it!=services.end();)
            if(it->nid==symbol.nid && it->library==symbol.library && it->module==symbol.module){it=services.erase(it);++replaced;}else ++it;
        services.push_back({symbol.nid,symbol.library,symbol.module,reinterpret_cast<const void*>(address)});++routed;
        std::printf("{\"scope\":\"hle_library_binding\",\"nid\":\"%s\",\"library\":\"%s\",\"module\":\"%s\"}\n",symbol.nid.c_str(),symbol.library.c_str(),symbol.module.c_str());
    }
    std::printf("{\"scope\":\"hle_library_routing\",\"registered_functions\":%u,\"routed_imports\":%u,\"replaced_ctrport_bindings\":%u}\n",count(),routed,replaced);std::fflush(stdout);
}
} // namespace ctrport
