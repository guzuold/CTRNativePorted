// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owner_dispatch.hpp"
#include "owner_dispatch_api.h"
namespace ctrport {
inline int owner_dispatch_invoke(void* context,CtrOwnerAction action,void* user,int* output) noexcept {
 if(!context || !action || !output)return 1;
 try{const auto result=static_cast<OwnerDispatch*>(context)->invoke([=]{return action(user);});*output=result;return 0;}catch(...){return 1;}
}
inline CtrOwnerDispatchApi make_owner_dispatch_api(OwnerDispatch& dispatcher){return {sizeof(CtrOwnerDispatchApi),1,&dispatcher,&owner_dispatch_invoke};}
struct OwnerApiFixture {std::thread::id owner;bool observed=false;};
inline int owner_api_fixture(void* raw){auto& fixture=*static_cast<OwnerApiFixture*>(raw);fixture.observed=std::this_thread::get_id()==fixture.owner;return -73;}
inline int owner_dispatch_api_control(){
 OwnerDispatch dispatcher;const auto api=make_owner_dispatch_api(dispatcher);OwnerApiFixture fixture{std::this_thread::get_id(),false};int result=91;
 auto worker=std::async(std::launch::async,[&]{return api.invoke(api.context,&owner_api_fixture,&fixture,&result);});
 if(!dispatcher.pump(1000) || worker.get() || result!=-73 || !fixture.observed)return 1;
 result=91;unsigned rejected=0;
 if(api.invoke(nullptr,&owner_api_fixture,&fixture,&result)==1 && result==91)++rejected;
 if(api.invoke(api.context,nullptr,&fixture,&result)==1 && result==91)++rejected;
 if(api.invoke(api.context,&owner_api_fixture,&fixture,nullptr)==1)++rejected;
 dispatcher.close();fixture.observed=false;
 if(api.invoke(api.context,&owner_api_fixture,&fixture,&result)==1 && result==91 && !fixture.observed)++rejected;
 if(rejected!=4)return 1;
 std::cout<<"{\"scope\":\"owner_dispatch_C_ABI_control\",\"action_result\":-73,\"negative_rejections\":4,\"output_preserved\":true,\"video_integrated\":false}\n";return 0;
}
}
