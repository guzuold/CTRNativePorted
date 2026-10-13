// SPDX-License-Identifier: GPL-2.0-only
#include "shader_path.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
#include <algorithm>
#include <cwctype>
namespace CtrVideo {
std::filesystem::path ResolveShaderOutput(const std::filesystem::path& project_root,
                                         const std::filesystem::path& relative) {
    namespace fs=std::filesystem;
    if(!project_root.is_absolute() || relative.empty() || relative.has_root_path())
        throw std::invalid_argument("Shader output requires absolute project and relative leaf");
    for(const auto& part:project_root) if(part==L".." || part==L".")
        throw std::invalid_argument("Unnormalized project path");
    for(const auto& part:relative) {
        const auto value=part.native();
        if(part==L".." || part==L"." || value.empty() || value.find_first_of(LR"(:*?<>|")")!=std::wstring::npos || value.back()==L'.' || value.back()==L' ')
            throw std::invalid_argument("Unsupported shader output component");
        auto base=value.substr(0,value.find(L'.'));
        std::transform(base.begin(),base.end(),base.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towupper(c));});
        const bool numbered=base.size()==4 && (base.substr(0,3)==L"COM" || base.substr(0,3)==L"LPT") &&
            ((base[3]>=L'1' && base[3]<=L'9') || base[3]==0x00b9 || base[3]==0x00b2 || base[3]==0x00b3);
        if(base==L"CON" || base==L"PRN" || base==L"AUX" || base==L"NUL" || base==L"CONIN$" || base==L"CONOUT$" || numbered)
            throw std::invalid_argument("Windows device alias is not a shader output file");

    }
    const fs::path target=project_root/L"runtime"/L"user"/L"shader"/relative;
    fs::path current;
    for(const auto& part:target) {
        current/=part;
        if(current==current.root_path()) continue;
        const DWORD attributes=GetFileAttributesW(current.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES) {
            const DWORD error=GetLastError();
            if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) continue;
            throw std::runtime_error("Cannot inspect shader output path");
        }
        if(attributes&FILE_ATTRIBUTE_REPARSE_POINT)
            throw std::invalid_argument("Shader output traverses a reparse point");
    }
    return target;
}
}
