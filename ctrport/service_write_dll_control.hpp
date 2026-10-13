// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_write_host.hpp"
#include "service_integer_dispatch.hpp"
namespace ctrport {
inline int service_write_dll_control(const std::filesystem::path& build,bool fail){
    namespace fs=std::filesystem;const auto root=fs::path("runtime/user/tests/service_write_dll");
    fs::create_directories(root);KernelFiles files;files.configure(root);files.configure_user(root);
    const auto fd=files.open("/data/bytes",0x602,0777);
    const std::string pin="ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1";
    ServiceImportRecord record{pin,pin,"4wSze92BhLI","libkernel","libkernel",8,2};
    const auto token=integer_dispatch_records.add(record,pin,pin,16);
    ServiceIntegerThunk thunk(token);
    const auto write=make_write_host_api(files);CtrHostApi host{};
    host.size=sizeof(host);host.version=1;host.context=const_cast<CtrWriteHostApi*>(&write);
    auto call=[&](int descriptor,const void* bytes,std::uint64_t count){
        return ctr_call_original_integer3(thunk.target(),static_cast<std::uint64_t>(descriptor),
            reinterpret_cast<std::uint64_t>(bytes),count);
    };
    auto pending=std::async(std::launch::async,[&]{return call(fd,"abc",3);});
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(integer_dispatch_generations.waiting_count()!=1){
        if(std::chrono::steady_clock::now()>end)ExitProcess(5);std::this_thread::yield();
    }
    load_service_generation(integer_dispatch_generations,build,build/L"service_write_1.dll",host,pin,1);
    if(pending.get()!=3)throw std::runtime_error("Resumed real write count differs");
    if(fail){(void)call(fd,"x",16ULL*1024*1024+1);throw std::runtime_error("Unsupported write returned");}
    load_service_generation(integer_dispatch_generations,build,build/L"service_write_2.dll",host,pin,2);
    if(call(fd,"def",3)!=3 || call(fd,nullptr,0)!=0 || call(1,nullptr,0)!=0 || call(2,nullptr,0)!=0 ||
       call(999,"x",1)!=static_cast<std::uint64_t>(static_cast<std::int64_t>(int(0x80020009U))))
        throw std::runtime_error("Reloaded real write count/error differs");
    if(files.close(fd))throw std::runtime_error("Real write close differs");
    std::ifstream input(root/"bytes",std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)),{});
    if(actual!="abcdef")throw std::runtime_error("Reloaded real write actual bytes differ");
    integer_dispatch_generations.shutdown();integer_dispatch_records.retire(token);
    // DLLs only hold callbacks/context; no workers/destructors use host state.
    return 0;
}
}
