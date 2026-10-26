// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_time.hpp"
#include "settings/native_graphics.hpp"
#include <Xinput.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include "pad_test_input.hpp"
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

// Buttons are OR-ed, triggers take the larger value, each stick the larger deflection.
inline void merge_gamepad(XINPUT_GAMEPAD& out,const XINPUT_GAMEPAD& g){
 const auto stick=[](SHORT& x,SHORT& y,SHORT nx,SHORT ny){
  if(static_cast<long long>(nx)*nx+static_cast<long long>(ny)*ny>static_cast<long long>(x)*x+static_cast<long long>(y)*y){x=nx;y=ny;}};
 out.wButtons|=g.wButtons;
 out.bLeftTrigger=(std::max)(out.bLeftTrigger,g.bLeftTrigger);out.bRightTrigger=(std::max)(out.bRightTrigger,g.bRightTrigger);
 stick(out.sThumbLX,out.sThumbLY,g.sThumbLX,g.sThumbLY);stick(out.sThumbRX,out.sThumbRY,g.sThumbRX,g.sThumbRY);
}
// Without split screen every connected XInput slot feeds the one local user. Observed case
// (2026-10-16): a Steam Controller emulated as Xbox 360 by Steamless Controller showed slots 0
// and 1 connected on one PC, with no input reaching the game while only slot 0 was read.
inline DWORD merge_xinput(const std::array<DWORD,4>& results,const std::array<XINPUT_STATE,4>& states,XINPUT_GAMEPAD& out){
 out={};DWORD merged=ERROR_DEVICE_NOT_CONNECTED;
 for(std::size_t i=0;i<results.size();++i){
  if(results[i]!=ERROR_SUCCESS)continue;
  merged=ERROR_SUCCESS;merge_gamepad(out,states[i].Gamepad);
 }
 return merged;
}

// Keyboard as a controller, in the XINPUT_GAMEPAD layout of the other devices. Default keys
// as SacredFire's runtime_pad.c (GPL-2.0-or-later) so players of both ports find the same
// layout: WASD left stick, arrow keys right stick, Space Cross, LShift Circle, E Square,
// Q Triangle, 1 L1, 3 R1, R L2, F R2, Z L3, C R3, Enter Options, Tab touch pad, IJKL d-pad.
// CTR_KEYBOARD_MAP names a file of "input=Key,Key" lines that replace an input's keys.
class KeyboardMap {
public:
 enum Input {CROSS,CIRCLE,SQUARE,TRIANGLE,L1,R1,L2,R2,L3,R3,OPTIONS,TOUCHPAD,UP,DOWN,LEFT,RIGHT,
  MOVE_UP,MOVE_DOWN,MOVE_LEFT,MOVE_RIGHT,LOOK_UP,LOOK_DOWN,LOOK_LEFT,LOOK_RIGHT,COUNT};
 static constexpr const char* names[COUNT]={"cross","circle","square","triangle","l1","r1","l2","r2","l3","r3","options","touchpad",
  "up","down","left","right","move_up","move_down","move_left","move_right","look_up","look_down","look_left","look_right"};
 std::array<std::vector<int>,COUNT> keys;
 KeyboardMap(){
  const int defaults[COUNT]={VK_SPACE,VK_LSHIFT,'E','Q','1','3','R','F','Z','C',VK_RETURN,VK_TAB,'I','K','J','L','W','S','A','D',VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};
  for(int i=0;i<COUNT;++i)keys[i]={defaults[i]};
 }
 // Virtual-key code of a key name (letters, digits, F1-F24, named keys), 0 when unknown.
 static int key_code(std::string name){
  for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if(name.size()==1 && std::isalnum(static_cast<unsigned char>(name[0])))return std::toupper(static_cast<unsigned char>(name[0]));
  if(name.size()>=2 && name.size()<=3 && name[0]=='f' && std::all_of(name.begin()+1,name.end(),[](char c){return std::isdigit(static_cast<unsigned char>(c))!=0;})){
   const int n=std::stoi(name.substr(1));if(n>=1 && n<=24)return VK_F1+n-1;
  }
  if(name.size()==7 && name.rfind("numpad",0)==0 && std::isdigit(static_cast<unsigned char>(name[6])))return VK_NUMPAD0+(name[6]-'0');
  static const std::pair<const char*,int> named[]={{"space",VK_SPACE},{"enter",VK_RETURN},{"return",VK_RETURN},{"tab",VK_TAB},
   {"backspace",VK_BACK},{"escape",VK_ESCAPE},{"shift",VK_SHIFT},{"lshift",VK_LSHIFT},{"rshift",VK_RSHIFT},{"ctrl",VK_CONTROL},
   {"lctrl",VK_LCONTROL},{"rctrl",VK_RCONTROL},{"alt",VK_MENU},{"lalt",VK_LMENU},{"ralt",VK_RMENU},{"up",VK_UP},{"down",VK_DOWN},
   {"left",VK_LEFT},{"right",VK_RIGHT},{"insert",VK_INSERT},{"delete",VK_DELETE},{"home",VK_HOME},{"end",VK_END},
   {"pageup",VK_PRIOR},{"pagedown",VK_NEXT},{"capslock",VK_CAPITAL}};
  for(const auto& [n,code]:named)if(name==n)return code;
  return 0;
 }
 // Applies "input=Key,Key" lines; returns the lines it could not use (logged by the caller).
 std::vector<std::string> load(std::istream& in){
  std::vector<std::string> rejected;std::string line;
  while(std::getline(in,line)){
   if(!line.empty() && line.back()=='\r')line.pop_back();
   if(line.empty() || line[0]=='#' || line[0]==';')continue;
   const auto eq=line.find('=');int input=-1;
   if(eq!=std::string::npos)for(int i=0;i<COUNT;++i)if(line.compare(0,eq,names[i])==0 && std::strlen(names[i])==eq)input=i;
   if(input<0){rejected.push_back(line);continue;}
   std::vector<int> codes;std::stringstream list(line.substr(eq+1));std::string key;bool bad=false;
   while(std::getline(list,key,',')){
    key.erase(0,key.find_first_not_of(" \t"));key.erase(key.find_last_not_of(" \t")+1);
    if(key.empty())continue;
    if(const int code=key_code(key))codes.push_back(code);else bad=true;
   }
   if(bad){rejected.push_back(line);continue;}
   keys[input]=codes;
  }
  return rejected;
 }
 // The pad the held keys describe; a held direction key moves its stick all the way.
 template<class Down> XINPUT_GAMEPAD state(Down down) const {
  const auto held=[&](int input){for(int code:keys[input])if(down(code))return true;return false;};
  XINPUT_GAMEPAD g{};
  const std::pair<int,int> buttons[]={{CROSS,XINPUT_GAMEPAD_A},{CIRCLE,XINPUT_GAMEPAD_B},{SQUARE,XINPUT_GAMEPAD_X},{TRIANGLE,XINPUT_GAMEPAD_Y},
   {L1,XINPUT_GAMEPAD_LEFT_SHOULDER},{R1,XINPUT_GAMEPAD_RIGHT_SHOULDER},{L3,XINPUT_GAMEPAD_LEFT_THUMB},{R3,XINPUT_GAMEPAD_RIGHT_THUMB},
   {OPTIONS,XINPUT_GAMEPAD_START},{TOUCHPAD,XINPUT_GAMEPAD_BACK},{UP,XINPUT_GAMEPAD_DPAD_UP},{DOWN,XINPUT_GAMEPAD_DPAD_DOWN},
   {LEFT,XINPUT_GAMEPAD_DPAD_LEFT},{RIGHT,XINPUT_GAMEPAD_DPAD_RIGHT}};
  for(const auto& [input,bit]:buttons)if(held(input))g.wButtons=static_cast<WORD>(g.wButtons|bit);
  if(held(L2))g.bLeftTrigger=255;
  if(held(R2))g.bRightTrigger=255;
  const auto axis=[&](int negative,int positive){return static_cast<SHORT>((held(negative)?-32768:0)+(held(positive)?32767:0));};
  g.sThumbLX=axis(MOVE_LEFT,MOVE_RIGHT);g.sThumbLY=axis(MOVE_DOWN,MOVE_UP);
  g.sThumbRX=axis(LOOK_LEFT,LOOK_RIGHT);g.sThumbRY=axis(LOOK_DOWN,LOOK_UP);
  return g;
 }
};

