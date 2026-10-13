// SPDX-License-Identifier: GPL-2.0-only
#include "shader_directory_pin.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <thread>
#include <atomic>
#include <vector>
int main(int argc,char** argv) {
    if(argc==4 && std::string(argv[3])=="global") {
        try {
            const auto root=std::filesystem::absolute(argv[2]);
            CtrVideo::InitializeShaderDumpSession(root);
            const auto* published=&CtrVideo::GetShaderDumpSession();
            std::atomic<unsigned> failures{0};
            std::vector<std::thread> readers;
            for(unsigned worker=0;worker<8;++worker) readers.emplace_back([&]{
                for(unsigned i=0;i<2000;++i)
                    if(&CtrVideo::GetShaderDumpSession()!=published) failures.fetch_add(1);
            });
            for(auto& reader:readers) reader.join();
            if(failures.load()!=0) throw std::runtime_error("publication identity mismatch");
            bool rejected=false;
            try{CtrVideo::InitializeShaderDumpSession(root);}catch(const std::logic_error&){rejected=true;}
            if(!rejected || &CtrVideo::GetShaderDumpSession()!=published) throw std::runtime_error("reinit changed session");
            auto output=published->Open(L"global.bin");
            const char bytes[]="global-session-control";
            output->Write(bytes,sizeof(bytes));output->Finish();output.reset();
            const auto file=root/L"runtime/user/shader/dumps/global.bin";
            HANDLE input=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
            if(input==INVALID_HANDLE_VALUE) throw std::runtime_error("global readback open failed");
            char actual[sizeof(bytes)]{};DWORD count=0;
            const bool ok=ReadFile(input,actual,sizeof(actual),&count,nullptr)!=0;
            CloseHandle(input);
            if(!ok || count!=sizeof(bytes) || std::string(actual)!=bytes) throw std::runtime_error("global readback mismatch");
            if(!DeleteFileW(file.c_str())) throw std::runtime_error("global file cleanup failed");
            return 0; // Process exit releases the deliberately process-lifetime directory pins.
        }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 8;}
    }
    if(argc!=2 && argc!=3) return 1;
    if(argc==3) {
        try { CtrVideo::ShaderDirectoryPin pin{std::filesystem::path(argv[2])}; return 5; }
        catch(const std::runtime_error& error) { return std::string(error.what())=="Shader directory handle is not an ordinary directory" ? 0 : 6; }
    }
    namespace fs=std::filesystem;
    const auto parent=fs::absolute(argv[1])/L"runtime/user/shader";
    const auto leaf=parent/(L"data-pin-control-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    const auto renamed=fs::path(leaf.native()+L"-renamed");
    if(!CreateDirectoryW(leaf.c_str(),nullptr)) return 2;
    int result=0;
    bool uninitialized=false;
    try{(void)CtrVideo::GetShaderDumpSession();}catch(const std::logic_error&){uninitialized=true;}
    if(!uninitialized) return 7;
    try {
        {
            CtrVideo::ShaderDirectoryPin pin(leaf);
            if(MoveFileW(leaf.c_str(),renamed.c_str()) || GetLastError()!=ERROR_SHARING_VIOLATION) throw std::runtime_error("rename not blocked");
            if(RemoveDirectoryW(leaf.c_str()) || GetLastError()!=ERROR_SHARING_VIOLATION) throw std::runtime_error("delete not blocked");
        }
        if(!MoveFileW(leaf.c_str(),renamed.c_str()) || !MoveFileW(renamed.c_str(),leaf.c_str())) throw std::runtime_error("release failed");
        bool rejected=false;
        try { CtrVideo::ShaderDirectoryPin pin(leaf/L"absent"); }
        catch(const std::runtime_error&) { rejected=true; }
        if(!rejected || !MoveFileW(leaf.c_str(),renamed.c_str()) || !MoveFileW(renamed.c_str(),leaf.c_str())) throw std::runtime_error("partial chain leaked");
        const auto relative=leaf.lexically_relative(fs::absolute(argv[1])/L"runtime/user/shader")/L"payload.bin";
        const char payload[]="shader-output-control";
        { CtrVideo::ShaderOutputStream stream(fs::absolute(argv[1]),relative);
        stream.Write(payload,7);stream.Write(payload+7,sizeof(payload)-7);stream.Finish();
        bool closed=false;try{stream.Write(payload,1);}catch(const std::logic_error&){closed=true;}
        if(!closed) throw std::runtime_error("closed stream accepted write"); }
        HANDLE input=CreateFileW((leaf/L"payload.bin").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        if(input==INVALID_HANDLE_VALUE) throw std::runtime_error("readback open failed");
        char actual[sizeof(payload)]{}; DWORD read=0;
        const bool read_ok=ReadFile(input,actual,sizeof(actual),&read,nullptr)!=0;
        CloseHandle(input);
        if(!read_ok || read!=sizeof(payload) || std::string(actual)!=payload) throw std::runtime_error("output mismatch");
        rejected=false;
        try {CtrVideo::WriteNewShaderOutput(fs::absolute(argv[1]),relative,payload,sizeof(payload));}
        catch(const std::runtime_error&) {rejected=true;}
        if(!rejected) throw std::runtime_error("overwrite accepted");
        const auto alias=leaf/L"alias.bin";
        if(!CreateHardLinkW(alias.c_str(),(leaf/L"payload.bin").c_str(),nullptr)) throw std::runtime_error("hardlink fixture failed");
        rejected=false;
        try {CtrVideo::WriteNewShaderOutput(fs::absolute(argv[1]),relative.parent_path()/L"alias.bin","x",1);}
        catch(const std::runtime_error&) {rejected=true;}
        if(!rejected || fs::file_size(leaf/L"payload.bin")!=sizeof(payload)) throw std::runtime_error("hardlink target changed");
        if(!DeleteFileW(alias.c_str())) throw std::runtime_error("hardlink cleanup failed");
        CtrVideo::WriteNewShaderOutput(fs::absolute(argv[1]),relative.parent_path()/L"empty.bin",nullptr,0);
        if(fs::file_size(leaf/L"empty.bin")!=0 || !DeleteFileW((leaf/L"empty.bin").c_str())) throw std::runtime_error("empty output failed");
        rejected=false;
        try {CtrVideo::WriteNewShaderOutput(fs::absolute(argv[1]),relative.parent_path()/L"absent/file.bin","x",1);}
        catch(const std::runtime_error&) {rejected=true;}
        if(!rejected || fs::exists(leaf/L"absent")) throw std::runtime_error("missing parent not explicit");
        if(!DeleteFileW((leaf/L"payload.bin").c_str())) throw std::runtime_error("output cleanup failed");
        rejected=false;
        try {CtrVideo::WriteNewShaderOutput(fs::absolute(argv[1]),relative,nullptr,1);}
        catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected || fs::exists(leaf/L"payload.bin")) throw std::runtime_error("invalid extent touched disk");
        fs::create_directories(leaf/L"runtime/user/shader/dumps");
        {
            CtrVideo::ShaderDumpSession session(leaf);
            auto output=session.Open(L"session.bin");output->Write(payload,sizeof(payload));output->Finish();output.reset();
            if(fs::file_size(leaf/L"runtime/user/shader/dumps/session.bin")!=sizeof(payload)) throw std::runtime_error("session output failed");
            rejected=false;try{(void)session.Open(L"../escape");}catch(const std::invalid_argument&){rejected=true;}
            if(!rejected) throw std::runtime_error("session accepted nonleaf");
            if(!DeleteFileW((leaf/L"runtime/user/shader/dumps/session.bin").c_str())) throw std::runtime_error("session file cleanup failed");
        }
        for(const auto* suffix:{L"runtime/user/shader/dumps",L"runtime/user/shader",L"runtime/user",L"runtime"})
            if(!RemoveDirectoryW((leaf/suffix).c_str())) throw std::runtime_error("session directory cleanup failed");
        HANDLE competitor=CreateFileW(leaf.c_str(),DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(competitor==INVALID_HANDLE_VALUE) throw std::runtime_error("competitor open failed");
        rejected=false;
        try { CtrVideo::ShaderDirectoryPin pin(leaf); }
        catch(const std::runtime_error&) { rejected=true; }
        CloseHandle(competitor);
        if(!rejected) throw std::runtime_error("delete competitor accepted");
    } catch(const std::exception& error) {std::fprintf(stderr,"%s winerr=%lu\n",error.what(),GetLastError());result=3;}
    if(!RemoveDirectoryW(leaf.c_str())) {std::fprintf(stderr,"cleanup winerr=%lu\n",GetLastError()); result=4;}
    return result;
}
