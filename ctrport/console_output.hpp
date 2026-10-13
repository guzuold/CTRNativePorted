// SPDX-License-Identifier: GPL-2.0-only
// Own implementation; contract: shadPS4 e3ce810f debug.cpp and file_system.cpp.
#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include "kernel_file.hpp"
namespace ctrport {
inline std::mutex console_output_lock;
inline std::int64_t console_write(int fd,const void* buffer,std::uint64_t size) {
    if(fd!=1 && fd!=2)throw std::runtime_error("Console write supports stdout/stderr only");
    if(size>1024*1024)throw std::runtime_error("Console write exceeds declared 1MiB bound");
    std::vector<char> bytes(static_cast<std::size_t>(size));SIZE_T read=0;
    if(size && (!buffer || !ReadProcessMemory(GetCurrentProcess(),buffer,bytes.data(),bytes.size(),&read) || read!=bytes.size()))return static_cast<int>(0x8002000eU);
    std::lock_guard<std::mutex> guard(console_output_lock);
    std::filesystem::create_directories("runtime/user/logs");
    std::ofstream log(fd==1?"runtime/user/logs/game.stdout.log":"runtime/user/logs/game.stderr.log",std::ios::binary|std::ios::app);
    if(!log)throw std::runtime_error("Console log open failed");
    if(size){log.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));log.flush();if(!log)throw std::runtime_error("Console log write failed");
        auto stream=fd==1?stdout:stderr;if(std::fwrite(bytes.data(),1,bytes.size(),stream)!=bytes.size() || std::fflush(stream))throw std::runtime_error("Console output failed");}
    return static_cast<std::int64_t>(size);
}
extern "C" std::int64_t ctr_console_write(int fd,const void* buffer,std::uint64_t size,std::uint64_t caller) {
    try{return fd==1 || fd==2 ? console_write(fd,buffer,size) : kernel_write(kernel_files,fd,buffer,size);}catch(const std::exception& error){std::fprintf(stderr,"NID=4wSze92BhLI library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
// Public write ABI differs from sceKernelWrite: -1 plus thread-local errno.
// Source: pinned shadPS4 file_system.cpp:304..335, GPL-2.0-or-later.
extern "C" int* ctr_guest_errno();
inline std::int64_t posix_write(KernelFiles& files,int fd,const void* buffer,std::uint64_t size,int& error){
 const auto result=(fd==1 || fd==2)?console_write(fd,buffer,size):kernel_write(files,fd,buffer,size);
 if(result>=0)return result;
 const auto code=static_cast<std::uint32_t>(result);
 if(code!=0x80020009U && code!=0x8002000eU && code!=0x8002001cU)throw std::runtime_error("Posix write error conversion unsupported");
 error=static_cast<int>(code&0xffffU);return -1;
}
extern "C" std::int64_t ctr_posix_write(int fd,const void* buffer,std::uint64_t size,std::uint64_t caller){
 try{return posix_write(kernel_files,fd,buffer,size,*ctr_guest_errno());}
 catch(const std::exception& e){std::fprintf(stderr,"NID=FxVZqBAA7ks library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_write();
inline int posix_write_control(){
 namespace fs=std::filesystem;const auto root=fs::path("runtime/user/tests/posix_write");fs::create_directories(root);
 KernelFiles files;files.configure(root);files.configure_user(root);auto fd=files.open("/data/bytes",0x602,0777);int error=123;
 if(fd<3 || posix_write(files,fd,"actual",6,error)!=6 || error!=123)return 1;
 if(posix_write(files,999,"x",1,error)!=-1 || error!=9)return 1;
 if(posix_write(files,fd,reinterpret_cast<void*>(1),1,error)!=-1 || error!=14)return 1;
 if(posix_write(files,fd,nullptr,0,error)!=0 || error!=14 || files.close(fd))return 1;
 std::ifstream in(root/"bytes",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});if(bytes!="actual")return 1;
 if(posix_write(files,1,nullptr,0,error)!=0 || error!=14 || posix_write(files,2,nullptr,0,error)!=0)return 1;
 std::cout<<"{\"scope\":\"posix_write_control\",\"real_file_bytes\":\"actual\",\"ebadf\":9,\"efault\":14,\"success_preserves_errno\":true}\n";return 0;
}
extern "C" void ctr_console_debug(void*,const char* text,std::uint64_t caller) {
    try {std::vector<char> bytes;for(std::size_t i=0;i<1024*1024;++i){char c=0;SIZE_T read=0;
        if(!text || !ReadProcessMemory(GetCurrentProcess(),text+i,&c,1,&read) || read!=1)throw std::runtime_error("Debug text unreadable");
        if(!c){console_write(1,bytes.data(),bytes.size());return;}bytes.push_back(c);}
        throw std::runtime_error("Debug text exceeds declared 1MiB bound");
    }catch(const std::exception& error){std::fprintf(stderr,"NID=9JYNqN6jAKI library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_console_write();
extern "C" void ctr_sysv_console_debug();
inline int console_output_control() {
    const char bytes[]={'C','T','R','\0','\n'};
    if(console_write(1,bytes,sizeof(bytes))!=sizeof(bytes) || console_write(2,bytes,sizeof(bytes))!=sizeof(bytes))return 1;
    ctr_console_debug(nullptr,"CTR kernel debug control\n",0);
    if(console_write(1,reinterpret_cast<void*>(1),1)!=static_cast<int>(0x8002000eU))return 1;
    bool rejected=false;try{console_write(3,bytes,1);}catch(const std::runtime_error&){rejected=true;}
    return rejected?0:1;
}
}