// One input device: XInput slot 0..3 (ids 0..3), the keyboard (0x200), or a PlayStation
// controller read by ctr_gpu.dll (high bit | SDL joystick id; SDL ids start at 1, so the bare
// high bit is the merged snapshot of an older DLL without per-controller entries).
struct PadDevice {std::uint32_t id;XINPUT_GAMEPAD pad;};
constexpr std::uint32_t keyboard_device=0x200,playstation_flag=0x80000000u,playstation_merged_device=playstation_flag;
inline bool is_playstation_device(std::uint32_t id){return (id&playstation_flag)!=0;}
inline std::uint32_t sdl_joystick_id(std::uint32_t id){return id&~playstation_flag;}

// Split screen: which device plays on which controller port. Port 0 is the primary user. While
// it has no device, every unassigned device drives it; the first one that is used takes it. A
// further device then joins as the next port by holding a button for join_hold_ms, as a PS4
// player presses the PS button to log in. A device that only mirrors an assigned one (one pad
// seen through two XInput slots, see merge_xinput) never joins: its buttons always match.
// A device that disconnects leaves its port and that port's user logs out.
// The launcher can also fix a device to a port (CTR_PAD_PLAYERS): that port takes only that
// device, logs in while it is connected, and other devices never join it; a port set to
// "none" stays empty. Ports left on "auto" keep the rules above.
class PortAssigner {
 static constexpr std::uint64_t join_hold_ms=300;
 std::optional<std::uint32_t> candidate;std::uint64_t candidate_since=0;
 static bool pressed(const XINPUT_GAMEPAD& g){return g.wButtons || g.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD || g.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;}
 static bool active(const XINPUT_GAMEPAD& g){
  const auto pushed=[](SHORT x,SHORT y){return static_cast<long long>(x)*x+static_cast<long long>(y)*y>16000LL*16000;};
  return pressed(g) || pushed(g.sThumbLX,g.sThumbLY) || pushed(g.sThumbRX,g.sThumbRY);
 }
 static const PadDevice* find(const std::vector<PadDevice>& devices,std::uint32_t id){
  for(const auto& d:devices)if(d.id==id)return &d;return nullptr;
 }
public:
 struct Change {int port;std::uint32_t device;bool login;};
 std::array<std::optional<std::uint32_t>,4> owner,fixed;
 std::array<bool,4> blocked{};
 bool assigned(std::uint32_t id) const {
  for(const auto& o:owner)if(o==id)return true;
  for(const auto& f:fixed)if(f==id)return true;
  return false;
 }
 std::vector<Change> update(const std::vector<PadDevice>& devices,std::uint64_t now_ms){
  std::vector<Change> changes;
  for(int port=0;port<4;++port){
   if(fixed[port]){
    const bool present=find(devices,*fixed[port])!=nullptr;
    if(present && owner[port]!=fixed[port]){if(!owner[port])changes.push_back({port,*fixed[port],true});owner[port]=fixed[port];}
    else if(!present && owner[port]){changes.push_back({port,*owner[port],false});owner[port].reset();}
   }else if(owner[port] && (!find(devices,*owner[port]) || blocked[port])){
    changes.push_back({port,*owner[port],false});owner[port].reset();
   }
  }
  if(!owner[0] && !fixed[0]){
   for(const auto& d:devices)if(!assigned(d.id) && active(d.pad)){owner[0]=d.id;changes.push_back({0,d.id,true});break;}
   candidate.reset();return changes;
  }
  const auto mirrors=[&](const PadDevice& d){
   for(const auto& o:owner)if(o)if(const auto* a=find(devices,*o))
    if(a->pad.wButtons==d.pad.wButtons && pressed(a->pad)==pressed(d.pad))return true;
   return false;
  };
  const auto eligible=[&](const PadDevice& d){return !assigned(d.id) && pressed(d.pad) && !mirrors(d);};
  if(candidate){
   const auto* d=find(devices,*candidate);
   if(!d || !eligible(*d))candidate.reset();
   else if(now_ms-candidate_since>=join_hold_ms){
    for(int port=1;port<4;++port)if(!owner[port] && !fixed[port] && !blocked[port]){owner[port]=d->id;changes.push_back({port,d->id,true});break;}
    candidate.reset();return changes;
   }
  }
  if(!candidate)for(const auto& d:devices)if(eligible(d)){candidate=d.id;candidate_since=now_ms;break;}
  return changes;
 }
 // The pad a port reads: its device, or for port 0 without one every unassigned device.
 bool state(int port,const std::vector<PadDevice>& devices,XINPUT_GAMEPAD& out) const {
  out={};
  if(owner[port]){if(const auto* d=find(devices,*owner[port])){out=d->pad;return true;}return false;}
  if(port || fixed[0])return false;
  bool any=false;for(const auto& d:devices)if(!assigned(d.id)){merge_gamepad(out,d.pad);any=true;}
  return any;
 }
 // Devices whose rumble and light bar belong to a port (same rule as state).
 std::vector<std::uint32_t> devices_of(int port,const std::vector<PadDevice>& devices) const {
  std::vector<std::uint32_t> ids;
  if(owner[port]){ids.push_back(*owner[port]);return ids;}
  if(!port && !fixed[0])for(const auto& d:devices)if(!assigned(d.id))ids.push_back(d.id);
  return ids;
 }
};
inline std::string pad_device_name(std::uint32_t id){
 if(id==keyboard_device)return "keyboard";
 if(id==playstation_merged_device)return "playstation";
 if(is_playstation_device(id))return "playstation"+std::to_string(sdl_joystick_id(id));
 return "xinput"+std::to_string(id);
}

