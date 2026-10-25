// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "overrides.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

extern "C" std::uint64_t ctr_call_original_integer2(void*, std::uint64_t, std::uint64_t);
extern "C" std::uint64_t ctr_call_original_integer3(void*, std::uint64_t, std::uint64_t, std::uint64_t);
extern "C" void ctr_msvc_probe() { std::abort(); }
extern "C" void ctr_sysv_orbis_condition_broadcast() { std::abort(); }
extern "C" void ctr_thread_exit_frame() { std::abort(); }
namespace rg = ctrport::settings::retail_graphics;
namespace {
unsigned checks = 0, calls = 0;
void require(bool condition, const char* reason) { ++checks; if (!condition) throw std::runtime_error(reason); }
template<class F> void rejects(F f) { bool did = false; try { f(); } catch (const std::invalid_argument&) { did = true; } require(did, "expected rejection"); }
template<class T> void put(unsigned char* p, std::size_t offset, T value) { std::memcpy(p+offset, &value, sizeof(value)); }
template<class T> T get(const unsigned char* p, std::size_t offset) { T value{}; std::memcpy(&value,p+offset,sizeof(value)); return value; }
std::uint64_t allocation_boundary = 0;
LONG CALLBACK boundary(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || e->ContextRecord->Rip != allocation_boundary)
        return EXCEPTION_CONTINUE_SEARCH;
    static constexpr char marker[] = "RETAIL_GRAPHICS_ALLOCATION_BOUNDARY\n";
    DWORD written = 0;
    const auto ok = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), marker, sizeof(marker)-1, &written, nullptr);
    ExitProcess(ok && written == sizeof(marker)-1 ? 42 : 43);
}
struct Fixture {
    alignas(32) unsigned char source[0x100]{};
    alignas(32) unsigned char context[0x80]{};
    alignas(32) unsigned char device[0x600]{};
    alignas(32) unsigned char helper[0x80]{};
    alignas(32) unsigned char caches[4][0x50]{};
    alignas(32) unsigned char values[4][0x20]{};
    void reset(unsigned char* image, bool missing, bool linked, bool allocate = false) {
        std::memset(source, 0, sizeof(source)); std::memset(context, 0, sizeof(context));
        std::memset(device, 0, sizeof(device)); std::memset(helper, 0, sizeof(helper));
        std::memset(caches, 0, sizeof(caches)); std::memset(values, 0, sizeof(values));
        put(context,0x40,reinterpret_cast<std::uintptr_t>(device));
        put(device,0x5a8,reinterpret_cast<std::uintptr_t>(helper));
        // Allocation-boundary controls need a readable pool header to reach
        // the original allocator call. The allocator itself is not mapped.
        put(helper,0x48,reinterpret_cast<std::uintptr_t>(device));
        put(helper,0x50,reinterpret_cast<std::uintptr_t>(device));
        put(helper,0x6c,std::uint32_t(41));
        put(helper,0x70,std::uintptr_t(0x12345678)); // Sentinel never dereferenced by publishers.
        const std::size_t globals[]{0x2a29e28,0x2a29e30,0x2a29e38,0x2a60f08};
        for (unsigned i=0;i<4;++i) {
            put(caches[i],0,allocate ? std::uintptr_t(0) : reinterpret_cast<std::uintptr_t>(values[i]));
            put(caches[i],0x38,linked ? std::uintptr_t(0x87654321) : std::uintptr_t(0));
            put(image,globals[i], missing ? std::uintptr_t(0) : reinterpret_cast<std::uintptr_t>(caches[i]));
        }
    }
    std::vector<unsigned char> snapshot() const {
        std::vector<unsigned char> out;
        for (const auto& c : caches) out.insert(out.end(), c, c+sizeof(c));
        for (const auto& v : values) out.insert(out.end(), v, v+sizeof(v));
        out.insert(out.end(),helper,helper+sizeof(helper));
        return out;
    }
    void call(unsigned char* image, std::size_t function) {
        const auto before = std::vector<unsigned char>(source,source+sizeof(source));
        ctr_call_original_integer2(image+function,reinterpret_cast<std::uintptr_t>(source),reinterpret_cast<std::uintptr_t>(context));
        ++calls;
        require(std::memcmp(before.data(),source,sizeof(source))==0,"publisher mutated scene source");
    }
};
struct MotionFixture {
    alignas(32) unsigned char update[0x180]{}, camera[0x100]{}, renderer[0x260]{}, bundles[0x100]{};
    alignas(32) unsigned char first[0xc0]{}, second[0xc0]{}, list[0x40]{}, list_data[0x20]{};
    void reset(unsigned char* image, float exposure, bool cut, bool singular, int width) {
        std::memset(update,0,sizeof(update)); std::memset(camera,0,sizeof(camera)); std::memset(renderer,0,sizeof(renderer));
        std::memset(bundles,0,sizeof(bundles)); std::memset(first,0,sizeof(first)); std::memset(second,0,sizeof(second));
        std::memset(list,0,sizeof(list)); std::memset(list_data,0,sizeof(list_data));
        put(update,0x38,reinterpret_cast<std::uintptr_t>(list)); put(update,0x50,reinterpret_cast<std::uintptr_t>(bundles));
        put(bundles,0x80,reinterpret_cast<std::uintptr_t>(first)); put(bundles,0x88,reinterpret_cast<std::uintptr_t>(second));
        put(list,0x10,std::uint32_t(4)); put(list,0x20,reinterpret_cast<std::uintptr_t>(list_data));
        put(renderer,0x228,exposure); renderer[0x1d2]=cut?1:0;
        put(image,0x2a7f420,reinterpret_cast<std::uintptr_t>(renderer));
        put(camera,0x5c,width); put(camera,0x60,width/2);
        for(unsigned i=0;i<4;++i) {
            put(update,0xa0+i*20,1.0f); put(camera,0xb0+i*20,1.0f);
            if(!singular) put(camera,0x70+i*20,1.0f);
        }
    }
    void call(unsigned char* image) {
        ctr_call_original_integer2(image+0xaf23d0,reinterpret_cast<std::uintptr_t>(update),reinterpret_cast<std::uintptr_t>(camera));
        ++calls;
        require(get<std::uint32_t>(list,0xc)==1 && get<std::uint32_t>(first,8)==1,"motion list/refcount update lost");
        require(get<std::uintptr_t>(bundles,0x88)==reinterpret_cast<std::uintptr_t>(first),"motion frame buffer rotation lost");
        require(std::memcmp(update+0xa0,camera+0x70,64)==0,"motion history update lost");
    }
    std::vector<unsigned char> snapshot() const {
        std::vector<unsigned char> out;
        const std::pair<const unsigned char*,std::size_t> parts[]{{update,sizeof(update)},{camera,sizeof(camera)},
            {renderer,sizeof(renderer)},{bundles,sizeof(bundles)},{first,sizeof(first)},{second,sizeof(second)},
            {list,sizeof(list)},{list_data,sizeof(list_data)}};
        for(const auto& part:parts) out.insert(out.end(),part.first,part.first+part.second);
        return out;
    }
};
struct VignetteFixture {
    alignas(32) unsigned char update[0x160]{}, scene[0x500]{}, bundles[0x40]{}, bundle[0xc0]{};
    alignas(32) unsigned char handle[0x40]{}, viewport[0x60]{}, targets[0x20]{}, empty_list[0x20]{}, texture[0x60]{};
    alignas(32) unsigned char list[0x40]{}, list_data[0x20]{};
    std::uint64_t cookie=0x123456789abcdef;
    void reset(unsigned char* image,float strength,float width,int pixels) {
        std::memset(update,0,sizeof(update)); std::memset(scene,0,sizeof(scene)); std::memset(bundles,0,sizeof(bundles));
        std::memset(bundle,0,sizeof(bundle)); std::memset(handle,0,sizeof(handle)); std::memset(viewport,0,sizeof(viewport));
        std::memset(targets,0,sizeof(targets)); std::memset(empty_list,0,sizeof(empty_list)); std::memset(texture,0,sizeof(texture));
        std::memset(list,0,sizeof(list)); std::memset(list_data,0,sizeof(list_data));
        put(image,0x2807098,reinterpret_cast<std::uintptr_t>(&cookie));
        put(update,0x38,reinterpret_cast<std::uintptr_t>(list)); put(update,0x50,reinterpret_cast<std::uintptr_t>(bundles));
        put(bundles,0x20,reinterpret_cast<std::uintptr_t>(bundle));
        put(update,0x140,reinterpret_cast<std::uintptr_t>(handle)); put(handle,0x28,reinterpret_cast<std::uintptr_t>(viewport));
        put(viewport,8,std::uint32_t(5)); put(viewport,0x48,reinterpret_cast<std::uintptr_t>(targets));
        put(targets,0x10,reinterpret_cast<std::uintptr_t>(empty_list)); put(targets,0x18,reinterpret_cast<std::uintptr_t>(texture));
        put(texture,0x58,pixels); put(texture,0x5c,pixels/2);
        put(list,0x10,std::uint32_t(4)); put(list,0x20,reinterpret_cast<std::uintptr_t>(list_data));
        put(scene,0x468,strength); put(scene,0x46c,0.5f); put(scene,0x470,width);
    }
    void call(unsigned char* image) {
        const auto original_scene=std::vector<unsigned char>(scene,scene+sizeof(scene));
        ctr_call_original_integer2(image+0xaf1f90,reinterpret_cast<std::uintptr_t>(update),reinterpret_cast<std::uintptr_t>(scene)); ++calls;
        require(get<std::uint32_t>(list,0xc)==1 && get<std::uint32_t>(bundle,8)==1,"color bundle list/refcount changed");
        require(get<std::uint32_t>(viewport,8)==5,"viewport reference not released");
        require(std::memcmp(scene,original_scene.data(),sizeof(scene))==0,"color override mutated scene parameters");
    }
    std::vector<unsigned char> snapshot() const {
        std::vector<unsigned char> out(bundle,bundle+sizeof(bundle));
        out.insert(out.end(),list,list+sizeof(list)); out.insert(out.end(),list_data,list_data+sizeof(list_data));
        return out;
    }
};
struct DepthFixture {
    alignas(32) unsigned char update[0x260]{}, scene[0x500]{}, camera[0x80]{}, bundles[0x120]{}, bundle[0x80]{}, enabled[0x20]{};
    alignas(32) unsigned char list[0x40]{}, list_data[0x20]{}, override_data[0x60]{}, override_list[0x40]{}, override_entries[0x20]{}, override_entry[0x60]{};
    void reset(unsigned char* image,float fade,bool camera_override,int width) {
        std::memset(this,0,sizeof(*this));
        put(update,0x38,reinterpret_cast<std::uintptr_t>(list)); put(update,0x50,reinterpret_cast<std::uintptr_t>(bundles));
        put(update,0x240,reinterpret_cast<std::uintptr_t>(enabled)); put(bundles,0x100,reinterpret_cast<std::uintptr_t>(bundle));
        put(list,0x10,std::uint32_t(4)); put(list,0x20,reinterpret_cast<std::uintptr_t>(list_data));
        put(camera,0x5c,width); put(scene,0x4d8,fade);
        const float planes[]{1.0f,2.0f,10.0f,20.0f};
        std::memcpy(scene+0x4e0,planes,sizeof(planes)); std::memcpy(override_data+0x40,planes,sizeof(planes));
        put(bundle,0x6c,0.03f); put(bundle,0x70,0.06f);
        put(override_data,0x10,reinterpret_cast<std::uintptr_t>(override_list));
        put(override_list,0xc,std::uint32_t(1)); put(override_list,0x20,reinterpret_cast<std::uintptr_t>(override_entries));
        put(override_entries,0,reinterpret_cast<std::uintptr_t>(override_entry)); put(override_entry,0x50,std::uint32_t(3));
        put(image,0x2ab67f0,camera_override?reinterpret_cast<std::uintptr_t>(override_data):std::uintptr_t(0));
    }
    void call(unsigned char* image) {
        const auto original_scene=std::vector<unsigned char>(scene,scene+sizeof(scene));
        ctr_call_original_integer3(image+0xaf8da0,reinterpret_cast<std::uintptr_t>(update),reinterpret_cast<std::uintptr_t>(camera),reinterpret_cast<std::uintptr_t>(scene)); ++calls;
        require(get<std::uint32_t>(list,0xc)==1 && get<std::uint32_t>(bundle,8)==1,"depth bundle bookkeeping changed");
        require(std::memcmp(scene,original_scene.data(),sizeof(scene))==0,"depth override mutated scene");
    }
    std::vector<unsigned char> snapshot() const {
        std::vector<unsigned char> out(bundle,bundle+sizeof(bundle)); out.insert(out.end(),enabled,enabled+sizeof(enabled));
        out.insert(out.end(),list,list+sizeof(list)); out.insert(out.end(),list_data,list_data+sizeof(list_data)); return out;
    }
};
}
int wmain(int argc,wchar_t** argv) {
    constexpr std::size_t extent = 0x2b00000;
    unsigned char* image = nullptr;
    try {
        if (argc < 2 || argc > 3) return 2;
        std::ifstream input(argv[1],std::ios::binary);
        require(bool(input),"fixture open");
        image = static_cast<unsigned char*>(VirtualAlloc(nullptr,extent,MEM_RESERVE,PAGE_NOACCESS));
        require(image!=nullptr,"reserve image");
        std::vector<std::pair<std::size_t,std::vector<unsigned char>>> originals;
        std::uint32_t count=0; input.read(reinterpret_cast<char*>(&count),4); require(count==19,"fixture span count");
        std::vector<std::size_t> code_pages;
        for (unsigned i=0;i<count;++i) {
            std::uint32_t address=0,size=0;
            input.read(reinterpret_cast<char*>(&address),4); input.read(reinterpret_cast<char*>(&size),4);
            require(address<extent && size<4096 && address+size<extent,"fixture extent");
            std::vector<unsigned char> body(size); input.read(reinterpret_cast<char*>(body.data()),size); require(bool(input),"fixture bytes");
            const std::size_t page=address&~std::size_t(4095);
            if (std::find(code_pages.begin(),code_pages.end(),page)==code_pages.end()) {
                require(VirtualAlloc(image+page,4096,MEM_COMMIT,PAGE_READWRITE)==image+page,"commit code");
                code_pages.push_back(page);
            }
            std::memcpy(image+address,body.data(),size); originals.push_back({address,std::move(body)});
        }
        for (const std::size_t page : {std::size_t(0x2a29000),std::size_t(0x2a60000),std::size_t(0x2a7f000),std::size_t(0x2807000),std::size_t(0x2ab6000)})
            require(VirtualAlloc(image+page,4096,MEM_COMMIT,PAGE_READWRITE)==image+page,"commit globals");
        auto protect=[&](bool write) {
            for (const auto page : code_pages) { DWORD old=0; require(VirtualProtect(image+page,4096,write?PAGE_READWRITE:PAGE_EXECUTE_READ,&old)!=0,"protection"); }
            require(FlushInstructionCache(GetCurrentProcess(),image,extent)!=0,"instruction flush");
        };
        Fixture fixture;
        if (argc==3) {
            const std::wstring mode=argv[2];
            require(mode==L"ssr-allocate" || mode==L"bloom-allocate","unknown mode");
            rg::apply(image,extent,rg::retail_sha256,{true,true});
            fixture.reset(image,false,false,true);
            allocation_boundary=reinterpret_cast<std::uint64_t>(image+0x14adc10);
            require(AddVectoredExceptionHandler(1,boundary)!=nullptr,"observer install");
            protect(false); fixture.call(image,mode==L"ssr-allocate"?0x36d430:0x963ce0);
            throw std::runtime_error("allocator boundary unexpectedly returned");
        }
        require(rg::apply(nullptr,0,"unused",{}).changed_bytes==0,"original must be no-op");
        rejects([&]{rg::apply(image,extent,"wrong",{true,true});});
        rejects([&]{rg::apply(image,0x36d470,rg::retail_sha256,{true,false});});
        const auto old=image[0x963d00]; image[0x963d00]^=1;
        rejects([&]{rg::apply(image,extent,rg::retail_sha256,{true,true});});
        require(std::memcmp(image+0x36d470,rg::descriptors[0].original.data(),4)==0,"partial application on failed preflight");
        image[0x963d00]=old;
        rg::Options all{}; all.fill(true);
        for(const auto& descriptor:rg::descriptors) {
            const auto saved=image[descriptor.site_rva]; image[descriptor.site_rva]^=1;
            rejects([&]{rg::apply(image,extent,rg::retail_sha256,all);});
            for(const auto& other:rg::descriptors) if(other.site_rva!=descriptor.site_rva)
                require(std::memcmp(image+other.site_rva,other.original.data(),other.length)==0,"failed batch preflight changed another control");
            image[descriptor.site_rva]=saved;
            rg::Options one{}; one[static_cast<std::size_t>(&descriptor-rg::descriptors.data())]=true;
            rejects([&]{rg::apply(image,descriptor.site_rva+descriptor.length-1,rg::retail_sha256,one);});
        }
        for (unsigned setting=0;setting<2;++setting) for (bool missing : {false,true}) for (bool linked : {false,true})
            for (unsigned variant=0;variant<4;++variant) {
                auto seed=[&](bool neutral) {
                    fixture.reset(image,missing,linked);
                    const std::uint32_t patterns[]{0x3fc00000,0xc0100000,0x7fc12345,0x80000000};
                    for (unsigned n=0;n<8;++n) put(fixture.source,0x20+n*4,patterns[(n+variant)%4]);
                    fixture.source[0x40]=neutral?0:1;
                    if (setting==1) { put(fixture.source,0x18,patterns[variant]); put(fixture.source,0x1c,patterns[(variant+1)%4]); put(fixture.source,0x20,neutral?0u:0x3fc00000u); }
                };
                seed(false); protect(false); fixture.call(image,rg::descriptors[setting].function_rva);
                const auto active=fixture.snapshot();
                seed(true); fixture.call(image,rg::descriptors[setting].function_rva);
                const auto neutral=fixture.snapshot();
                if (!missing) require(active!=neutral,"positive control does not distinguish off value");
                protect(true); rg::Options options{}; options[setting]=true;
                const auto applied=rg::apply(image,extent,rg::retail_sha256,options);
                require(applied.controls_applied==1 && applied.changed_bytes==(setting==0?4u:1u),"apply accounting");
                seed(false); protect(false); fixture.call(image,rg::descriptors[setting].function_rva);
                require(fixture.snapshot()==neutral,"patched execution differs from original neutral input");
                // Repeated same-value publication must preserve original dirty sequence semantics.
                fixture.call(image,rg::descriptors[setting].function_rva); const auto repeated=fixture.snapshot();
                protect(true); rg::rollback(image,extent,rg::retail_sha256,options);
                seed(true); protect(false); fixture.call(image,rg::descriptors[setting].function_rva); fixture.call(image,rg::descriptors[setting].function_rva);
                require(fixture.snapshot()==repeated,"cache repeat semantics diverged");
                seed(false); fixture.call(image,rg::descriptors[setting].function_rva); require(fixture.snapshot()==active,"restored execution differs"); protect(true);
            }
        MotionFixture motion;
        for(float exposure : {-1.0f,0.0f,0.01f,0.1f}) for(bool singular : {false,true}) for(int width : {1280,3840}) {
            motion.reset(image,exposure,false,singular,width); protect(false); motion.call(image); const auto active=motion.snapshot();
            motion.reset(image,exposure,true,singular,width); motion.call(image); const auto neutral=motion.snapshot();
            require(get<float>(motion.first,0x60)==0.0f,"original camera cut did not zero exposure");
            if(exposure!=0.0f) require(active!=neutral,"motion positive control does not distinguish exposure");
            protect(true); rg::apply(image,extent,rg::retail_sha256,{false,false,true});
            motion.reset(image,exposure,false,singular,width); protect(false); motion.call(image);
            require(motion.snapshot()==neutral,"motion override differs from original camera-cut execution");
            protect(true); rg::rollback(image,extent,rg::retail_sha256,{false,false,true});
            motion.reset(image,exposure,false,singular,width); protect(false); motion.call(image);
            require(motion.snapshot()==active,"motion rollback differs from original execution"); protect(true);
        }
        VignetteFixture vignette;
        for(float strength : {0.0f,0.25f,1.0f}) for(float width : {0.0f,0.1f,1.0f}) for(int pixels : {1280,3840}) {
            vignette.reset(image,strength,width,pixels); protect(false); vignette.call(image); const auto active=vignette.snapshot();
            vignette.reset(image,0.0f,width,pixels); vignette.call(image); const auto neutral=vignette.snapshot();
            require(get<float>(vignette.bundle,0xa0)==0.0f,"original vignette strength not neutral");
            if(strength!=0.0f) require(active!=neutral,"vignette positive control not distinct");
            protect(true);
            const auto applied=rg::apply(image,extent,rg::retail_sha256,{false,false,false,true});
            require(applied.controls_applied==1 && applied.changed_bytes==7,"vignette apply accounting");
            vignette.reset(image,strength,width,pixels); protect(false); vignette.call(image); require(vignette.snapshot()==neutral,"vignette differs from original neutral-strength execution");
            protect(true); rg::rollback(image,extent,rg::retail_sha256,{false,false,false,true});
            vignette.reset(image,strength,width,pixels); protect(false); vignette.call(image); require(vignette.snapshot()==active,"vignette restore differs"); protect(true);
        }
        DepthFixture depth;
        for(float fade : {0.0f,0.5f,-0.5f,1.0f}) for(bool camera_override : {false,true}) for(int width : {1280,3840}) {
            depth.reset(image,fade,camera_override,width); protect(false); depth.call(image); const auto active=depth.snapshot();
            depth.reset(image,0.0f,false,width); depth.call(image); const auto neutral=depth.snapshot();
            require(depth.enabled[0xc]==0 && get<float>(depth.bundle,0x68)==0.0f,"original depth zero fade not disabled");
            if(fade!=0.0f || camera_override) require(active!=neutral,"depth positive control not distinct");
            protect(true);
            const auto applied=rg::apply(image,extent,rg::retail_sha256,{false,false,false,false,true});
            require(applied.controls_applied==1 && applied.changed_bytes==5,"depth apply accounting");
            depth.reset(image,fade,camera_override,width); protect(false); depth.call(image); require(depth.snapshot()==neutral,"depth differs from original zero-fade execution");
            protect(true); rg::rollback(image,extent,rg::retail_sha256,{false,false,false,false,true});
            depth.reset(image,fade,camera_override,width); protect(false); depth.call(image); require(depth.snapshot()==active,"depth restore differs"); protect(true);
        }
        rg::apply(image,extent,rg::retail_sha256,{true,true,true,true,true});
        rejects([&]{rg::apply(image,extent,rg::retail_sha256,{true,true});});
        image[0x963d00]^=1; rejects([&]{rg::rollback(image,extent,rg::retail_sha256,{true,true});});
        require(std::memcmp(image+0x36d470,rg::descriptors[0].disabled.data(),4)==0,"partial rollback on failed preflight");
        image[0x963d00]^=1; rg::rollback(image,extent,rg::retail_sha256,{true,true,true,true,true});
        for(const auto& item:originals) require(std::memcmp(image+item.first,item.second.data(),item.second.size())==0,"code not restored exactly");
        VirtualFree(image,0,MEM_RELEASE);
        std::cout<<"RETAIL_GRAPHICS_CPU_VERIFIED checks="<<checks<<" original_cpu_calls="<<calls<<" visual_verified=false\n";
        return 0;
    } catch(const std::exception& e) { if(image) VirtualFree(image,0,MEM_RELEASE); std::cerr<<e.what()<<"\n"; return 1; }
}
