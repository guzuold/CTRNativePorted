// SPDX-License-Identifier: GPL-2.0-only
#include "shader_directory_pin.h"
#include "shader_path.h"
#include <limits>
#include <cstdio>
#include <atomic>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
namespace CtrVideo {
ShaderDirectoryPin::ShaderDirectoryPin(const std::filesystem::path& directory) {
    if(!directory.is_absolute()) throw std::invalid_argument("Directory pin requires absolute path");
    for(const auto& part:directory) if(part==L"." || part==L"..")
        throw std::invalid_argument("Directory pin requires normalized path");
    try {
        std::filesystem::path current=directory.root_path();
        for(const auto& part:directory.relative_path()) {
            current/=part;
            HANDLE handle=CreateFileW(current.c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot pin shader directory");
            BY_HANDLE_FILE_INFORMATION info{};
            if(!GetFileInformationByHandle(handle,&info) ||
               !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
               (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
                CloseHandle(handle);
                throw std::runtime_error("Shader directory handle is not an ordinary directory");
            }
            try { handles_.push_back(handle); }
            catch(...) { CloseHandle(handle); throw; }
        }
        if(handles_.empty()) throw std::invalid_argument("Directory chain is empty");
    } catch(...) {
        for(auto handle:handles_) CloseHandle(handle);
        throw;
    }
}
ShaderDirectoryPin::~ShaderDirectoryPin() {
    for(auto handle:handles_) CloseHandle(handle);
}
ShaderOutputStream::ShaderOutputStream(const std::filesystem::path& root,const std::filesystem::path& relative)
    :target_(ResolveShaderOutput(root,relative)),parents_(target_.parent_path()) {
    HANDLE handle=CreateFileW(target_.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create exclusive shader output");
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info) || info.nNumberOfLinks!=1 ||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(handle);
        throw std::runtime_error("Invalid exclusive shader output handle");
    }
    file_=handle;
}
ShaderOutputStream::~ShaderOutputStream() {
    if(file_) {
        std::fputs("Shader output abandoned without successful Finish; partial file retained\n",stderr);
        if(!CloseHandle(file_)) {std::fputs("Shader output cleanup failed\n",stderr);std::fflush(stderr);ExitProcess(86);}
    }
}
void ShaderOutputStream::Write(const void* bytes,std::size_t size) {
    if(!file_ || failed_) throw std::logic_error("Shader output is closed or failed");
    if((size && !bytes) || size>(std::numeric_limits<DWORD>::max)())
        throw std::invalid_argument("Invalid shader output extent");
    DWORD written=0;
    if(size && (!WriteFile(file_,bytes,static_cast<DWORD>(size),&written,nullptr) || written!=size)) {
        failed_=true;
        throw std::runtime_error("Shader output write failed or short");
    }
}
void ShaderOutputStream::Finish() {
    if(!file_ || failed_) throw std::logic_error("Shader output is closed or failed");
    if(!FlushFileBuffers(file_)) {failed_=true;throw std::runtime_error("Shader output flush failed");}
    HANDLE handle=file_; file_=nullptr;
    if(!CloseHandle(handle)) {failed_=true;throw std::runtime_error("Shader output close failed");}
}
void WriteNewShaderOutput(const std::filesystem::path& root,const std::filesystem::path& relative,
                          const void* bytes,std::size_t size) {
    if((size && !bytes) || size>(std::numeric_limits<DWORD>::max)())
        throw std::invalid_argument("Invalid shader output extent");
    ShaderOutputStream stream(root,relative);
    stream.Write(bytes,size);
    stream.Finish();
}
ShaderDumpSession::ShaderDumpSession(const std::filesystem::path& root)
    :root_(root),directory_(ResolveShaderOutput(root,L"dumps/session-check").parent_path()) {}
std::unique_ptr<ShaderOutputStream> ShaderDumpSession::Open(const std::filesystem::path& filename) const {
    if(filename.empty() || filename.has_root_path() || filename.has_parent_path())
        throw std::invalid_argument("Shader dump requires a leaf filename");
    return std::make_unique<ShaderOutputStream>(root_,std::filesystem::path(L"dumps")/filename);
}
namespace {
std::atomic<const ShaderDumpSession*> dump_session{nullptr};
std::mutex dump_session_mutex;
}
void InitializeShaderDumpSession(const std::filesystem::path& root) {
    std::lock_guard<std::mutex> lock(dump_session_mutex);
    if(dump_session.load(std::memory_order_acquire)) throw std::logic_error("Shader dump session already initialized");
    // Immutable process-lifetime ownership, published only after directory validation/pinning.
    const auto* session=new ShaderDumpSession(root);
    dump_session.store(session,std::memory_order_release);
}
const ShaderDumpSession& GetShaderDumpSession() {
    const auto* session=dump_session.load(std::memory_order_acquire);
    if(!session) throw std::logic_error("Shader dump session not initialized");
    return *session;
}
}
