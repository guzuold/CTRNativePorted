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
}