// New Windows backend, not a compatibility stub. Public PS4 Init lifecycle:
// pinned shadPS4 pad.cpp e3ce810f3a653f43ac64ebab63023de281a4103a.
// XInput API/types from installed Windows SDK. No fabricated connected device, motion data,
// PS4 read/open behavior or game input state. Up to four local users each open a port
// (CTR_SPLIT_SCREEN=1); without split screen every device drives the one primary user.
// The loaded renderer library, which reads PlayStation controllers: ctr_gpu.dll (Vulkan) or
// ctr_d3d.dll (Direct3D 11).
inline HMODULE renderer_library(){
 if(auto library=GetModuleHandleW(L"ctr_gpu.dll"))return library;
 return GetModuleHandleW(L"ctr_d3d.dll");
}
class PadBackend {
 using GetState=DWORD (WINAPI*)(DWORD,XINPUT_STATE*);
 HMODULE module=nullptr;GetState get_state=nullptr;std::mutex mutex;
 struct Port {int user;int index;int handle;bool connected=false;unsigned char connections=0;};
 std::vector<Port> ports;int next_handle=1;
 std::array<DWORD,4> status{};std::vector<PadDevice> devices;
 PortAssigner assigner;
 bool split=false,keyboard=true;KeyboardMap keys;
 // CTR_PAD_PLAYERS: four choices separated by '|', for players 1..4: "auto", "none",
 // "keyboard", "xinput:N" (slot 0..3) or "hid:<device path>" (a controller read by ctr_gpu).
 std::array<std::string,4> choices{"auto","auto","auto","auto"};
 std::map<std::uint32_t,std::string> device_paths;
 struct PlayStationApi {
  int(*merged)(XINPUT_GAMEPAD*)=nullptr;
  int(*list)(void*,int)=nullptr;
  void(*rumble_all)(std::uint16_t,std::uint16_t)=nullptr;
  void(*rumble_one)(std::uint32_t,std::uint16_t,std::uint16_t)=nullptr;
  void(*light)(std::uint32_t,std::uint8_t,std::uint8_t,std::uint8_t)=nullptr;
  int(*path)(std::uint32_t,char*,int)=nullptr;
 };
 // The renderer library reads PlayStation controllers through SDL's HID drivers in the XINPUT_GAMEPAD
 // layout (ctrport/gpu/ctr_gpu.h). Looked up until the DLL is loaded.
 static const PlayStationApi& playstation(){
  static PlayStationApi api;
  if(!api.merged)if(auto library=renderer_library()){
   api.list=reinterpret_cast<int(*)(void*,int)>(GetProcAddress(library,"ctr_gpu_gamepad_list"));
   api.rumble_all=reinterpret_cast<void(*)(std::uint16_t,std::uint16_t)>(GetProcAddress(library,"ctr_gpu_gamepad_rumble"));
   api.rumble_one=reinterpret_cast<void(*)(std::uint32_t,std::uint16_t,std::uint16_t)>(GetProcAddress(library,"ctr_gpu_gamepad_rumble_one"));
   api.light=reinterpret_cast<void(*)(std::uint32_t,std::uint8_t,std::uint8_t,std::uint8_t)>(GetProcAddress(library,"ctr_gpu_gamepad_light"));
   api.path=reinterpret_cast<int(*)(std::uint32_t,char*,int)>(GetProcAddress(library,"ctr_gpu_gamepad_path"));
   api.merged=reinterpret_cast<int(*)(XINPUT_GAMEPAD*)>(GetProcAddress(library,"ctr_gpu_gamepad_state"));
  }
  return api;
 }
 // Keys count only while one of this process's windows is in front.
 static bool focused(){
  DWORD process=0;const HWND foreground=GetForegroundWindow();
  return foreground && GetWindowThreadProcessId(foreground,&process) && process==GetCurrentProcessId();
 }
 // Polls every device (caller holds the mutex); in split screen also assigns ports and logs
 // the matching local users in and out.
 void poll(const char* failure){
  devices.clear();
  for(DWORD i=0;i<4;++i){
   XINPUT_STATE state{};const DWORD result=get_state(i,&state);
   if(result!=ERROR_SUCCESS && result!=ERROR_DEVICE_NOT_CONNECTED)throw std::runtime_error(std::string(failure)+" status="+std::to_string(result));
   status[i]=result;if(result==ERROR_SUCCESS)devices.push_back({i,state.Gamepad});
  }
  const auto& ps=playstation();
  if(ps.list){
   struct Entry {std::uint32_t id;XINPUT_GAMEPAD pad;};static_assert(sizeof(Entry)==16);
   std::array<Entry,8> entries{};const int count=ps.list(entries.data(),static_cast<int>(entries.size()));
   for(int i=0;i<count;++i)devices.push_back({playstation_flag|entries[i].id,entries[i].pad});
  }else if(ps.merged){XINPUT_GAMEPAD pad{};if(ps.merged(&pad)>0)devices.push_back({playstation_merged_device,pad});}
  if(keyboard){
   const bool front=focused();
   devices.push_back({keyboard_device,front?keys.state([](int code){return (GetAsyncKeyState(code)&0x8000)!=0;}):XINPUT_GAMEPAD{}});
  }
  resolve_choices();
  if(!split)return;
  for(const auto& change:assigner.update(devices,GetTickCount64())){
   std::printf("{\"scope\":\"split_screen_port\",\"port\":%d,\"device\":\"%s\",\"assigned\":%s}\n",change.port,pad_device_name(change.device).c_str(),change.login?"true":"false");std::fflush(stdout);
   if(change.port && user_service.set_extra_user(change.port,change.login)){
    std::printf("{\"scope\":\"split_screen_user\",\"user\":%d,\"login\":%s}\n",UserService::extra_user_id(change.port),change.login?"true":"false");std::fflush(stdout);
   }
  }
 }
 // The device each launcher choice names right now (a PlayStation controller's SDL id changes
 // when it reconnects, so paths are matched again at every poll).
 void resolve_choices(){
  const auto& ps=playstation();
  for(int port=0;port<4;++port){
   const std::string& choice=choices[port];
   std::optional<std::uint32_t> device;
   if(choice=="keyboard")device=keyboard_device;
   else if(choice.rfind("xinput:",0)==0)device=static_cast<std::uint32_t>(std::atoi(choice.c_str()+7)&3);
   else if(choice.rfind("hid:",0)==0){
    device=0x7fffffffu;  // not connected
    for(const auto& d:devices)if(is_playstation_device(d.id) && d.id!=playstation_merged_device){
     auto known=device_paths.find(d.id);
     if(known==device_paths.end()){
      char path[1024]{};if(ps.path)ps.path(sdl_joystick_id(d.id),path,sizeof(path));
      known=device_paths.emplace(d.id,path).first;
     }
     if(!known->second.empty() && _stricmp(known->second.c_str(),choice.c_str()+4)==0){device=d.id;break;}
    }
   }
   assigner.fixed[port]=device;
   assigner.blocked[port]=port && choice=="none";
  }
 }
 // The pad of a controller port after polling.
 bool port_state(int index,XINPUT_GAMEPAD& out) const {
  if(split)return assigner.state(index,devices,out);
  if(assigner.fixed[0]){out={};for(const auto& d:devices)if(d.id==*assigner.fixed[0]){out=d.pad;return true;}return false;}
  out={};bool any=false;for(const auto& d:devices){merge_gamepad(out,d.pad);any=true;}return any;
 }
 std::vector<std::uint32_t> port_devices(int index) const {
  if(split)return assigner.devices_of(index,devices);
  if(assigner.fixed[0])return {*assigner.fixed[0]};
  std::vector<std::uint32_t> ids;for(const auto& d:devices)ids.push_back(d.id);return ids;
 }
 std::vector<Port>::iterator find_port(int handle){return std::find_if(ports.begin(),ports.end(),[handle](const Port& p){return p.handle==handle;});}
 static bool environment_flag(const char* name,bool fallback){
  char value[8]{};const DWORD length=GetEnvironmentVariableA(name,value,sizeof(value));
  return length==1?value[0]=='1':fallback;
 }
 void load_keyboard_map(){
  char path[MAX_PATH]{};const DWORD length=GetEnvironmentVariableA("CTR_KEYBOARD_MAP",path,sizeof(path));
  if(!length || length>=sizeof(path))return;
  std::ifstream file(path);
  if(!file){std::printf("{\"scope\":\"keyboard_map\",\"file\":\"missing\"}\n");std::fflush(stdout);return;}
  const auto rejected=keys.load(file);
  std::printf("{\"scope\":\"keyboard_map\",\"file\":\"loaded\",\"rejected_lines\":%zu}\n",rejected.size());
  for(const auto& line:rejected)std::fprintf(stderr,"ctrport: keyboard map line not used: %s\n",line.c_str());
  std::fflush(stdout);
 }
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
  std::array<DWORD,4> prepared_status{};
  for(DWORD i=0;i<4;++i){XINPUT_STATE prepared{};prepared_status[i]=call(i,&prepared);
   if(prepared_status[i]!=ERROR_SUCCESS && prepared_status[i]!=ERROR_DEVICE_NOT_CONNECTED){const auto error=prepared_status[i];FreeLibrary(candidate);throw std::runtime_error("Pad XInput polling failed status="+std::to_string(error));}
  }
  module=candidate;get_state=call;status=prepared_status;
  split=environment_flag("CTR_SPLIT_SCREEN",false);keyboard=environment_flag("CTR_KEYBOARD",true);
  if(keyboard)load_keyboard_map();
  char players[4096]{};
  if(const DWORD length=GetEnvironmentVariableA("CTR_PAD_PLAYERS",players,sizeof(players));length && length<sizeof(players)){
   std::stringstream list(players);std::string choice;
   for(int port=0;port<4 && std::getline(list,choice,'|');++port)if(!choice.empty())choices[port]=choice;
   std::printf("{\"scope\":\"pad_players\",\"player1\":\"%s\",\"player2\":\"%s\",\"player3\":\"%s\",\"player4\":\"%s\"}\n",
               choices[0].substr(0,64).c_str(),choices[1].substr(0,64).c_str(),choices[2].substr(0,64).c_str(),choices[3].substr(0,64).c_str());
   std::fflush(stdout);
  }
  return 0;
 }
 // Test configuration of an isolated backend (controls only).
 void configure(bool split_screen,bool use_keyboard){std::lock_guard<std::mutex> guard(mutex);split=split_screen;keyboard=use_keyboard;}
 int open(int user,int type,int index,const void* params){
  std::lock_guard<std::mutex> guard(mutex);
  if(!module || !get_state)return static_cast<int>(0x80920005U);
  if(user<0 || user==255)return static_cast<int>(0x809b0001U);
  const int port_index=user_service.port_of_user(user);
  if(port_index<0)return static_cast<int>(0x809b0081U);
  if(type!=0 || index!=0 || params)throw std::runtime_error("PadOpen only observed standard port0/index0/null parameters implemented");
  for(const auto& port:ports)if(port.user==user)return static_cast<int>(0x80920004U);
  poll("PadOpen real XInput poll failed");
  XINPUT_GAMEPAD pad{};const bool connected=port_state(port_index,pad);
  const int handle=next_handle++;ports.push_back({user,port_index,handle,connected,static_cast<unsigned char>(connected?1:0)});return handle;
 }
 // scePadClose: pinned shadPS4 pad.cpp46-58, an unknown handle is ORBIS_PAD_ERROR_INVALID_HANDLE.
 int close(int handle){
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=find_port(handle);
  if(port==ports.end())return static_cast<int>(0x80920003U);
  ports.erase(port);return 0;
 }
 // Public disconnected sample: pinned pad.cpp ProcessStates368-374/readState467-470,
 // pad.h OrbisPadData size120, quaternion offset12/w24, connected76.
 // GPL-2.0-or-later upstream e3ce810f3a653f43ac64ebab63023de281a4103a.
 int read_state(int handle,void* output){
  if(!output)return static_cast<int>(0x80920001U);
  std::lock_guard<std::mutex> guard(mutex);
  auto port=find_port(handle);
  if(port==ports.end())return static_cast<int>(0x80920003U);
  poll("XInput read failed");port=find_port(handle);
  XINPUT_GAMEPAD pad{};const bool connected=port_state(port->index,pad);
  std::array<unsigned char,120> sample{};const float identity=1.0f;std::memcpy(sample.data()+24,&identity,4);
  if(connected){
   if(port->index==0)apply_pad_test_input(pad);
   // Host exit confirmation consumes input; preserve physical connection and time.
   using CaptureQuery=int(*)();
   static CaptureQuery capture=[] {auto library=renderer_library();return library?reinterpret_cast<CaptureQuery>(GetProcAddress(library,"ctr_gpu_input_captured")):nullptr;}();
   if(capture && capture())pad={};
   if(!port->connected){if(port->connections!=255)++port->connections;}
   sample=pad_connected_sample(pad,port->connections,ctr_kernel_process_time());
  }
  port->connected=connected;
  require_time_output(output,sample.size());std::memcpy(output,sample.data(),sample.size());return 0;
 }
 // scePadSetVibration (shadPS4 pad.cpp651-664; OrbisPadVibrationParam {u8 largeMotor;
 // u8 smallMotor}): XInput low-frequency (left) motor = large, high-frequency = small.
 int set_vibration(int handle,const void* parameter){
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=find_port(handle);
  if(port==ports.end())return static_cast<int>(0x80920003U);
  if(!parameter)return static_cast<int>(0x80920001U);
  unsigned char motors[2]{};SIZE_T read=0;
  if(!ReadProcessMemory(GetCurrentProcess(),parameter,motors,2,&read) || read!=2)return static_cast<int>(0x80920001U);
  using SetState=DWORD (WINAPI*)(DWORD,XINPUT_VIBRATION*);
  static const SetState set_state=[this]{FARPROC symbol=module?GetProcAddress(module,"XInputSetState"):nullptr;SetState call=nullptr;std::memcpy(&call,&symbol,sizeof(call));return call;}();
  if(!port->connected)return 0;
  const auto low=static_cast<std::uint16_t>(motors[0]*257),high=static_cast<std::uint16_t>(motors[1]*257);
  const auto& ps=playstation();
  for(const auto id:port_devices(port->index)){
   XINPUT_VIBRATION vibration{low,high};
   if(id<4){if(set_state)set_state(id,&vibration);}
   else if(id==playstation_merged_device || (is_playstation_device(id) && !ps.rumble_one)){if(ps.rumble_all)ps.rumble_all(low,high);}
   else if(is_playstation_device(id))ps.rumble_one(sdl_joystick_id(id),low,high);
  }
  return 0;
 }
 // scePadSetLightBar / scePadResetLightBar: pinned shadPS4 pad.cpp478-496 and 566-586
 // (OrbisPadLightBarParam {u8 r,g,b,reserved}; all three components below 0x0d are
 // ORBIS_PAD_ERROR_INVALID_LIGHTBAR_SETTING; reset shows the user colour of the port: blue,
 // red, green, pink, input/controller.h g_user_colours). Only PlayStation controllers have
 // a light bar; for the other devices of the port the call has no visible effect.
 int set_light_bar(int handle,const void* parameter){
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=find_port(handle);
  if(port==ports.end())return static_cast<int>(0x80920003U);
  if(!parameter)return static_cast<int>(0x80920001U);
  unsigned char colour[3]{};SIZE_T read=0;
  if(!ReadProcessMemory(GetCurrentProcess(),parameter,colour,3,&read) || read!=3)return static_cast<int>(0x80920001U);
  if(colour[0]<0xd && colour[1]<0xd && colour[2]<0xd)return static_cast<int>(0x80920006U);
  light(port->index,colour[0],colour[1],colour[2]);return 0;
 }
 int reset_light_bar(int handle){
  std::lock_guard<std::mutex> guard(mutex);
  const auto port=find_port(handle);
  if(port==ports.end())return static_cast<int>(0x80920003U);
  static constexpr unsigned char colours[4][3]={{0,0,255},{255,0,0},{0,255,0},{255,0,255}};
  const auto& c=colours[port->index];light(port->index,c[0],c[1],c[2]);return 0;
 }
 void light(int index,unsigned char r,unsigned char g,unsigned char b){
  const auto& ps=playstation();if(!ps.light)return;
  for(const auto id:port_devices(index))if(is_playstation_device(id) && id!=playstation_merged_device)ps.light(sdl_joystick_id(id),r,g,b);
 }
 // Public 28-byte controller information, backed by the port's actual devices.
 // No touchpad capability is advertised for an XInput standard controller.
 int information(int handle,void* output){
  if(!output)return static_cast<int>(0x80920001U);
  std::lock_guard<std::mutex> guard(mutex);
  auto port=find_port(handle);
  if(port==ports.end())return static_cast<int>(0x80920003U);
  poll("XInput information poll failed");port=find_port(handle);
  XINPUT_GAMEPAD pad{};const bool connected=port_state(port->index,pad);
  if(connected && !port->connected && port->connections!=255)++port->connections;
  port->connected=connected;
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
 // A split-screen user opens its own port only while logged in; closing frees the user's port.
 const int extra=UserService::extra_user_id(1);
 if(isolated.open(extra,0,0,nullptr)!=static_cast<int>(0x809b0081U) || !user_service.set_extra_user(1,true))return 2;
 const int second=isolated.open(extra,0,0,nullptr);
 if(second<=0 || second==handle || isolated.open(extra,0,0,nullptr)!=static_cast<int>(0x80920004U))return 3;
 if(isolated.close(second) || isolated.close(second)!=static_cast<int>(0x80920003U) || isolated.open(extra,0,0,nullptr)<=0)return 4;
 if(!user_service.set_extra_user(1,false) || isolated.close(handle))return 5;
 std::cout<<"{\"scope\":\"real_XInput_port_open\",\"passed\":true,\"handle_owned\":true,\"split_screen_user_port\":true,\"close\":true,\"connected_devices\":"<<isolated.connected_count()<<",\"no_fabricated_connection\":true}\n";return 0;
}
}

