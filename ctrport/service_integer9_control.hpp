// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_integer_dispatch.hpp"
namespace ctrport {
extern "C" std::uint64_t ctr_service_integer9_fixture(void*,const std::uint64_t*);
inline std::atomic<bool> integer9_entered{false},integer9_release{false};
inline const std::array<std::uint64_t,9> integer9_arguments{{1,0x8000000000000002ULL,3,0xffffffff00000004ULL,5,6,0x123456789abcdef0ULL,8,0xfedcba9876543210ULL}};
inline uint32_t integer9_first(const uint64_t* args,uint64_t caller,uint64_t* output){
 if(!args || !caller || !output || !std::equal(integer9_arguments.begin(),integer9_arguments.end(),args))return 60;
 integer9_entered.store(true);
 while(!integer9_release.load())std::this_thread::yield();
 *output=0x80020009;return 0;
}
inline uint32_t integer9_second(const uint64_t* args,uint64_t caller,uint64_t* output){
 if(!args || !caller || !output || !std::equal(integer9_arguments.begin(),integer9_arguments.end(),args))return 60;
 *output=0x1122334455667788ULL;return 0;
}
inline int service_integer9_control(){
 const std::string pin(64,'a');ServiceImportRecord record{pin,pin,"synthetic9","test","test",8,9};
 const auto token=integer_dispatch_records.add(record,pin,pin,16);ServiceIntegerThunk thunk(token,9);
 auto future=std::async(std::launch::async,[&]{return ctr_service_integer9_fixture(thunk.target(),integer9_arguments.data());});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 while(!integer9_dispatch_generations.waiting_count()){if(std::chrono::steady_clock::now()>deadline)ExitProcess(5);std::this_thread::yield();}
 if(future.wait_for(std::chrono::milliseconds(20))!=std::future_status::timeout)ExitProcess(6);
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> old=owner;
 integer9_dispatch_generations.publish(owner,{{"synthetic9","test","test",reinterpret_cast<const void*>(&integer9_first),9}},1,pin);owner.reset();
 while(!integer9_entered.load()){if(std::chrono::steady_clock::now()>deadline)ExitProcess(5);std::this_thread::yield();}
 integer9_dispatch_generations.publish(std::make_shared<int>(2),{{"synthetic9","test","test",reinterpret_cast<const void*>(&integer9_second),9}},2,pin);
 if(old.expired())ExitProcess(7);integer9_release.store(true);
 if(future.get()!=0x80020009 || !old.expired())return 8;
 if(ctr_service_integer9_fixture(thunk.target(),integer9_arguments.data())!=0x1122334455667788ULL)return 9;
 integer_dispatch_records.retire(token);integer9_dispatch_generations.shutdown();
 std::cout<<"{\"scope\":\"service_integer9_control\",\"nine_exact_arguments\":true,\"pending_no_return\":true,\"generation_pinned\":true,\"reload\":true,\"guest_errno_bits\":true,\"synthetic\":true,\"PS4_service_implemented\":false}\n";return 0;
}
inline int service_integer9_dll_control(const std::filesystem::path& root){
 const std::string pin(64,'a');ServiceImportRecord record{pin,pin,"synthetic9","test","test",8,9};
 const auto token=integer_dispatch_records.add(record,pin,pin,16);ServiceIntegerThunk thunk(token,9);
 auto future=std::async(std::launch::async,[&]{return ctr_service_integer9_fixture(thunk.target(),integer9_arguments.data());});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 while(!integer9_dispatch_generations.waiting_count()){if(std::chrono::steady_clock::now()>deadline)ExitProcess(5);std::this_thread::yield();}
 CtrHostApi host{sizeof(CtrHostApi),1,nullptr,nullptr};
 load_service_generation(integer9_dispatch_generations,root,root/L"service_nine_1.dll",host,pin,1);
 if(future.get()!=0x80020009)return 1;
 load_service_generation(integer9_dispatch_generations,root,root/L"service_nine_2.dll",host,pin,2);
 if(ctr_service_integer9_fixture(thunk.target(),integer9_arguments.data())!=0x8002000a)return 2;
 integer_dispatch_records.retire(token);integer9_dispatch_generations.shutdown();
 std::cout<<"{\"scope\":\"service_integer9_dll_control\",\"paused_call_resumed\":true,\"DLL_generations\":2,\"nine_exact_arguments\":true,\"synthetic\":true,\"PS4_service_implemented\":false}\n";return 0;
}

inline uint32_t integer9_failure(const uint64_t*,uint64_t,uint64_t* result){*result=123;return 88;}
inline int service_integer9_failure_control(unsigned mode){
 const std::string pin(64,'a');ServiceImportRecord record{pin,pin,"synthetic9","test","test",8,9};
 const auto token=integer_dispatch_records.add(record,pin,pin,16);ServiceIntegerThunk thunk(token,9);
 if(mode==2)integer_dispatch_records.retire(token);
 else integer9_dispatch_generations.publish(std::make_shared<int>(1),{{"synthetic9","test","test",reinterpret_cast<const void*>(&integer9_failure),mode==1?1U:9U}},1,pin);
 (void)ctr_service_integer9_fixture(thunk.target(),integer9_arguments.data());return 99;
}

}
