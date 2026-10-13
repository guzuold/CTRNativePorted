// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_slot_admission.hpp"
#include "service_write_dll_control.hpp"
#include "missing_import.hpp"
namespace ctrport {
inline int service_retail_slot_control(const char* filename,const std::filesystem::path& build){
    const std::string pin="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1";
    ImageMetadata metadata{};std::vector<MappedImage> images;
    images.push_back(load_retail_image(filename,false,&metadata));
    if(metadata.sha256!=pin)throw std::runtime_error("Write slot control requires retail1.00");
    // The public write import belongs to retail Fios2, not the eboot itself.
    images.clear();
    const auto library=std::filesystem::path(filename).parent_path()/"sce_module"/"libSceFios2.prx";
    images.push_back(load_retail_image(library.string().c_str(),false,&metadata));
    if(metadata.sha256!="65d518fa10c13d122346113fa46616564dcab439e42cc43df64e69767bb83d12")throw std::runtime_error("Retail Fios2 identity differs");
    const OriginalSymbol* selected=nullptr;
    for(const auto& symbol:metadata.imports)if(symbol.nid=="4wSze92BhLI" && symbol.library=="libkernel" && symbol.module=="libkernel"){
        if(selected)throw std::runtime_error("Retail write import ambiguous");selected=&symbol;
    }
    if(!selected)throw std::runtime_error("Retail write import absent");
    std::vector<ImageMetadata> all{metadata};
    const auto resident=reinterpret_cast<const void*>(&ctr_sysv_console_write);
    auto traps=install_missing_traps(images,all,{{"4wSze92BhLI","libkernel","libkernel",resident}});
    std::uint64_t extent=0;for(const auto& span:metadata.spans)if(span.second>extent)extent=span.second;
    ServiceImportRecord record{pin,metadata.sha256,selected->nid,selected->library,selected->module,selected->slot,2};
    const auto token=integer_dispatch_records.add(record,pin,metadata.sha256,extent);ServiceIntegerThunk thunk(token);
    admit_service_slot(images[0],metadata,pin,record,resident,thunk.target());
    std::uint64_t readback=0;std::memcpy(&readback,images[0].get()+record.slot,8);
    namespace fs=std::filesystem;const auto root=fs::path("runtime/user/tests/retail_write_slot");
    fs::create_directories(root);KernelFiles files;files.configure(root);files.configure_user(root);
    const int fd=files.open("/data/bytes",0x602,0777);
    auto pending=std::async(std::launch::async,[&]{return ctr_call_original_integer3(reinterpret_cast<void*>(readback),fd,reinterpret_cast<std::uint64_t>("abc"),3);});
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(integer_dispatch_generations.waiting_count()!=1){
        if(std::chrono::steady_clock::now()>end)ExitProcess(5);std::this_thread::yield();
    }
    const auto write=make_write_host_api(files);CtrHostApi host{};host.size=sizeof(host);host.version=1;
    host.context=const_cast<CtrWriteHostApi*>(&write);
    load_service_generation(integer_dispatch_generations,build,build/L"service_write_1.dll",host,pin,1);
    if(pending.get()!=3)throw std::runtime_error("Retail slot host control result differs");
    if(files.close(fd))throw std::runtime_error("Retail slot control close failed");
    std::ifstream input(root/"bytes",std::ios::binary);const std::string actual((std::istreambuf_iterator<char>(input)),{});
    if(actual!="abc")throw std::runtime_error("Retail slot control actual file differs");
    admit_service_slot(images[0],metadata,pin,record,thunk.target(),resident);
    integer_dispatch_generations.shutdown();integer_dispatch_records.retire(token);
    std::cout<<"{\"scope\":\"retail_mapped_write_slot_synthetic_call\",\"slot\":"<<record.slot
             <<",\"eboot_sha256\":"<<std::quoted(pin)<<",\"actual_bytes\":\"abc\",\"rollback\":true,\"original_guest_execution\":false}\n";
    return 0;
}
}