namespace ctrport {
extern "C" int ctr_pad_read_state(int handle,void* output,uint64_t caller){
 settings::native_graphics::apply();
 try{const int result=pad_backend.read_state(handle,output);if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"original_PadReadState_return\",\"caller\":%llu,\"result\":%d,\"connected\":%s}\n",static_cast<unsigned long long>(caller),result,(!result && static_cast<unsigned char*>(output)[76])?"true":"false");std::fflush(stdout);}return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=YndgXqQVV7c library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_read_state();
extern "C" int ctr_pad_set_vibration(int handle,const void* parameter,uint64_t caller){
 try{return pad_backend.set_vibration(handle,parameter);
 }catch(const std::exception& e){std::fprintf(stderr,"NID=yFVnOdGxvZY library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_set_vibration();
extern "C" int ctr_pad_close(int handle,uint64_t caller){
 try{const int result=pad_backend.close(handle);std::printf("{\"scope\":\"original_PadClose_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",static_cast<unsigned long long>(caller),handle,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=6ncge5+l5Qs library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_close();
extern "C" int ctr_pad_set_light_bar(int handle,const void* parameter,uint64_t caller){
 try{const int result=pad_backend.set_light_bar(handle,parameter);std::printf("{\"scope\":\"original_PadSetLightBar_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",static_cast<unsigned long long>(caller),handle,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=RR4novUEENY library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_set_light_bar();
extern "C" int ctr_pad_reset_light_bar(int handle,uint64_t caller){
 try{const int result=pad_backend.reset_light_bar(handle);std::printf("{\"scope\":\"original_PadResetLightBar_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",static_cast<unsigned long long>(caller),handle,result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=DscD1i9HX1w library=libScePad caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_pad_reset_light_bar();
inline int pad_read_state_control(){
 XINPUT_GAMEPAD fixture{};fixture.wButtons=XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_START;
 fixture.sThumbLX=-32768;fixture.sThumbLY=32767;fixture.sThumbRX=32767;fixture.sThumbRY=-32768;fixture.bLeftTrigger=31;fixture.bRightTrigger=30;
 const auto mapped=pad_connected_sample(fixture,2,0x123456789ULL);uint32_t bits=0;uint64_t time=0;std::memcpy(&bits,mapped.data(),4);std::memcpy(&time,mapped.data()+80,8);
 if(bits!=0x4188 || mapped[4]!=0 || mapped[5]!=0 || mapped[6]!=255 || mapped[7]!=255 || mapped[8]!=31 || mapped[9]!=30 || mapped[76]!=1 || mapped[104]!=2 || time!=0x123456789ULL)return 7;
 fixture={};const auto centered=pad_connected_sample(fixture,1,1);if(centered[4]!=128 || centered[6]!=128 || centered[5]!=127 || centered[7]!=127)return 8;
 {
  // Two connected slots (idle slot 0, active slot 1) and a disconnected slot 2 with stale data.
  std::array<DWORD,4> results{ERROR_SUCCESS,ERROR_SUCCESS,ERROR_DEVICE_NOT_CONNECTED,ERROR_DEVICE_NOT_CONNECTED};
  std::array<XINPUT_STATE,4> states{};
  states[0].Gamepad.wButtons=XINPUT_GAMEPAD_START;states[0].Gamepad.sThumbLX=1000;states[0].Gamepad.bLeftTrigger=40;
  states[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;states[1].Gamepad.sThumbLX=-32768;states[1].Gamepad.sThumbRY=20000;states[1].Gamepad.bRightTrigger=255;
  states[2].Gamepad.wButtons=XINPUT_GAMEPAD_B;states[2].Gamepad.sThumbRX=32767;
  XINPUT_GAMEPAD merged{};
  if(merge_xinput(results,states,merged)!=ERROR_SUCCESS || merged.wButtons!=(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_A) || merged.sThumbLX!=-32768 ||
     merged.sThumbRX!=0 || merged.sThumbRY!=20000 || merged.bLeftTrigger!=40 || merged.bRightTrigger!=255)return 9;
  results.fill(ERROR_DEVICE_NOT_CONNECTED);
  if(merge_xinput(results,states,merged)!=ERROR_DEVICE_NOT_CONNECTED || merged.wButtons)return 10;
 }
 if(user_service.initialize(nullptr) || pad_backend.initialize())return 1;
 const int handle=pad_backend.open(1,0,0,nullptr);if(handle<=0)return 2;
 std::array<unsigned char,122> output;output.fill(0xa5);
 if(pad_backend.read_state(-1,output.data()+1)!=static_cast<int>(0x80920003U) || output[1]!=0xa5)return 3;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_read_state),handle,reinterpret_cast<uint64_t>(output.data()+1),0))return 4;
 if(!output[77])for(size_t i=1;i<121;++i){const unsigned char expected=i==27?0x80:i==28?0x3f:0;if(output[i]!=expected)return 5;}
 if(output[0]!=0xa5 || output[121]!=0xa5)return 6;
 std::cout<<"{\"scope\":\"actual_XInput_sample\",\"passed\":true,\"connected_translation\":true}\n";return 0;
}
// Synthetic devices (no hardware): split-screen port assignment and the keyboard map.
inline int pad_split_screen_control(){
 const auto pad=[](WORD buttons,SHORT lx=0){XINPUT_GAMEPAD g{};g.wButtons=buttons;g.sThumbLX=lx;return g;};
 PortAssigner a;std::vector<PadDevice> devices{{0,pad(0)},{1,pad(0)},{keyboard_device,pad(0)}};
 XINPUT_GAMEPAD out{};
 // Idle devices: port 0 reads all of them, nobody owns it yet, ports 1..3 are disconnected.
 if(!a.update(devices,0).empty() || !a.state(0,devices,out) || a.state(1,devices,out))return 1;
 // The first device used takes port 0; its input is no longer merged with the others.
 devices[1].pad=pad(0,30000);
 auto changes=a.update(devices,10);
 if(changes.size()!=1 || changes[0].port!=0 || changes[0].device!=1 || !changes[0].login || a.owner[0]!=1u)return 2;
 devices[1].pad=pad(0);devices[0].pad=pad(XINPUT_GAMEPAD_A);
 if(!a.state(0,devices,out) || out.wButtons)return 3;
 // Slot 0 holds A: a candidate, joining as port 1 only after the hold time.
 if(!a.update(devices,20).empty() || !a.update(devices,200).empty())return 4;
 changes=a.update(devices,330);
 if(changes.size()!=1 || changes[0].port!=1 || changes[0].device!=0 || !changes[0].login || !a.state(1,devices,out) || out.wButtons!=XINPUT_GAMEPAD_A)return 5;
 // A short press does not join; a device mirroring an assigned one never does.
 devices[2].pad=pad(XINPUT_GAMEPAD_B);
 if(!a.update(devices,400).empty())return 6;
 devices[2].pad=pad(0);
 if(!a.update(devices,500).empty() || !a.update(devices,900).empty() || a.assigned(keyboard_device))return 7;
 devices[2].pad=pad(XINPUT_GAMEPAD_A);
 if(!a.update(devices,1000).empty() || !a.update(devices,1400).empty() || a.assigned(keyboard_device))return 8;
 // Keyboard holds Start alone: joins as port 2. Slot 0 disconnects: port 1 is freed.
 devices[0].pad=pad(0);devices[2].pad=pad(XINPUT_GAMEPAD_START);
 a.update(devices,1500);changes=a.update(devices,1900);
 if(changes.size()!=1 || changes[0].port!=2 || changes[0].device!=keyboard_device)return 9;
 devices.erase(devices.begin());
 changes=a.update(devices,2000);
 if(changes.size()!=1 || changes[0].port!=1 || changes[0].login || a.owner[1] || a.state(1,devices,out))return 10;
 const auto ids=a.devices_of(2,devices);if(ids.size()!=1 || ids[0]!=keyboard_device)return 11;
 // Port 0's device disconnects: port 0 again reads every unassigned device.
 devices.erase(devices.begin());
 changes=a.update(devices,2100);
 if(changes.size()!=1 || changes[0].port!=0 || changes[0].login || a.owner[0] || a.state(0,devices,out))return 12;
 // Launcher assignment: player 1 = slot 1, player 2 = keyboard, player 3 = none, player 4 auto.
 PortAssigner f;f.fixed[0]=1u;f.fixed[1]=keyboard_device;f.blocked[2]=true;
 std::vector<PadDevice> fixed_devices{{0,pad(0)},{keyboard_device,pad(0)}};
 changes=f.update(fixed_devices,0);
 // Slot 1 is not connected: player 1 has no input and slot 0 does not take its place.
 if(changes.size()!=1 || changes[0].port!=1 || changes[0].device!=keyboard_device || !changes[0].login || f.state(0,fixed_devices,out))return 17;
 fixed_devices[0].pad=pad(XINPUT_GAMEPAD_A,30000);
 if(!f.update(fixed_devices,10).empty() || f.owner[0])return 18;
 // Slot 0 held long enough joins the first free automatic port: 4, not the "none" port 3.
 f.update(fixed_devices,400);
 if(f.owner[3]!=0u || f.owner[2])return 19;
 // Slot 1 connects: player 1 logs in on it.
 fixed_devices.push_back({1,pad(XINPUT_GAMEPAD_B)});
 changes=f.update(fixed_devices,500);
 if(changes.size()!=1 || changes[0].port!=0 || changes[0].device!=1u || !f.state(0,fixed_devices,out) || out.wButtons!=XINPUT_GAMEPAD_B)return 20;
 // Keyboard map: defaults, remapping, rejected lines, stick extremes.
 KeyboardMap keys;
 std::istringstream file("# comment\ncross=P,Space\nr1=shift\nl2=nokey\nwrong=A\nmove_left=numpad4\n");
 const auto rejected=keys.load(file);
 if(rejected.size()!=2 || keys.keys[KeyboardMap::CROSS]!=std::vector<int>{'P',VK_SPACE} || keys.keys[KeyboardMap::R1]!=std::vector<int>{VK_SHIFT})return 13;
 if(KeyboardMap::key_code("F12")!=VK_F12 || KeyboardMap::key_code("x")!='X' || KeyboardMap::key_code("PageDown")!=VK_NEXT || KeyboardMap::key_code("f25"))return 14;
 const auto g=keys.state([](int code){return code=='P' || code=='W' || code==VK_NUMPAD4 || code=='R' || code==VK_RETURN;});
 if(g.wButtons!=(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_START) || g.bLeftTrigger!=255 || g.bRightTrigger || g.sThumbLX!=-32768 || g.sThumbLY!=32767 || g.sThumbRX || g.sThumbRY)return 15;
 const auto sample=pad_connected_sample(g,1,0);
 if(sample[4]!=0 || sample[5]!=0 || sample[6]!=128)return 16;
 std::cout<<"{\"scope\":\"pad_split_screen_control\",\"passed\":true,\"synthetic_devices\":true,\"hardware_read\":false}\n";return 0;
}
// Light bar and close through the SysV bridges on a real backend (no light bar is needed:
// the argument checks and handle ownership are what is verified).
inline int pad_light_bar_control(){
 if(user_service.initialize(nullptr) || pad_backend.initialize())return 1;
 const int handle=pad_backend.open(1,0,0,nullptr);if(handle<=0)return 2;
 const unsigned char dark[4]{0xc,0xc,0xc,0},red[4]{0xff,0,0,0};
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_set_light_bar),handle,reinterpret_cast<uint64_t>(dark),0))!=static_cast<int>(0x80920006U))return 3;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_set_light_bar),handle,reinterpret_cast<uint64_t>(red),0))return 4;
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_set_light_bar),handle,0,0))!=static_cast<int>(0x80920001U))return 5;
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_set_light_bar),handle+100,reinterpret_cast<uint64_t>(red),0))!=static_cast<int>(0x80920003U))return 6;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_reset_light_bar),handle,0,0))return 7;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_close),handle,0,0))return 8;
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_close),handle,0,0))!=static_cast<int>(0x80920003U))return 9;
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_pad_reset_light_bar),handle,0,0))!=static_cast<int>(0x80920003U))return 10;
 std::cout<<"{\"scope\":\"pad_light_bar_close_control\",\"passed\":true,\"SysV\":true,\"invalid_setting_rejected\":true,\"closed_handle_rejected\":true}\n";return 0;
}
}

namespace ctrport {
extern "C" int ctr_pad_information(int handle,void* output,uint64_t caller){
 try{const int result=pad_backend.information(handle,output);if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"original_PadGetControllerInformation_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);}return result;}
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
