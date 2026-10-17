// SPDX-License-Identifier: GPL-2.0-only
// Synthetic parsing checks only; does not establish live race continuity.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "race_host_migration.hpp"

int main() {
    using namespace ctrport::race_migration;
    unsigned failures=0;
    auto check=[&](bool value,const char* name){if(!value){++failures;std::fprintf(stderr,"FAIL: %s\n",name);}};
    auto parse=[](const std::string& text){cache_routes(reinterpret_cast<std::uint64_t>(text.c_str()));};
    const std::string address(134,'a');
    parse("{\"ctr_migration\":\"v1|42="+address+";43="+address+";\"}");
    check(routes.size()==2 && routes[0].account==42 && routes[1].account==43 && routes[1].address[66]==0xaa,"ordered fixed-size routes");
    for(const auto& text:std::vector<std::string>{
        "{}",
        "{\"ctr_migration\":\"v1|42=abcd;\"}",
        "{\"ctr_migration\":\"v1|42="+std::string(134,'z')+";\"}",
        "{\"ctr_migration\":\"v1|42="+address+";42="+address+";\"}",
        "{\"ctr_migration\":\"v1|18446744073709551616="+address+";\"}",
        "{\"ctr_migration\":\"v1|0="+address+";\"}",
        "{\"ctr_migration\":\"v1|42="+address+";\",\"ctr_migration\":\"v1|\"}",
        "{\"ctr_migration\":\"v1|42="+address+";"
    }) {parse(text);check(routes.empty(),"invalid/missing field clears previous routes");}
    std::string full="{\"ctr_migration\":\"v1|";
    for(unsigned n=1;n<=9;++n)full+=std::to_string(n)+"="+address+";";
    full+="\"}";parse(full);check(routes.empty(),"more than eight accounts rejected");
    std::printf("migration route parser: %u failures\n",failures);
    return failures?1:0;
}
