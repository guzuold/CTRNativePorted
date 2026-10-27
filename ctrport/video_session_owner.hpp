// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owner_dispatch.hpp"
#include "gpu_hooks.hpp"
#include "video_clock_binding.hpp"
#include "video/open_session.h"
#include "owned_guest_copy.hpp"
#include "video/guest_status_copy.h"
#include "video/guest_capability_write.h"
#include "video/guest_attribute_write.h"
#include "video/tiled_session.hpp"
#include "video/output_mode.h"
#include "original_call.hpp"
#include "kernel_equeue.hpp"
#include <atomic>
namespace ctrport {
// Owns a real Win32/Vulkan session on its creation thread. The borrowed API
// never escapes the callback; consumers execute synchronously through dispatch.
class RuntimeVideoSession {
 std::thread worker_; std::promise<int> ready_; std::atomic<bool> stop_{false};
 std::mutex mutex_; std::shared_ptr<OwnerDispatch> dispatch_;
 const CtrVideoSessionApiV1* session_=nullptr; bool published_=false;
 CtrVideoResolutionConfigV1 resolution_{};
 static int body(const CtrVideoSessionApiV1* api,uint32_t width,uint32_t height,uint32_t,void* raw){
  auto& self=*static_cast<RuntimeVideoSession*>(raw);
  if(!api || api->size!=sizeof(*api) || api->version!=1 || !api->context || !api->pump_messages)return 60;
  const int clock=bind_video_clock(api,kernel_display_events,&ctr_video_bind_clock_v1);
  if(clock)return clock;
  std::cout<<"{\"scope\":\"original_VideoOut_clock_bound\",\"status\":0,\"before_session_publication\":true,\"global_kernel_clock\":true,\"extra_events\":0}\n"<<std::flush;
  self.resolution_.size=sizeof(self.resolution_);self.resolution_.version=1;self.resolution_.configured=1;
  self.resolution_.value.full_width=self.resolution_.value.pane_width=static_cast<int32_t>(width);
  self.resolution_.value.full_height=self.resolution_.value.pane_height=static_cast<int32_t>(height);
  auto dispatch=std::make_shared<OwnerDispatch>();
  {std::lock_guard<std::mutex> lock(self.mutex_);self.dispatch_=dispatch;self.session_=api;self.published_=true;}
  self.ready_.set_value(0);
  int result=0;
  while(!self.stop_.load()){
   dispatch->pump(5);
   result=api->pump_messages(api->context);if(result)break;
  }
  dispatch->close();
  {std::lock_guard<std::mutex> lock(self.mutex_);self.session_=nullptr;self.dispatch_.reset();}
  return result;
 }
public:
 int start(){
  auto ready=ready_.get_future();
  worker_=std::thread([this]{
   // Runtime-selected virtual display configuration, not inferred PS4 hardware.
   const CtrVideoWindowOptionsV1 options{sizeof(options),1,1280,720,1,0};
   const int result=ctr_video_open_main_session_v1(255,0,0,0,&options,&body,this);
   std::lock_guard<std::mutex> lock(mutex_);if(!published_)ready_.set_value(result?result:91);
  });
  return ready.get();
 }
 int invoke(const std::function<int(const CtrVideoSessionApiV1*)>& action){
  std::shared_ptr<OwnerDispatch> dispatch;
  {std::lock_guard<std::mutex> lock(mutex_);dispatch=dispatch_;}
  if(!dispatch)throw std::runtime_error("Video session unavailable");
  return dispatch->invoke([this,&action]{if(!session_)throw std::runtime_error("Video session expired");return action(session_);});
 }
 int copy_resolution(uint64_t address){return invoke([this,address](const CtrVideoSessionApiV1*){const auto memory=make_owned_guest_copy_api();return ctr_video_copy_resolution(&resolution_,&memory,address);});}
 ~RuntimeVideoSession(){stop_=true;if(worker_.joinable())worker_.join();}
};
inline std::mutex runtime_video_mutex;
inline std::shared_ptr<RuntimeVideoSession> runtime_video_session;
inline std::weak_ptr<RuntimeVideoSession> runtime_video_reservation;
inline int runtime_video_open(int,int,int,uint64_t);
inline int runtime_video_close(int);
// Public signatures/errors: shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a,
// src/core/libraries/videoout/{video_out.cpp,driver.cpp,videoout_error.h}, GPL-2.0-or-later.
inline int runtime_video_register(int handle,int first,uint64_t addresses,int count,uint64_t attributes){
 if(!addresses || !attributes)return static_cast<int>(0x80290002u);
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 if(first<0 || count<=0)throw std::runtime_error("RegisterBuffers negative/empty domain not observed");
 if(first>=16 || count>16-first)return static_cast<int>(0x80290001u);
 CtrVideoBufferAttributesV1 value{};std::array<uint64_t,16> buffers{};
 const auto memory=make_owned_guest_copy_api();
 if(memory.read(memory.context,attributes,&value,sizeof(value)) || memory.read(memory.context,addresses,buffers.data(),static_cast<size_t>(count)*8))return static_cast<int>(0x80290002u);
 auto backing=direct_memory_backing;uint32_t group=UINT32_MAX;
 const int status=session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_register_tiled_mapped_v1(api->context,&backing,static_cast<uint32_t>(first),static_cast<uint32_t>(count),&value,buffers.data(),&group);});
 if(status==108)return static_cast<int>(0x8029000fu);
 if(status==109)return static_cast<int>(0x80290010u);
 if(status)throw std::runtime_error("Real tiled registration adapter failed: "+std::to_string(status));
 for(int n=0;n<count;++n)std::cout<<"{\"scope\":\"original_VideoOut_registered_surface_identity\",\"slot\":"<<(first+n)<<",\"group\":"<<group<<",\"guest_address\":"<<buffers[n]<<",\"width\":"<<value.width<<",\"height\":"<<value.height<<",\"pitch\":"<<value.pitch_pixels<<",\"format\":"<<value.format<<",\"tiling\":"<<value.tiling<<",\"contents_published\":false}\n";
 return static_cast<int>(group);
}
inline int runtime_video_label_address(int handle,uint64_t destination){
 if(!destination)return static_cast<int>(0x80290002u);
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 uint64_t address=0;const int status=session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_tiled_label_address_v1(api->context,&address);});
 if(status)throw std::runtime_error("Live tiled labels unavailable: "+std::to_string(status));
 const auto memory=make_owned_guest_copy_api();if(memory.write(memory.context,destination,&address,8))return static_cast<int>(0x80290002u);
 return 16;
}
inline void runtime_video_mode_any(uint64_t destination,uint32_t size){
 // Whole public behavior: even size0..3 writes the full u32 size field.
 // Explicit checked transport bound, never silently truncate the requested span.
 if(size>16u*1024u*1024u)throw std::runtime_error("ModeSetAny exceeds checked-copy transport bound");
 std::vector<unsigned char> value((std::max)(size,4u),0xff);std::memcpy(value.data(),&size,4);
 const auto memory=make_owned_guest_copy_api();
 if(memory.write(memory.context,destination,value.data(),value.size()))throw std::runtime_error("ModeSetAny destination not owned writable span");
}
extern "C" void ctr_video_mode_any(uint64_t address,uint32_t size,uint64_t caller){
 try{runtime_video_mode_any(address,size);std::printf("{\"scope\":\"original_VideoOutModeSetAny_return\",\"caller\":%llu,\"bytes\":%u}\n",static_cast<unsigned long long>(caller),size);std::fflush(stdout);
 }catch(const std::exception& error){std::fprintf(stderr,"NID=pjkDsgxli6c library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_mode_any();
extern "C" int ctr_video_register(int handle,int first,uint64_t addresses,int count,uint64_t attributes,uint64_t caller){
 try{const int result=runtime_video_register(handle,first,addresses,count,attributes);
  std::printf("{\"scope\":\"original_VideoOutRegisterBuffers_return\",\"caller\":%llu,\"result\":%d,\"count\":%d,\"GPU_coherence\":false}\n",static_cast<unsigned long long>(caller),result,count);std::fflush(stdout);return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=w3BY+tAEiQY library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_video_label_address(int handle,uint64_t address,uint64_t caller){
 try{const int result=runtime_video_label_address(handle,address);
  std::printf("{\"scope\":\"original_VideoOutGetBufferLabelAddress_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=OcQybQejHEY library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_register();
extern "C" void ctr_sysv_video_label_address();
inline int runtime_video_unregister(int handle,int group){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 if(group<0)throw std::runtime_error("UnregisterBuffers negative group not observed");
 const int status=session->invoke([=](const CtrVideoSessionApiV1* api){return ctr_video_unregister_tiled_v1(api->context,static_cast<uint32_t>(group));});
 if(status==62)return static_cast<int>(0x80290001u);
 if(status)throw std::runtime_error("Tiled unregister failed: "+std::to_string(status));return 0;
}
extern "C" int ctr_video_unregister(int handle,int group,uint64_t caller){
 try{return runtime_video_unregister(handle,group);}catch(const std::exception& error){std::fprintf(stderr,"NID=N5KDtkIjjJ4 library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_unregister();
inline int runtime_video_configure(int handle,uint32_t reserved,uint64_t address,uint64_t options,uint32_t mode_size,uint32_t options_size){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 if(reserved)return static_cast<int>(0x80290001u);
 if(mode_size!=sizeof(CtrVideoOutputModeV1))throw std::runtime_error("ConfigureOutputMode unknown mode size");
 CtrVideoOutputModeV1 mode{};const auto memory=make_owned_guest_copy_api();
 if(memory.read(memory.context,address,&mode,sizeof(mode)))return static_cast<int>(0x80290002u);
 const int status=session->invoke([&](const CtrVideoSessionApiV1* api){return ctr_video_configure_output_any_v1(api->context,reserved,&mode,reinterpret_cast<const void*>(options),mode_size,options_size);});
 if(status)throw std::runtime_error("Real SDR Any output admission failed: "+std::to_string(status));return 0;
}
extern "C" int ctr_video_configure(int handle,uint32_t reserved,uint64_t mode,uint64_t options,uint32_t mode_size,uint32_t options_size,uint64_t caller){
 try{const int result=runtime_video_configure(handle,reserved,mode,options,mode_size,options_size);std::printf("{\"scope\":\"original_VideoOutConfigureOutputMode_return\",\"caller\":%llu,\"result\":%d,\"real_SDR_admission\":%s}\n",static_cast<unsigned long long>(caller),result,result==0?"true":"false");std::fflush(stdout);return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=N1bEoJ4SRw4 library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_configure();
extern "C" int ctr_video_configure_bridge_fixture(void*);
extern "C" int ctr_video_register_bridge_fixture(void*);
inline int runtime_video_open(int,int,int,uint64_t);
inline int runtime_video_close(int);
inline int runtime_video_open(int user,int bus,int index,uint64_t parameter){
 if(index!=0)return static_cast<int>(0x80290001u);
 if(user!=255 || bus!=0 || parameter)throw std::runtime_error("VideoOutOpen unsupported user/bus/parameter");
 std::lock_guard<std::mutex> lock(runtime_video_mutex);
 if(!runtime_video_reservation.expired())return static_cast<int>(0x80290009u);
 auto session=std::make_shared<RuntimeVideoSession>();
 const int status=session->start();if(status)throw std::runtime_error("Real Win32/Vulkan VideoOut initialization failed: "+std::to_string(status));
 if(session->invoke([](const CtrVideoSessionApiV1* api){return api&&api->context?0:91;}))throw std::runtime_error("VideoOut live session validation failed");
 runtime_video_reservation=session;runtime_video_session=std::move(session);return 1;
}
inline int runtime_video_close(int handle){
 std::shared_ptr<RuntimeVideoSession> closing;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);
  if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);
  closing=std::move(runtime_video_session);
 }
 closing.reset();return 0;
}
inline int runtime_video_adjust_color(int handle,uint64_t address){
 if(!address)return static_cast<int>(0x80290002U);
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 CtrVideoColorSettingsV1 settings{};const auto memory=make_owned_guest_copy_api();
 if(memory.read(memory.context,address,&settings,sizeof(settings)))return static_cast<int>(0x80290002U);
 const int result=session->invoke([settings](const CtrVideoSessionApiV1* api){return ctr_video_adjust_color_v1(api->context,&settings);});
 if(result)throw std::runtime_error("Actual presentation color adjustment unsupported status="+std::to_string(result));return 0;
}
extern "C" int ctr_video_adjust_color(int handle,uint64_t address,uint64_t caller){
 try{const int result=runtime_video_adjust_color(handle,address);std::printf("{\"scope\":\"original_VideoOutAdjustColor_return\",\"caller\":%llu,\"result\":%d,\"owner_thread_dispatch\":true}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=pv9CI5VC+R0 library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_video_adjust_color();
inline int runtime_video_resolution(int handle,uint64_t address){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 const int status=session->copy_resolution(address);
 if(status==112)return static_cast<int>(0x80290002u);
 if(status)throw std::runtime_error("VideoOut resolution adapter failed");return 0;
}
inline int runtime_video_capability(int handle,uint64_t address){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 const int status=session->invoke([address](const CtrVideoSessionApiV1* api){const auto memory=make_owned_guest_copy_api();return ctr_video_copy_capability(api,&memory,address);});
 if(status==112)return static_cast<int>(0x80290002u);
 if(status)throw std::runtime_error("VideoOut capability adapter failed: "+std::to_string(status));return 0;
}
extern "C" int ctr_video_capability(int handle,uint64_t address,uint64_t caller){
 try{const auto result=runtime_video_capability(handle,address);
  char line[256];const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_VideoOutCapability_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);
  DWORD written=0;if(length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=kGVLc3htQE8 library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_capability();
inline int runtime_video_add_event(uint64_t queue,int handle,uint64_t userdata,uint64_t ident){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 const int status=session->invoke([=](const CtrVideoSessionApiV1* api){const auto kernel=display_event_api(kernel_display_events);return api->add_event(api->context,&kernel,queue,ident,userdata);});
 if(status==91)return static_cast<int>(0x8029000cu);
 if(status)throw std::runtime_error("VideoOut event adapter failed");return 0;
}
inline int runtime_video_status(int handle,uint64_t address,int vblank){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 const int status=session->invoke([=](const CtrVideoSessionApiV1* api){const auto memory=make_owned_guest_copy_api();return ctr_video_copy_public_status(api,&memory,address,vblank);});
 if(status==112)return static_cast<int>(0x80290002u);
 if(status)throw std::runtime_error("VideoOut status adapter failed: "+std::to_string(status));return 0;
}
extern "C" int ctr_video_add_flip(uint64_t queue,int handle,uint64_t userdata,uint64_t caller){
 try{return runtime_video_add_event(queue,handle,userdata,6);}catch(const std::exception& error){std::fprintf(stderr,"NID=HXzjK9yI30k library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_video_add_vblank(uint64_t queue,int handle,uint64_t userdata,uint64_t caller){
 try{return runtime_video_add_event(queue,handle,userdata,7);}catch(const std::exception& error){std::fprintf(stderr,"NID=Xru92wHJRmg library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_video_flip_status(int handle,uint64_t address,uint64_t caller){
 try{const int result=runtime_video_status(handle,address,0);
  CtrVideoPublicFlipStatus observed{};const auto memory=make_owned_guest_copy_api();
  if(!result && !memory.read(memory.context,address,&observed,sizeof(observed))){
   static std::atomic<uint64_t> last{UINT64_MAX};
   if(last.exchange(observed.count)!=observed.count){
    std::printf("{\"scope\":\"original_VideoOut_GetFlipStatus_observed\",\"count\":%llu,\"process_time\":%llu,\"tsc\":%llu,\"submit_tsc\":%llu,\"current_buffer\":%d,\"result\":0,\"caller\":%llu}\n",static_cast<unsigned long long>(observed.count),static_cast<unsigned long long>(observed.process_time),static_cast<unsigned long long>(observed.tsc),static_cast<unsigned long long>(observed.submit_tsc),observed.current_buffer,static_cast<unsigned long long>(caller));std::fflush(stdout);
   }
  }return result;}catch(const std::exception& error){std::fprintf(stderr,"NID=SbU3dwp80lQ library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_video_vblank_status(int handle,uint64_t address,uint64_t caller){
 try{return runtime_video_status(handle,address,1);}catch(const std::exception& error){std::fprintf(stderr,"NID=1FZBKy8HeNU library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_add_flip();
extern "C" void ctr_sysv_video_add_vblank();
extern "C" void ctr_sysv_video_flip_status();
extern "C" void ctr_sysv_video_vblank_status();
inline int runtime_video_set_rate(int handle,int rate){
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);if(handle!=1 || !runtime_video_session)return static_cast<int>(0x8029000bu);session=runtime_video_session;}
 const int status=session->invoke([=](const CtrVideoSessionApiV1* api){if(!api->set_flip_rate)return 60;return api->set_flip_rate(api->context,rate);});
 if(status)throw std::runtime_error("VideoOut flip-rate adapter failed: "+std::to_string(status));return 0;
}
extern "C" int ctr_video_set_rate(int handle,int rate,uint64_t caller){
 try{const auto result=runtime_video_set_rate(handle,rate);char line[256];const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_VideoOutSetFlipRate_return\",\"caller\":%llu,\"rate\":%d,\"result\":%d}\n",static_cast<unsigned long long>(caller),rate,result);DWORD written=0;if(length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=CBiu4mCE1DA library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_set_rate();
extern "C" void ctr_video_set_attributes(uint64_t address,uint32_t format,uint32_t tiling,uint32_t aspect,uint32_t width,uint32_t height,uint32_t pitch,uint64_t caller){
 try{const auto memory=make_owned_guest_copy_api();const int status=ctr_video_write_buffer_attributes(&memory,address,format,tiling,aspect,width,height,pitch);
  if(status)throw std::runtime_error("VideoOut attribute checked copy rejected: "+std::to_string(status));
  char line[320];const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_VideoOutSetBufferAttribute_return\",\"caller\":%llu,\"format\":%u,\"tiling\":%u,\"aspect\":%u,\"width\":%u,\"height\":%u,\"pitch\":%u}\n",static_cast<unsigned long long>(caller),format,tiling,aspect,width,height,pitch);DWORD written=0;if(length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);
 }catch(const std::exception& error){std::fprintf(stderr,"NID=i6-sR91Wt-4 library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_set_attributes();
extern "C" void ctr_video_attribute_bridge_fixture(void*);
extern "C" int ctr_video_resolution(int handle,uint64_t address,uint64_t caller){
 try{return runtime_video_resolution(handle,address);}catch(const std::exception& error){std::fprintf(stderr,"NID=6kPnj51T62Y library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_video_close(int handle,uint64_t caller){
 try{return runtime_video_close(handle);}catch(const std::exception& error){std::fprintf(stderr,"NID=uquVH4-Du78 library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_resolution();
extern "C" void ctr_sysv_video_close();
extern "C" int ctr_video_open(int user,int bus,int index,uint64_t parameter,uint64_t caller){
 try{const int result=runtime_video_open(user,bus,index,parameter);
  char line[256];const auto length=snprintf(line,sizeof(line),"{\"scope\":\"original_VideoOutOpen_return\",\"caller\":%llu,\"handle\":%d,\"real_window_session\":true}\n",static_cast<unsigned long long>(caller),result);
  DWORD written=0;if(length>0)WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),line,static_cast<DWORD>(length),&written,nullptr);return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=Up36PTk687E library=libSceVideoOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_video_open();
}

#include "video/splash_lifecycle.h"
namespace ctrport {
extern "C" int ctr_hide_splash(uint64_t caller){
 // GPU library: the presenter shows game frames directly; there is no ctrport splash
 // surface to retire (shadPS4 systemservice.cpp only clears its splash flag).
 if(gpu_library_present){std::printf("{\"scope\":\"HideSplashScreen_gpu_library\",\"caller\":%llu,\"result\":0}\n",static_cast<unsigned long long>(caller));std::fflush(stdout);return 0;}
 try{
 std::shared_ptr<RuntimeVideoSession> session;
 {std::lock_guard<std::mutex> lock(runtime_video_mutex);session=runtime_video_session;}
 if(!session)throw std::runtime_error("HideSplashScreen live video owner unavailable");
 const int result=session->invoke([](const CtrVideoSessionApiV1* api){return ctr_video_hide_system_splash_v1(api->context);});
 if(result)throw std::runtime_error("HideSplashScreen window transition failed host_status="+std::to_string(result));
 std::printf("{\"scope\":\"original_HideSplashScreen_return\",\"caller\":%llu,\"result\":0,\"real_window_visible\":true,\"frame_produced\":false}\n",static_cast<unsigned long long>(caller));std::fflush(stdout);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=Vo5V8KAwCmk library=libSceSystemService caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_hide_splash();
}
