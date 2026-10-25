// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <memory>
#include <string>
namespace ctrport::settings::quality_controls {
enum class WorldLod{original,reduced};
class WorldHook {
 public:~WorldHook();void uninstall();void invoke(const unsigned long long*)noexcept;
 WorldHook(const WorldHook&)=delete;WorldHook&operator=(const WorldHook&)=delete;
 private:WorldHook()=default;unsigned char*image_=nullptr;unsigned char*code_=nullptr;bool installed_=false,unwind_registered_=false;
 friend std::unique_ptr<WorldHook> install_world(unsigned char*,std::size_t,const std::string&,WorldLod);
};
std::unique_ptr<WorldHook> install_world(unsigned char*,std::size_t,const std::string&,WorldLod);
}
