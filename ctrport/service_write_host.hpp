// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_write_api.h"
#include "kernel_file.hpp"
#include "console_output.hpp"
namespace ctrport {
inline std::uint32_t service_host_write(void* context,std::int32_t fd,const void* bytes,
    std::uint64_t count,std::int64_t* result){
    // result is a trusted host/DLL output, never a guest output argument.
    if(!context || !result)return 1;
    try{*result=(fd==1 || fd==2)?console_write(fd,bytes,count):kernel_write(*static_cast<KernelFiles*>(context),fd,bytes,count);return 0;}
    catch(const std::exception&){return 1;}
}
inline CtrWriteHostApi make_write_host_api(KernelFiles& owner){
    return {sizeof(CtrWriteHostApi),1,&owner,&service_host_write};
}
inline int service_write_host_control(){
    namespace fs=std::filesystem;const auto root=fs::path("runtime/user/tests/service_write_host");
    fs::create_directories(root);KernelFiles files;files.configure(root);files.configure_user(root);
    const auto api=make_write_host_api(files);const auto fd=files.open("/data/bytes",0x602,0777);
    std::int64_t result=-1;
    if(api.write(api.context,fd,"abc",3,&result) || result!=3 ||
       api.write(api.context,fd,"def",3,&result) || result!=3 ||
       api.write(api.context,fd,nullptr,0,&result) || result!=0)
        throw std::runtime_error("Write callback real output differs");
    if(api.write(api.context,999,"x",1,&result) || result!=int(0x80020009U) ||
       api.write(api.context,fd,reinterpret_cast<void*>(1),1,&result) || result!=int(0x8002000eU))
        throw std::runtime_error("Write callback guest errors differ");
    if(api.write(api.context,1,nullptr,0,&result) || result!=0 ||
       api.write(api.context,2,nullptr,0,&result) || result!=0)
        throw std::runtime_error("Console descriptor routing differs");
    result=123;
    if(api.write(nullptr,fd,"x",1,&result)!=1 || result!=123 ||
       api.write(api.context,fd,"x",1,nullptr)!=1 ||
       api.write(api.context,fd,"x",16ULL*1024*1024+1,&result)!=1 || result!=123)
        throw std::runtime_error("Unsupported write callback fabricated result");
    if(files.close(fd))throw std::runtime_error("Write callback descriptor close failed");
    std::ifstream input(root/"bytes",std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(input)),{});
    if(actual!="abcdef")throw std::runtime_error("Write callback actual file bytes differ");
    return 0;
}
}
