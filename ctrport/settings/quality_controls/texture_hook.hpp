// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <memory>
#include <string>
namespace ctrport::settings::quality_controls {
enum class TextureMips { original, half, quarter };
class TextureHook {
public:
    ~TextureHook();
    void uninstall();
    TextureHook(const TextureHook&) = delete;
    TextureHook& operator=(const TextureHook&) = delete;

private:
    TextureHook() = default;
    unsigned char* image_ = nullptr;
    unsigned char* code_ = nullptr;
    bool installed_ = false;
    friend std::unique_ptr<TextureHook> install_texture(unsigned char*, std::size_t, const std::string&, TextureMips);
};
std::unique_ptr<TextureHook> install_texture(unsigned char*, std::size_t, const std::string&, TextureMips);
}
