// SPDX-License-Identifier: GPL-2.0-only
#include <windows.h>
#include <string>
#include <thread>
#include <cstdio>
namespace Common { std::string GetCurrentThreadName(); }
int main(){
    PWSTR original=nullptr;
    if(FAILED(GetThreadDescription(GetCurrentThread(), &original))) return 2;
    bool good=true;
    good &= SUCCEEDED(SetThreadDescription(GetCurrentThread(), L""));
    good &= Common::GetCurrentThreadName().empty();
    good &= SUCCEEDED(SetThreadDescription(GetCurrentThread(), L"CTR shader host"));
    good &= Common::GetCurrentThreadName()=="CTR shader host";
    good &= SUCCEEDED(SetThreadDescription(GetCurrentThread(), L"shader \u03b1"));
    good &= Common::GetCurrentThreadName()=="shader \xce\xb1";
    bool worker_good=false;
    std::thread worker([&]{worker_good=SUCCEEDED(SetThreadDescription(GetCurrentThread(), L"worker")) && Common::GetCurrentThreadName()=="worker";});
    worker.join();
    good &= worker_good && Common::GetCurrentThreadName()=="shader \xce\xb1";
    good &= SUCCEEDED(SetThreadDescription(GetCurrentThread(), original));
    LocalFree(original);
    std::puts(good ? "thread_name: empty/ascii/utf8/worker-isolation passed" : "thread_name: failed");
    return good?0:1;
}
