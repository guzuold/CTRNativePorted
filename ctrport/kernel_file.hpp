// SPDX-License-Identifier: GPL-2.0-only
// Independent read-only open; public shadPS4 file_system.cpp open/sceKernelOpen,
// e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
#pragma once
#include "settings/hot_trace.hpp"
#include "settings/menu_overlay.hpp"
#include "app0.hpp"
#include "kernel_time.hpp"
#include "gpu_hooks.hpp"
#include <map>
#include <memory>
#include <fstream>
namespace ctrport {
// view: the PC Settings menu archive served instead of the file bytes (settings/menu_overlay.hpp),
// with its own cursor.
struct KernelFile {bool writable=false;std::mutex position_lock;HANDLE handle=INVALID_HANDLE_VALUE;std::filesystem::path path;
    std::shared_ptr<const settings::menu_overlay::View> view;std::uint64_t cursor=0;
    ~KernelFile(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}};
class KernelFiles {
    std::filesystem::path base,patch,user;std::mutex mutex;int next=3;
    std::map<int,std::shared_ptr<KernelFile>> owned;
public:
    void configure(const std::filesystem::path& b,const std::filesystem::path& p={}){base=std::filesystem::canonical(b);patch=p.empty()?p:std::filesystem::canonical(p);}
    void configure_user(const std::filesystem::path& root){
        namespace fs=std::filesystem;
        const auto boundary=fs::canonical("runtime/user");
        const auto parent=fs::canonical(root.parent_path());
        const auto relative=parent.lexically_relative(boundary);
        if(relative.is_absolute())throw std::runtime_error("Writable mount outside runtime/user");
        for(const auto& part:relative)if(part=="..")throw std::runtime_error("Writable mount parent escapes runtime/user");
        fs::create_directory(root);user=fs::canonical(root);
        const auto within=user.lexically_relative(boundary);
        if(within.empty() || within.is_absolute())throw std::runtime_error("Writable mount root invalid");
        for(const auto& part:within)if(part=="..")throw std::runtime_error("Writable mount root escapes runtime/user");
    }
    // Save data mount points (ctr_hle sceSaveDataMount2): guest prefix such as
    // "/savedata0" -> host directory inside runtime/user, same confinement as /data.
    std::map<std::string,std::filesystem::path> mounts;
    void mount(const std::string& point,const std::filesystem::path& root){
        namespace fs=std::filesystem;
        const auto boundary=fs::canonical("runtime/user");fs::create_directories(root);
        const auto host=fs::canonical(root);const auto within=host.lexically_relative(boundary);
        if(within.empty() || within.is_absolute())throw std::runtime_error("Mount outside runtime/user");
        for(const auto& part:within)if(part=="..")throw std::runtime_error("Mount escapes runtime/user");
        std::lock_guard<std::mutex> guard(mutex);mounts[point]=host;
    }
    void unmount(const std::string& point){std::lock_guard<std::mutex> guard(mutex);mounts.erase(point);}
    // Returns the mount root and the path below it for a guest path under a mount point.
    bool find_mount(const std::string& guest,std::filesystem::path& root,std::string& relative){
        std::lock_guard<std::mutex> guard(mutex);
        for(const auto& [point,host]:mounts){
            if(guest==point || guest==point+"/"){root=host;relative.clear();return true;}
            if(guest.rfind(point+"/",0)==0){root=host;relative=guest.substr(point.size()+1);return true;}
        }
        return false;
    }
    std::filesystem::path user_path(const std::string& relative){return confined_path(user,relative);}
    std::filesystem::path confined_path(const std::filesystem::path& root,const std::string& relative){
        namespace fs=std::filesystem;
        if(root.empty() || relative.empty() || relative.find(':')!=std::string::npos || relative.find('\\')!=std::string::npos)throw std::runtime_error("Writable path invalid");
        fs::path requested(relative);if(requested.has_root_path())throw std::runtime_error("Writable absolute path rejected");
        for(const auto& part:requested)if(part==".." || part==".")throw std::runtime_error("Writable traversal rejected");
        const auto parent=fs::canonical(root/requested.parent_path());
        const auto within=parent.lexically_relative(root);
        if(within.is_absolute())throw std::runtime_error("Writable parent outside mount");
        for(const auto& part:within)if(part=="..")throw std::runtime_error("Writable parent escapes mount");
        const auto target=parent/requested.filename();
        if(fs::exists(target) && (fs::is_symlink(fs::symlink_status(target)) || !fs::is_regular_file(target)))throw std::runtime_error("Writable target is not owned regular file");
        return target;
    }
    std::shared_ptr<const settings::menu_overlay::View> menu_view(const std::string& relative,const std::filesystem::path& host){
        return settings::menu_overlay::overlay.find(relative,host,[&](const std::string& r){return app0_read_path(base,patch,r);});
    }
    std::filesystem::path metadata_path(const std::string& guest){
        if(guest=="/app0" || guest=="/app0/"){if(base.empty())throw std::runtime_error("File base mount missing");return patch.empty()?base:patch;}
        if(guest.rfind("/app0/",0)==0){if(base.empty())throw std::runtime_error("File base mount missing");return app0_read_path(base,patch,guest.substr(6),false);}
        if(guest.rfind("/data/",0)==0)return user_path(guest.substr(6));
        std::filesystem::path root;std::string relative;
        if(find_mount(guest,root,relative))return relative.empty()?root:confined_path(root,relative);
        throw std::runtime_error("Stat mount unsupported");
    }
    int open(const char* path,int flags,unsigned mode){
        if(!path)return int(0x8002000eU);
        std::string guest;bool terminated=false;
        for(unsigned i=0;i<256;++i){char ch=0;SIZE_T read=0;if(!ReadProcessMemory(GetCurrentProcess(),path+i,&ch,1,&read) || read!=1)throw std::runtime_error("File pathname unreadable");if(!ch){terminated=true;break;}guest.push_back(ch);}
        if(!terminated)return int(0x8002003fU);
        std::cout<<"{\"scope\":\"kernel_open_observed\",\"path\":"<<std::quoted(guest)<<",\"flags\":"<<flags<<",\"mode\":"<<mode<<"}\n"<<std::flush;
        std::filesystem::path host;DWORD access=GENERIC_READ,creation=OPEN_EXISTING;
        if(guest.rfind("/app0/",0)==0){
            if(flags!=0)throw std::runtime_error("app0 is read-only");
            if(base.empty())throw std::runtime_error("File base mount missing");
            try{host=app0_read_path(base,patch,guest.substr(6));}catch(const std::runtime_error& error){if(std::string(error.what())=="app0 file missing")return int(0x80020002U);throw;}
        }else{
            std::filesystem::path root;std::string relative;
            const bool data=guest.rfind("/data/",0)==0;
            if(data){root=user;relative=guest.substr(6);}
            else if(!find_mount(guest,root,relative) || relative.empty())throw std::runtime_error("File mount unsupported");
            if((flags&~0xe03) || (flags&3)==3 || ((flags&0x800) && !(flags&0x200)) || ((flags&0x400) && !(flags&3)))throw std::runtime_error("Writable open flags unsupported");
            access=(flags&3)==1?GENERIC_WRITE:(flags&3)==2?GENERIC_READ|GENERIC_WRITE:GENERIC_READ;
            creation=(flags&0x800)?CREATE_NEW:(flags&0x200)?((flags&0x400)?CREATE_ALWAYS:OPEN_ALWAYS):(flags&0x400)?TRUNCATE_EXISTING:OPEN_EXISTING;
            try{host=confined_path(root,relative);}catch(const std::filesystem::filesystem_error& error){if(error.code()==std::errc::no_such_file_or_directory)return int(0x80020002U);throw;}
        }
        auto file=std::make_shared<KernelFile>();file->path=host;file->writable=(access&GENERIC_WRITE)!=0;
        file->handle=CreateFileW(host.c_str(),access,FILE_SHARE_READ,nullptr,creation,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(file->handle==INVALID_HANDLE_VALUE){const auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)return int(0x80020002U);if(error==ERROR_FILE_EXISTS || error==ERROR_ALREADY_EXISTS)return int(0x80020011U);if(error==ERROR_ACCESS_DENIED)return int(0x8002000dU);throw std::runtime_error("Native owned-file open failed");}
        if(guest.rfind("/app0/",0)==0)if(auto view=menu_view(guest.substr(6),host)){
            LARGE_INTEGER size{};
            if(!GetFileSizeEx(file->handle,&size))throw std::runtime_error("Native file size query failed");
            if(view->source_size && std::uint64_t(size.QuadPart)!=view->source_size){std::fprintf(stderr,"ctrport: PC Settings menu not applied to %s: file changed\n",guest.c_str());std::fflush(stderr);}
            else file->view=view;
        }
        std::lock_guard<std::mutex> guard(mutex);if(next==INT_MAX)throw std::runtime_error("File descriptor domain exhausted");const auto fd=next++;owned.emplace(fd,file);return fd;
    }
    int close(int fd){
        std::lock_guard<std::mutex> guard(mutex);const auto it=owned.find(fd);
        if(it==owned.end())return int(0x80020009U);
        if(it->second.use_count()!=1)throw std::runtime_error("Close concurrent with active file operation not implemented");
        if(!CloseHandle(it->second->handle))throw std::runtime_error("Native owned-file close failed");
        it->second->handle=INVALID_HANDLE_VALUE;owned.erase(it);
        std::cout<<"{\"scope\":\"owned_file_close\",\"fd\":"<<fd<<"}\n"<<std::flush;return 0;
    }
    std::shared_ptr<KernelFile> find(int fd){std::lock_guard<std::mutex> guard(mutex);const auto it=owned.find(fd);return it==owned.end()?nullptr:it->second;}
};
inline KernelFiles kernel_files;
extern "C" int ctr_kernel_open(const char* path,int flags,unsigned mode,std::uint64_t caller){try{return kernel_files.open(path,flags,mode);}catch(const std::exception& error){std::fprintf(stderr,"NID=1G3lF1Gg1k8 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_open();
// Public Orbis layout and regular-file policy: shadPS4 file_system.h:22,
// file_system.cpp:790, pinned revision/license above. Unknown identity fields
// remain zero per that public virtual-filesystem contract, not host identities.
struct FileTimespec {std::int64_t seconds,nanoseconds;};
struct GuestFileStat {
    std::uint32_t device,inode;std::uint16_t mode,links;std::uint32_t uid,gid,rdev;
    FileTimespec access,modified,changed;std::int64_t size,blocks;
    std::uint32_t blocksize,flags,generation;std::int32_t spare;FileTimespec birth;
};
static_assert(sizeof(GuestFileStat)==120 && offsetof(GuestFileStat,size)==72);
inline FileTimespec file_timestamp(LONGLONG ticks){
    constexpr LONGLONG epoch=116444736000000000LL;
    const auto unix_ticks=ticks-epoch;auto seconds=unix_ticks/10000000LL;
    auto remainder=unix_ticks%10000000LL;if(remainder<0){--seconds;remainder+=10000000LL;}
    return {seconds,remainder*100};
}
inline int kernel_fstat(KernelFiles& files,int fd,GuestFileStat* output){
    if(!output)return int(0x8002000eU);
    const auto file=files.find(fd);if(!file)return int(0x80020009U);
    require_time_output(output,sizeof(*output));
    FILE_STANDARD_INFO standard{};FILE_BASIC_INFO basic{};
    if(!GetFileInformationByHandleEx(file->handle,FileStandardInfo,&standard,sizeof(standard)) ||
       !GetFileInformationByHandleEx(file->handle,FileBasicInfo,&basic,sizeof(basic)))throw std::runtime_error("Native file metadata query failed");
    if(standard.Directory || standard.EndOfFile.QuadPart<0)throw std::runtime_error("Only owned regular-file stat supported");
    GuestFileStat result{};result.mode=0100777;result.blocksize=512;
    result.size=file->view?std::int64_t(file->view->size()):standard.EndOfFile.QuadPart;result.blocks=result.size/512+(result.size%512!=0);
    result.access=file_timestamp(basic.LastAccessTime.QuadPart);result.modified=file_timestamp(basic.LastWriteTime.QuadPart);
    result.changed=file_timestamp(basic.ChangeTime.QuadPart);result.birth=file_timestamp(basic.CreationTime.QuadPart);
    *output=result;
    std::cout<<"{\"scope\":\"owned_file_stat\",\"fd\":"<<fd<<",\"size\":"<<result.size<<"}\n"<<std::flush;return 0;
}
// Adapted public posix_stat/sceKernelStat contract from pinned shadPS4
// file_system.cpp:686..771 (GPL-2.0-or-later). Actual host metadata, no success stub.
inline int kernel_stat(KernelFiles& files,const char* path,GuestFileStat* output){
 if(!path || !output)return int(0x8002000eU);
 std::string guest;bool terminated=false;
 for(unsigned i=0;i<256;++i){char ch=0;SIZE_T n=0;
  if(!ReadProcessMemory(GetCurrentProcess(),path+i,&ch,1,&n) || n!=1)throw std::runtime_error("Stat pathname unreadable");
  if(!ch){terminated=true;break;}guest.push_back(ch);
 }
 if(!terminated)return int(0x8002003fU);
 require_time_output(output,sizeof(*output));
 std::cout<<"{\"scope\":\"kernel_stat_observed\",\"path\":"<<std::quoted(guest)<<"}\n"<<std::flush;
 std::filesystem::path host;
 try{host=files.metadata_path(guest);}catch(const std::runtime_error& e){if(std::string(e.what())=="app0 file missing")return int(0x80020002U);throw;}
 KernelFile opened;
 opened.handle=CreateFileW(host.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(opened.handle==INVALID_HANDLE_VALUE){auto e=GetLastError();if(e==ERROR_FILE_NOT_FOUND || e==ERROR_PATH_NOT_FOUND)return int(0x80020002U);if(e==ERROR_ACCESS_DENIED)return int(0x8002000dU);throw std::runtime_error("Stat native open failed");}
 FILE_STANDARD_INFO standard{};FILE_BASIC_INFO basic{};
 if(!GetFileInformationByHandleEx(opened.handle,FileStandardInfo,&standard,sizeof(standard)) || !GetFileInformationByHandleEx(opened.handle,FileBasicInfo,&basic,sizeof(basic)))throw std::runtime_error("Stat native query failed");
 if(basic.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Stat reparse target rejected");
 GuestFileStat result{};
 if(standard.Directory){result.mode=0040777;result.size=65536;result.blocksize=65536;result.blocks=128;}
 else{if(standard.EndOfFile.QuadPart<0)throw std::runtime_error("Stat negative native size");result.mode=0100777;result.size=standard.EndOfFile.QuadPart;
  if(guest.rfind("/app0/",0)==0)if(const auto view=files.menu_view(guest.substr(6),host))result.size=std::int64_t(view->size());
  result.blocksize=512;result.blocks=result.size/512+(result.size%512!=0);}
 result.access=file_timestamp(basic.LastAccessTime.QuadPart);result.modified=file_timestamp(basic.LastWriteTime.QuadPart);result.changed=file_timestamp(basic.ChangeTime.QuadPart);result.birth=file_timestamp(basic.CreationTime.QuadPart);
 *output=result;return 0;
}
extern "C" int ctr_kernel_stat(const char* path,GuestFileStat* output,std::uint64_t caller){try{return kernel_stat(kernel_files,path,output);}catch(const std::exception& e){std::fprintf(stderr,"NID=eV9wAD2riIA library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_stat();
extern "C" int ctr_kernel_fstat(int fd,GuestFileStat* output,std::uint64_t caller){try{return kernel_fstat(kernel_files,fd,output);}catch(const std::exception& error){std::fprintf(stderr,"NID=kBwCPsYX-m4 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_fstat();

// Positioned read of the original bytes through a reopened overlapped handle (see pread).
inline std::uint64_t native_read_at(HANDLE handle,std::uint64_t offset,unsigned char* target,std::uint64_t count){
    if(!count)return 0;
    KernelFile read;read.handle=ReOpenFile(handle,GENERIC_READ,FILE_SHARE_READ,FILE_FLAG_OVERLAPPED);
    if(read.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Pread reopen failed");
    OVERLAPPED operation{};operation.Offset=static_cast<DWORD>(offset);operation.OffsetHigh=static_cast<DWORD>(offset>>32);
    DWORD received=0;const auto okay=ReadFile(read.handle,target,static_cast<DWORD>(count),&received,&operation);
    if(!okay){const auto error=GetLastError();if(error==ERROR_HANDLE_EOF)return 0;if(error!=ERROR_IO_PENDING)throw std::runtime_error("Native pread failed");if(!GetOverlappedResult(read.handle,&operation,&received,TRUE)){if(GetLastError()==ERROR_HANDLE_EOF)return 0;throw std::runtime_error("Native pread completion failed");}}
    return received;
}
inline std::uint64_t menu_view_read(const KernelFile& file,std::uint64_t offset,unsigned char* target,std::uint64_t count){
    return settings::menu_overlay::read(*file.view,offset,target,count,[&](std::uint64_t at,unsigned char* out,std::uint64_t n){return native_read_at(file.handle,at,out,n);});
}
// Independent regular-file read, public pinned file_system.cpp523-570.
// Synchronous original handle + position_lock share the real cursor with write/seek.
inline std::int64_t kernel_read(KernelFiles& files,int fd,void* output,std::uint64_t count){
 const auto file=files.find(fd);if(!file)return std::int64_t(int(0x80020009U));
 if(count>MAXDWORD)throw std::runtime_error("Read larger than DWORD unsupported");
 if(count && !output)return std::int64_t(int(0x8002000eU));
 if(count)require_time_output(output,static_cast<std::size_t>(count));
 std::lock_guard<std::mutex> lock(file->position_lock);DWORD received=0;
 // With the GPU library, guest pages may be write-protected for GPU tracking:
 // invalidate first and read through a host buffer (shadPS4 file_system.cpp ReadFile).
 thread_local std::vector<unsigned char> staged;void* target=output;
 if(guest_cpu_write_hook && count){guest_cpu_write_hook(output,count);staged.resize(static_cast<std::size_t>(count));target=staged.data();}
 const auto copy_back=[&]{if(target!=output && received)std::memcpy(output,target,received);};
 if(file->view){
  received=static_cast<DWORD>(menu_view_read(*file,file->cursor,static_cast<unsigned char*>(target),count));file->cursor+=received;
 }else if(!ReadFile(file->handle,target,static_cast<DWORD>(count),&received,nullptr)){
  const auto error=GetLastError();if(error==ERROR_ACCESS_DENIED)return std::int64_t(int(0x80020009U));if(error==ERROR_HANDLE_EOF)return 0;throw std::runtime_error("Native regular-file read failed");
 }
 copy_back();
 std::printf("{\"scope\":\"owned_file_read\",\"fd\":%d,\"requested\":%llu,\"received\":%lu}\n",fd,static_cast<unsigned long long>(count),static_cast<unsigned long>(received));std::fflush(stdout);return received;
}
extern "C" std::int64_t ctr_kernel_read(int fd,void* output,std::uint64_t count,std::uint64_t caller){try{return kernel_read(kernel_files,fd,output,count);}catch(const std::exception& e){std::fprintf(stderr,"NID=Cg4srZ6TKbU library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_read();
// Positioned regular-file reads use a separately reopened overlapped handle,
// retaining the original file object's current offset even across concurrent calls.
inline std::int64_t kernel_pread(KernelFiles& files,int fd,void* output,std::uint64_t count,std::int64_t offset){
    const auto file=files.find(fd);if(!file)return std::int64_t(int(0x80020009U));
    if(offset<0)return std::int64_t(int(0x80020016U));
    if(count>MAXDWORD)throw std::runtime_error("Pread larger than DWORD not implemented");
    if(count && !output)return std::int64_t(int(0x8002000eU));
    if(count)require_time_output(output,static_cast<std::size_t>(count));
    if(!count)return 0;
    if(file->view){
        thread_local std::vector<unsigned char> staged_view;unsigned char* out=static_cast<unsigned char*>(output);
        if(guest_cpu_write_hook){guest_cpu_write_hook(output,count);staged_view.resize(static_cast<std::size_t>(count));out=staged_view.data();}
        const auto received=menu_view_read(*file,static_cast<std::uint64_t>(offset),out,count);
        if(out!=output && received)std::memcpy(output,out,static_cast<std::size_t>(received));
        return std::int64_t(received);
    }
    KernelFile read;read.handle=ReOpenFile(file->handle,GENERIC_READ,FILE_SHARE_READ,FILE_FLAG_OVERLAPPED);
    if(read.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Pread reopen failed");
    OVERLAPPED operation{};operation.Offset=static_cast<DWORD>(offset);operation.OffsetHigh=static_cast<DWORD>(static_cast<std::uint64_t>(offset)>>32);
    thread_local std::vector<unsigned char> staged;void* target=output;
    if(guest_cpu_write_hook){guest_cpu_write_hook(output,count);staged.resize(static_cast<std::size_t>(count));target=staged.data();}
    DWORD received=0;const auto okay=ReadFile(read.handle,target,static_cast<DWORD>(count),&received,&operation);
    if(!okay){const auto error=GetLastError();if(error==ERROR_HANDLE_EOF)return 0;if(error!=ERROR_IO_PENDING)throw std::runtime_error("Native pread failed");if(!GetOverlappedResult(read.handle,&operation,&received,TRUE)){if(GetLastError()==ERROR_HANDLE_EOF)return 0;throw std::runtime_error("Native pread completion failed");}}
    if(target!=output && received)std::memcpy(output,target,received);
    if(ctrport::settings::hot_trace()){std::cout<<"{\"scope\":\"owned_file_pread\",\"fd\":"<<fd<<",\"offset\":"<<offset<<",\"requested\":"<<count<<",\"received\":"<<received<<"}\n"<<std::flush;}return received;
}
// sceKernelLseek (shadPS4 file_system.cpp454-521): SET/CUR/END on the shared cursor;
// whence 3/4 (POSIX extensions) ENOTTY, others EINVAL; returns the new position.
inline std::int64_t kernel_lseek(KernelFiles& files,int fd,std::int64_t offset,int whence){
    const auto file=files.find(fd);if(!file)return std::int64_t(int(0x80020009U));
    if(whence==3 || whence==4)return std::int64_t(int(0x80020019U));
    if(whence<0 || whence>2)return std::int64_t(int(0x80020016U));
    std::lock_guard<std::mutex> lock(file->position_lock);
    if(file->view){
        const std::int64_t origin=whence==0?0:whence==1?std::int64_t(file->cursor):std::int64_t(file->view->size());
        if((offset>0 && origin>INT64_MAX-offset) || origin+offset<0)return std::int64_t(int(0x80020016U));
        file->cursor=std::uint64_t(origin+offset);return std::int64_t(file->cursor);
    }
    LARGE_INTEGER distance{},position{};distance.QuadPart=offset;
    if(!SetFilePointerEx(file->handle,distance,&position,whence==0?FILE_BEGIN:whence==1?FILE_CURRENT:FILE_END))return std::int64_t(int(0x80020016U));
    return position.QuadPart;
}
extern "C" std::int64_t ctr_kernel_lseek(int fd,std::int64_t offset,int whence,std::uint64_t caller){try{return kernel_lseek(kernel_files,fd,offset,whence);}catch(const std::exception& error){std::fprintf(stderr,"NID=oib76F-12fk library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_lseek();
// Positioned write (sceKernelPwrite, shadPS4 file_system.cpp1201-1208): writable
// owned files only, through a reopened overlapped handle so the shared cursor is kept.
inline std::int64_t kernel_pwrite(KernelFiles& files,int fd,const void* input,std::uint64_t count,std::int64_t offset){
    const auto file=files.find(fd);if(!file)return std::int64_t(int(0x80020009U));
    if(offset<0)return std::int64_t(int(0x80020016U));
    if(!file->writable)return std::int64_t(int(0x80020009U));
    if(count>MAXDWORD)throw std::runtime_error("Pwrite larger than DWORD not implemented");
    if(count && !input)return std::int64_t(int(0x8002000eU));
    if(!count)return 0;
    std::vector<unsigned char> staged(static_cast<std::size_t>(count));SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),input,staged.data(),staged.size(),&copied) || copied!=staged.size())return std::int64_t(int(0x8002000eU));
    // The owning handle is the only writer (no write sharing): write at the offset on it
    // under the position lock and restore the shared cursor afterwards.
    std::lock_guard<std::mutex> lock(file->position_lock);
    LARGE_INTEGER zero{},cursor{};
    if(!SetFilePointerEx(file->handle,zero,&cursor,FILE_CURRENT))throw std::runtime_error("Pwrite cursor query failed");
    OVERLAPPED operation{};operation.Offset=static_cast<DWORD>(offset);operation.OffsetHigh=static_cast<DWORD>(static_cast<std::uint64_t>(offset)>>32);
    DWORD written=0;
    const BOOL okay=WriteFile(file->handle,staged.data(),static_cast<DWORD>(count),&written,&operation);
    if(!SetFilePointerEx(file->handle,cursor,nullptr,FILE_BEGIN))throw std::runtime_error("Pwrite cursor restore failed");
    if(!okay)throw std::runtime_error("Native pwrite failed");
    std::cout<<"{\"scope\":\"owned_file_pwrite\",\"fd\":"<<fd<<",\"offset\":"<<offset<<",\"requested\":"<<count<<",\"written\":"<<written<<"}\n"<<std::flush;return written;
}
extern "C" std::int64_t ctr_kernel_pwrite(int fd,const void* input,std::uint64_t count,std::int64_t offset,std::uint64_t caller){try{return kernel_pwrite(kernel_files,fd,input,count,offset);}catch(const std::exception& error){std::fprintf(stderr,"NID=nKWi-N2HBV4 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_pwrite();
extern "C" std::int64_t ctr_kernel_pread(int fd,void* output,std::uint64_t count,std::int64_t offset,std::uint64_t caller){try{return kernel_pread(kernel_files,fd,output,count,offset);}catch(const std::exception& error){std::fprintf(stderr,"NID=+r3rMFwItV4 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_pread();
// Public kernel/file_system.cpp write/sceKernelWrite, pinned provenance above.
inline std::int64_t kernel_write(KernelFiles& files,int fd,const void* input,std::uint64_t count){
    const auto file=files.find(fd);if(!file || !file->writable)return std::int64_t(int(0x80020009U));
    if(count>16ULL*1024*1024)throw std::runtime_error("Write above bounded16MiB domain unsupported");
    if(!count)return 0;
    if(!input)return std::int64_t(int(0x8002000eU));
    std::vector<unsigned char> copy(static_cast<std::size_t>(count));SIZE_T read=0;
    if(!ReadProcessMemory(GetCurrentProcess(),input,copy.data(),copy.size(),&read) || read!=copy.size())return std::int64_t(int(0x8002000eU));
    std::lock_guard<std::mutex> guard(file->position_lock);DWORD written=0;
    if(!WriteFile(file->handle,copy.data(),static_cast<DWORD>(count),&written,nullptr)){
        const auto error=GetLastError();if(error==ERROR_DISK_FULL)return std::int64_t(int(0x8002001cU));
        throw std::runtime_error("Native owned-file write failed");
    }
    return written;
}
extern "C" std::int64_t ctr_kernel_write(int fd,const void* input,std::uint64_t count,std::uint64_t caller){try{return kernel_write(kernel_files,fd,input,count);}catch(const std::exception& error){std::fprintf(stderr,"NID=4wSze92BhLI library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_write();

extern "C" int ctr_kernel_close(int fd,std::uint64_t caller){try{return kernel_files.close(fd);}catch(const std::exception& error){std::fprintf(stderr,"NID=UK2Tl2DWUns library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}}
extern "C" void ctr_sysv_kernel_close();

}
