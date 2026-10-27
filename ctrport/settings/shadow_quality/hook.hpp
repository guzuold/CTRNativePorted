// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <memory>
#include <string>
namespace ctrport::settings::shadow_quality {
enum class Resolution { original, half, three_quarters };
struct Options {
    Resolution resolution = Resolution::original;
    unsigned cascade_limit = 0;
};
class Hook {
public:
    ~Hook();
    void uninstall();
    Hook(const Hook&) = delete;
    Hook& operator=(const Hook&) = delete;

private:
    Hook() = default;
    friend std::unique_ptr<Hook> install(unsigned char*, std::size_t, const std::string&, Options);
    unsigned char* image_ = nullptr;
    unsigned char* code_ = nullptr;
    bool installed_ = false, publisher_ = false;
};
std::unique_ptr<Hook> install(unsigned char*, std::size_t, const std::string&, Options);
}
