// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "kernel_time.hpp"
#include <Xinput.h>
#include "user_service.hpp"
extern "C" std::uint64_t ctr_kernel_process_time();
namespace ctrport {
// Public PadData layout/button constants: pinned shadPS4 pad.h/pad.cpp,
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
// Windows axes map their full signed range to unsigned PS4 sticks; Y is inverted.
// XInput has no motion/touch sensor; their fields remain at the neutral values.
inline std::array<unsigned char,120> pad_connected_sample(const XINPUT_GAMEPAD& g,unsigned char connections,uint64_t time){
 std::array<unsigned char,120> out{};uint32_t buttons=0;
 const std::pair<DWORD,uint32_t> map[]={{XINPUT_GAMEPAD_LEFT_THUMB,2},{XINPUT_GAMEPAD_RIGHT_THUMB,4},{XINPUT_GAMEPAD_START,8},
 {XINPUT_GAMEPAD_DPAD_UP,0x10},{XINPUT_GAMEPAD_DPAD_RIGHT,0x20},{XINPUT_GAMEPAD_DPAD_DOWN,0x40},{XINPUT_GAMEPAD_DPAD_LEFT,0x80},
 {XINPUT_GAMEPAD_LEFT_SHOULDER,0x400},{XINPUT_GAMEPAD_RIGHT_SHOULDER,0x800},{XINPUT_GAMEPAD_Y,0x1000},
 {XINPUT_GAMEPAD_B,0x2000},{XINPUT_GAMEPAD_A,0x4000},{XINPUT_GAMEPAD_X,0x8000},{XINPUT_GAMEPAD_BACK,0x100000}};
 for(const auto& item:map)if(g.wButtons&item.first)buttons|=item.second;
 if(g.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)buttons|=0x100;
 if(g.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)buttons|=0x200;
 std::memcpy(out.data(),&buttons,4);
 const auto axis=[](SHORT value,bool inverted){const unsigned shifted=static_cast<unsigned>(int(value)+32768);return static_cast<unsigned char>((inverted?65535-shifted:shifted)>>8);};
 out[4]=axis(g.sThumbLX,false);out[5]=axis(g.sThumbLY,true);out[6]=axis(g.sThumbRX,false);out[7]=axis(g.sThumbRY,true);
 out[8]=g.bLeftTrigger;out[9]=g.bRightTrigger;const float identity=1;std::memcpy(out.data()+24,&identity,4);
 out[76]=1;std::memcpy(out.data()+80,&time,8);out[104]=connections;return out;
}

// New Windows backend, not a compatibility stub. Public PS4 Init lifecycle:
// pinned shadPS4 pad.cpp e3ce810f3a653f43ac64ebab63023de281a4103a.
// XInput API/types from installed Windows SDK. No fabricated connected device,
// keyboard mapping, motion data, PS4 read/open behavior or game input state.
class PadBackend {
 using GetState=DWORD (WINAPI*)(DWORD,XINPUT_STATE*);
 HMODULE module=nullptr;GetState get_state=nullptr;std::mutex mutex;
 struct Port {int user;int handle;DWORD slot;bool connected=false;unsigned char connections=0;};
 std::vector<Port> ports;int next_handle=1;
 std::array<DWORD,4> status{};std::array<XINPUT_STATE,4> snapshots{};
public:
 PadBackend()=default;PadBackend(const PadBackend&)=delete;
 ~PadBackend(){if(module)FreeLibrary(module);}
 int initialize(){
  std::lock_guard<std::mutex> guard(mutex);
  if(module)return 0;
  HMODULE candidate=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  if(!candidate)throw std::runtime_error("Pad XInput system backend unavailable Win32="+std::to_string(GetLastError()));
  FARPROC symbol=GetProcAddress(candidate,"XInputGetState");GetState call=nullptr;
  static_assert(sizeof(call)==sizeof(symbol));std::memcpy(&call,&symbol,sizeof(call));
  if(!call){FreeLibrary(candidate);throw std::runtime_error("Pad XInputGetState unavailable");}
  std::array<DWORD,4> prepared_status{};std::array<XINPUT_STATE,4> prepared{};
  for(DWORD i=0;i<4;++i){prepared_status[i]=call(i,&prepared[i]);
   if(prepared_status[i]!=ERROR_SUCCESS && prepared_status[i]!=ERROR_DEVICE_NOT_CONNECTED){const auto error=prepared_status[i];FreeLibrary(candidate);throw std::runtime_error("Pad XInput polling failed status="+std::to_string(error));}
  }
  module=candidate;get_state=call;status=prepared_status;snapshots=prepared;return 0;
 }
 int open(int user,int type,int index,const void* params){
  std::lock_guard<std::mutex> guard(mutex);
  if(!module || !get_state)return static_cast<int>(0x80920005U);
  if(user<0 || user==255)return static_cast<int>(0x809b0001U);
  if(!user_service.has_user(user))return static_cast<int>(0x809b0081U);
  if(type!=0 || index!=0 || params)throw std::runtime_error("PadOpen only observed standard port0/index0/null parameters implemented");
  for(const auto& port:ports)if(port.user==user)return static_cast<int>(0x80920004U);
  if(!ports.empty())throw std::runtime_error("PadOpen additional local user routing missing");
  XINPUT_STATE state{};const DWORD native=get_state(0,&state);
  if(native!=ERROR_SUCCESS && native!=ERROR_DEVICE_NOT_CONNECTED)throw std::runtime_error("PadOpen real XInput poll failed");
  status[0]=native;snapshots[0]=state;const int handle=next_handle++;ports.push_back({user,handle,0,native==ERROR_SUCCESS,static_cast<unsigned char>(native==ERROR_SUCCESS?1:0)});return handle;
 }
 // Public disconnected sample: pinned pad.cpp ProcessStates368-374/readState467-470,
 // pad.h OrbisPadData size120, quaternion offset12/w24, connected76.
 // GPL-2.0-or-later upstream e3ce810f3a653f43ac64ebab63023de281a4103a.
 int read_state(int handle,void* output){
  if(!output)return static_cast<int>(0x80920001U);
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=std::find_if(ports.begin(),ports.end(),[handle](const Port& p){return p.handle==handle;});
  if(port==ports.end())return static_cast<int>(0x80920003U);
  XINPUT_STATE state{};const DWORD result=get_state(port->slot,&state);
  if(result!=ERROR_SUCCESS && result!=ERROR_DEVICE_NOT_CONNECTED)throw std::runtime_error("XInput read failed status="+std::to_string(result));
  std::array<unsigned char,120> sample{};const float identity=1.0f;std::memcpy(sample.data()+24,&identity,4);
  if(result==ERROR_SUCCESS){
   if(!port->connected){if(port->connections!=255)++port->connections;}
   sample=pad_connected_sample(state.Gamepad,port->connections,ctr_kernel_process_time());
  }
  port->connected=result==ERROR_SUCCESS;status[port->slot]=result;snapshots[port->slot]=state;
  require_time_output(output,sample.size());std::memcpy(output,sample.data(),sample.size());return 0;
 }
 // scePadSetVibration (shadPS4 pad.cpp651-664; OrbisPadVibrationParam {u8 largeMotor;
 // u8 smallMotor}): XInput low-frequency (left) motor = large, high-frequency = small.
 int set_vibration(int handle,const void* parameter){
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=std::find_if(ports.begin(),ports.end(),[handle](const Port& p){return p.handle==handle;});
  if(port==ports.end())return static_cast<int>(0x80920003U);
  if(!parameter)return static_cast<int>(0x80920001U);
  unsigned char motors[2]{};SIZE_T read=0;
  if(!ReadProcessMemory(GetCurrentProcess(),parameter,motors,2,&read) || read!=2)return static_cast<int>(0x80920001U);
  using SetState=DWORD (WINAPI*)(DWORD,XINPUT_VIBRATION*);
  static const SetState set_state=[this]{FARPROC symbol=module?GetProcAddress(module,"XInputSetState"):nullptr;SetState call=nullptr;std::memcpy(&call,&symbol,sizeof(call));return call;}();
  if(set_state && port->connected){XINPUT_VIBRATION vibration{static_cast<WORD>(motors[0]*257),static_cast<WORD>(motors[1]*257)};set_state(port->slot,&vibration);}
  return 0;
 }
 // Public 28-byte controller information, backed by the actual XInput port.
 // No touchpad capability is advertised for an XInput standard controller.
 int information(int handle,void* output){
  if(!output)return static_cast<int>(0x80920001U);
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=std::find_if(ports.begin(),ports.end(),[handle](const Port& p){return p.handle==handle;});
  if(port==ports.end())return static_cast<int>(0x80920003U);
  XINPUT_STATE state{};const DWORD result=get_state(port->slot,&state);
  if(result!=ERROR_SUCCESS && result!=ERROR_DEVICE_NOT_CONNECTED)throw std::runtime_error("XInput information poll failed");
  if(result==ERROR_SUCCESS && !port->connected && port->connections!=255)++port->connections;
  port->connected=result==ERROR_SUCCESS;status[port->slot]=result;snapshots[port->slot]=state;
  std::array<unsigned char,28> info{};
  info[8]=static_cast<unsigned char>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE>>8);
  info[9]=static_cast<unsigned char>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE>>8);
  info[11]=port->connections;info[12]=port->connected?1:0;
  require_time_output(output,info.size());std::memcpy(output,info.data(),info.size());return 0;
 }
 unsigned connected_count(){std::lock_guard<std::mutex> guard(mutex);if(!module || !get_state)throw std::runtime_error("Pad not initialized");unsigned n=0;for(auto result:status)if(result==ERROR_SUCCESS)++n;return n;}
 bool initialized(){std::lock_guard<std::mutex> guard(mutex);return module && get_state;}
};
inline PadBackend pad_backend;
extern "C" int ctr_pad_init(uint64_t caller){
 try{const int result=pad_backend.initialize();std::printf("{\"scope\":\"original_PadInit_return\",\"caller\":%llu,\"result\":%d,\"XInput_backend\":true,\"initial_connected_devices\":%u,\"guest_read_bound\":false}\n",static_cast<unsigned long long>(caller),result,pad_backend.connected_count());std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=hv1luiJrqQM library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_init();
inline int pad_init_control(){
 PadBackend isolated;unsigned rejected=0;try{isolated.connected_count();}catch(...){++rejected;}
 if(isolated.initialized() || rejected!=1)return 1;
 if(isolated.initialize() || !isolated.initialized() || isolated.connected_count()>4 || isolated.initialize())return 1;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_init),0,0,0) || !pad_backend.initialized())return 1;
 std::cout<<"{\"scope\":\"pad_init_control\",\"real_XInput_four_ports_polled\":true,\"SysV0\":true,\"repeat_init\":true,\"uninitialized_query_rejected\":true,\"guest_input_read\":false}\n";return 0;
}
extern "C" int ctr_pad_open(int user,int type,int index,const void* params,uint64_t caller){
 try{const int result=pad_backend.open(user,type,index,params);std::printf("{\"scope\":\"original_PadOpen_return\",\"caller\":%llu,\"user\":%d,\"type\":%d,\"index\":%d,\"result\":%d,\"connected_devices\":%u}\n",static_cast<unsigned long long>(caller),user,type,index,result,pad_backend.connected_count());std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=xk0AcarP3V4 library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_open();
inline int pad_open_control(){
 PadBackend isolated;if(isolated.open(1,0,0,nullptr)!=static_cast<int>(0x80920005U))return 1;
 if(isolated.initialize())return 1;
 if(isolated.open(-1,0,0,nullptr)!=static_cast<int>(0x809b0001U) || isolated.open(1,0,0,nullptr)!=static_cast<int>(0x809b0081U))return 1;
 if(user_service.initialize(nullptr))return 1;const int handle=isolated.open(1,0,0,nullptr);
 if(handle<=0 || isolated.open(1,0,0,nullptr)!=static_cast<int>(0x80920004U))return 1;
 bool rejected=false;try{isolated.open(1,1,0,nullptr);}catch(...){rejected=true;}if(!rejected)return 1;
 std::cout<<"{\"scope\":\"real_XInput_port_open\",\"passed\":true,\"handle_owned\":true,\"connected_devices\":"<<isolated.connected_count()<<",\"no_fabricated_connection\":true}\n";return 0;
}
}

namespace ctrport {
extern "C" int ctr_pad_read_state(int handle,void* output,uint64_t caller){
 try{const int result=pad_backend.read_state(handle,output);std::printf("{\"scope\":\"original_PadReadState_return\",\"caller\":%llu,\"result\":%d,\"connected\":%s}\n",static_cast<unsigned long long>(caller),result,(!result && static_cast<unsigned char*>(output)[76])?"true":"false");std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=YndgXqQVV7c library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_read_state();
extern "C" int ctr_pad_set_vibration(int handle,const void* parameter,uint64_t caller){
 try{return pad_backend.set_vibration(handle,parameter);
 }catch(const std::exception& e){std::fprintf(stderr,"NID=yFVnOdGxvZY library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_set_vibration();
inline int pad_read_state_control(){
 XINPUT_GAMEPAD fixture{};fixture.wButtons=XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_START;
 fixture.sThumbLX=-32768;fixture.sThumbLY=32767;fixture.sThumbRX=32767;fixture.sThumbRY=-32768;fixture.bLeftTrigger=31;fixture.bRightTrigger=30;
 const auto mapped=pad_connected_sample(fixture,2,0x123456789ULL);uint32_t bits=0;uint64_t time=0;std::memcpy(&bits,mapped.data(),4);std::memcpy(&time,mapped.data()+80,8);
 if(bits!=0x4188 || mapped[4]!=0 || mapped[5]!=0 || mapped[6]!=255 || mapped[7]!=255 || mapped[8]!=31 || mapped[9]!=30 || mapped[76]!=1 || mapped[104]!=2 || time!=0x123456789ULL)return 7;
 fixture={};const auto centered=pad_connected_sample(fixture,1,1);if(centered[4]!=128 || centered[6]!=128 || centered[5]!=127 || centered[7]!=127)return 8;
 if(user_service.initialize(nullptr) || pad_backend.initialize())return 1;
 const int handle=pad_backend.open(1,0,0,nullptr);if(handle<=0)return 2;
 std::array<unsigned char,122> output;output.fill(0xa5);
 if(pad_backend.read_state(-1,output.data()+1)!=static_cast<int>(0x80920003U) || output[1]!=0xa5)return 3;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_read_state),handle,reinterpret_cast<uint64_t>(output.data()+1),0))return 4;
 if(!output[77])for(size_t i=1;i<121;++i){const unsigned char expected=i==27?0x80:i==28?0x3f:0;if(output[i]!=expected)return 5;}
 if(output[0]!=0xa5 || output[121]!=0xa5)return 6;
 std::cout<<"{\"scope\":\"actual_XInput_sample\",\"passed\":true,\"connected_translation\":true}\n";return 0;
}
}

namespace ctrport {
extern "C" int ctr_pad_information(int handle,void* output,uint64_t caller){
 try{const int result=pad_backend.information(handle,output);std::printf("{\"scope\":\"original_PadGetControllerInformation_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=gjP9-KQzoUk library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_information();
inline int pad_information_control(){
 if(user_service.initialize(nullptr) || pad_backend.initialize())return 1;
 const int handle=pad_backend.open(1,0,0,nullptr);if(handle<=0)return 2;
 std::array<unsigned char,30> info;info.fill(0xa5);
 if(pad_backend.information(-1,info.data()+1)!=static_cast<int>(0x80920003U) || info[1]!=0xa5)return 3;
 if(pad_backend.information(handle,nullptr)!=static_cast<int>(0x80920001U))return 4;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_information),handle,reinterpret_cast<uint64_t>(info.data()+1),0))return 5;
 if(info[0]!=0xa5 || info[29]!=0xa5 || info[9]!=30 || info[10]!=33 || info[13]>1)return 6;
 for(unsigned i=1;i<=28;++i)if(i!=9 && i!=10 && i!=12 && i!=13 && info[i])return 7;
 std::cout<<"{\"scope\":\"actual_XInput_controller_information\",\"passed\":true,\"public_ABI_bytes\":28,\"touchpad_advertised\":false}\n";return 0;
}
}
