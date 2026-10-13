// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <filesystem>
#include <vector>
#include <cstddef>
#include <memory>
namespace CtrVideo {
// Creates a new leaf only; parent directories must already exist. Existing files are rejected.
void WriteNewShaderOutput(const std::filesystem::path& root, const std::filesystem::path& relative,
                          const void* bytes, std::size_t size);
// Pins an existing directory chain only; does not open output files or create directories.
class ShaderDirectoryPin final {
public:
    explicit ShaderDirectoryPin(const std::filesystem::path& directory);
    ~ShaderDirectoryPin();
    ShaderDirectoryPin(const ShaderDirectoryPin&)=delete;
    ShaderDirectoryPin& operator=(const ShaderDirectoryPin&)=delete;
private:
    std::vector<void*> handles_;
};
class ShaderOutputStream final {
public:
    ShaderOutputStream(const std::filesystem::path& root,const std::filesystem::path& relative);
    ~ShaderOutputStream();
    ShaderOutputStream(const ShaderOutputStream&)=delete;
    ShaderOutputStream& operator=(const ShaderOutputStream&)=delete;
    void Write(const void* bytes,std::size_t size);
    void Finish();
private:
    std::filesystem::path target_;
    ShaderDirectoryPin parents_;
    void* file_=nullptr;
    bool failed_=false;
};
class ShaderDumpSession final {
public:
    explicit ShaderDumpSession(const std::filesystem::path& root);
    std::unique_ptr<ShaderOutputStream> Open(const std::filesystem::path& filename) const;
private:
    const std::filesystem::path root_;
    ShaderDirectoryPin directory_;
};
void InitializeShaderDumpSession(const std::filesystem::path& root);
const ShaderDumpSession& GetShaderDumpSession();
}
