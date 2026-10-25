// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <memory>
#include <string>
namespace ctrport::settings::geometry_controls {
enum class DistanceScale { original, half, three_quarters };
class DistanceHook {
public:
    ~DistanceHook();
    DistanceHook(const DistanceHook&)=delete;
    DistanceHook& operator=(const DistanceHook&)=delete;
    // Only after every guest worker is quiescent. Destructor has the same contract.
    void uninstall();
    void invoke(const unsigned long long* arguments) noexcept;
private:
    DistanceHook()=default;
    friend std::unique_ptr<DistanceHook> install(unsigned char*,std::size_t,const std::string&,DistanceScale);
    unsigned char* image_=nullptr;
    unsigned char* code_=nullptr;
    float factor_=1;
    bool installed_=false;
    bool unwind_registered_=false;
};
// Bootstrap only: image must be writable, no guest thread may have started.
// Unknown version, signature mismatch, invalid enum fail before image writes.
std::unique_ptr<DistanceHook> install(unsigned char* base,std::size_t extent,const std::string& sha,DistanceScale selected);
}
