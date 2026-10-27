// SPDX-License-Identifier: GPL-2.0-or-later
// Public Init/duplicate contract: shadPS4 audioout.cpp207, revision
// e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
// Independent Windows XAudio2 endpoint owner. No guest port/output stub.
#pragma once
#include <Windows.h>
#include <xaudio2.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <memory>
#include <condition_variable>
#include <thread>
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"ole32.lib")
namespace ctrport {
class AudioOutputBackend {
 std::mutex mutex;std::condition_variable wake;std::thread owner;
 // XAudio2 signals the end of every block on its own thread; output() waits on that
 // event instead of polling with Sleep(1), whose 15.6 ms default granularity starved
 // 5.3 ms (256-sample) blocks and made the sound crackle.
 struct BlockEnd:IXAudio2VoiceCallback{
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  ~BlockEnd(){if(event)CloseHandle(event);}
  void STDMETHODCALLTYPE OnBufferEnd(void*) override{SetEvent(event);}
  void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override{}
  void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override{}
  void STDMETHODCALLTYPE OnStreamEnd() override{}
  void STDMETHODCALLTYPE OnBufferStart(void*) override{}
  void STDMETHODCALLTYPE OnLoopEnd(void*) override{}
  void STDMETHODCALLTYPE OnVoiceError(void*,HRESULT) override{SetEvent(event);}
 };
 // XAudio2 reports a lost endpoint (device unplugged or disabled) on its own thread. Only a
 // flag is set there; the owner thread drops the engine, since no XAudio2 call is allowed here.
 struct EngineEvents:IXAudio2EngineCallback{
  std::atomic<HRESULT> lost{S_OK};
  void STDMETHODCALLTYPE OnProcessingPassStart() override{}
  void STDMETHODCALLTYPE OnProcessingPassEnd() override{}
  void STDMETHODCALLTYPE OnCriticalError(HRESULT error) override{lost.store(FAILED(error)?error:E_FAIL);}
 };
 static constexpr unsigned queue_depth=4,block_slots=6;
 // Without an audio output device (none at start, or lost later) an open port has no voice:
 // output() takes each block at the real 48 kHz rate on a steady clock and discards it, so
 // game pacing is unchanged. The owner thread retries the default device every second and
 // gives every open port its voice back, with its volumes, when one appears.
 struct Port {IXAudio2SourceVoice* voice=nullptr;bool open=false;WAVEFORMATEX format{};std::chrono::steady_clock::time_point clock{};std::array<float,8> gains{1,1,1,1,1,1,1,1};uint32_t length=0;int32_t user=0;uint64_t submitted_buffers=0,nonzero_buffers=0;std::vector<unsigned char> buffer;std::unique_ptr<BlockEnd> block_end;};
 std::array<Port,9> ports;IXAudio2* active_engine=nullptr;IXAudio2MasteringVoice* active_master=nullptr;EngineEvents events;
 bool stopping=false,ready=false,silent=false;uint32_t channels=0,sample_rate=0;
 std::chrono::steady_clock::time_point started{};
 // Windows reports no default render endpoint (no speakers or headphones, or all disabled).
 static bool no_device(HRESULT hr){return hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND) || hr==static_cast<HRESULT>(0x88890004L) || hr==static_cast<HRESULT>(0x88960004L);}
 static long test_value(const char* name){char text[16]{};const DWORD n=GetEnvironmentVariableA(name,text,sizeof text);return n && n<sizeof text?std::strtol(text,nullptr,10):0;}
 void release(IXAudio2*& engine,IXAudio2MasteringVoice*& master){
  if(master)master->DestroyVoice();if(engine){engine->UnregisterForCallbacks(&events);engine->Release();}
  master=nullptr;engine=nullptr;
 }
 // Owner thread: the engine and a mastering voice on the current default device.
 HRESULT connect(IXAudio2*& engine,IXAudio2MasteringVoice*& master){
  engine=nullptr;master=nullptr;
  // CTR_AUDIO_TEST_NO_DEVICE=<seconds> (tests): no device for that long after AudioOutInit.
  const long missing=test_value("CTR_AUDIO_TEST_NO_DEVICE");
  if(missing>0 && std::chrono::steady_clock::now()<started+std::chrono::seconds(missing))return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  HRESULT hr=XAudio2Create(&engine,0,XAUDIO2_DEFAULT_PROCESSOR);
  if(SUCCEEDED(hr))hr=engine->RegisterForCallbacks(&events);
  if(SUCCEEDED(hr))hr=engine->CreateMasteringVoice(&master);
  // CTR_AUDIO_MUTE=1 (test runs): zero endpoint gain only. Voices still consume every
  // block at the real rate, so AudioOutOutput pacing is unchanged.
  if(SUCCEEDED(hr) && test_value("CTR_AUDIO_MUTE")==1){hr=master->SetVolume(0.0f);std::printf("{\"scope\":\"audio_endpoint_muted\",\"result\":%ld}\n",static_cast<long>(hr));}
  if(SUCCEEDED(hr)){XAUDIO2_VOICE_DETAILS details{};master->GetVoiceDetails(&details);if(!details.InputChannels || !details.InputSampleRate)hr=E_UNEXPECTED;}
  if(FAILED(hr))release(engine,master);
  return hr;
 }
 HRESULT create_voice(Port& port){
  if(!port.block_end){port.block_end=std::make_unique<BlockEnd>();if(!port.block_end->event)return E_OUTOFMEMORY;}
  IXAudio2SourceVoice* voice=nullptr;
  HRESULT hr=active_engine->CreateSourceVoice(&voice,&port.format,0,XAUDIO2_DEFAULT_FREQ_RATIO,port.block_end.get());
  if(FAILED(hr))return hr;
  XAUDIO2_VOICE_DETAILS details{};voice->GetVoiceDetails(&details);
  if(details.InputChannels!=8 || details.InputSampleRate!=port.format.nSamplesPerSec)hr=E_UNEXPECTED;
  if(SUCCEEDED(hr))hr=voice->SetChannelVolumes(8,port.gains.data());
  if(FAILED(hr)){voice->DestroyVoice();return hr;}
  port.voice=voice;return S_OK;
 }
 // Mutex held, owner thread: every port goes silent and the engine is dropped.
 void go_silent(){
  for(auto& port:ports)if(port.voice){port.voice->DestroyVoice();port.voice=nullptr;port.clock={};}
  release(active_engine,active_master);silent=true;
 }
 // Mutex held, owner thread: the new engine takes over and every open port gets a voice.
 bool go_live(IXAudio2* engine,IXAudio2MasteringVoice* master){
  active_engine=engine;active_master=master;events.lost.store(S_OK);
  for(auto& port:ports)if(port.open && FAILED(create_voice(port))){go_silent();return false;}
  silent=false;return true;
 }
 void run(std::promise<HRESULT>& promise){
  const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  IXAudio2* engine=nullptr;IXAudio2MasteringVoice* master=nullptr;
  const HRESULT hr=FAILED(com)?com:connect(engine,master);
  if(SUCCEEDED(hr)){XAUDIO2_VOICE_DETAILS details{};master->GetVoiceDetails(&details);channels=details.InputChannels;sample_rate=details.InputSampleRate;active_engine=engine;active_master=master;}
  else if(no_device(hr)){channels=8;sample_rate=48000;silent=true;}
  const bool keep=SUCCEEDED(hr) || no_device(hr);
  promise.set_value(hr);
  if(keep){
   // CTR_AUDIO_TEST_LOSE_DEVICE=<seconds> (tests): the device is lost that long after AudioOutInit, once.
   const long lose=test_value("CTR_AUDIO_TEST_LOSE_DEVICE");bool lose_pending=lose>0;
   std::unique_lock<std::mutex> guard(mutex);
   while(!stopping){
    wake.wait_for(guard,std::chrono::seconds(1),[this]{return stopping;});
    if(stopping)break;
    HRESULT lost=events.lost.exchange(S_OK);
    if(lose_pending && !silent && std::chrono::steady_clock::now()>=started+std::chrono::seconds(lose)){lose_pending=false;lost=static_cast<HRESULT>(0x88960004L);}
    if(!silent && FAILED(lost)){
     go_silent();
     std::fprintf(stderr,"audio: audio output device lost (HRESULT=0x%08lx); the game continues without sound\n",static_cast<unsigned long>(lost));std::fflush(stderr);
     continue;
    }
    if(!silent)continue;
    guard.unlock();const HRESULT again=connect(engine,master);guard.lock();
    if(FAILED(again))continue;
    if(stopping){release(engine,master);break;}
    if(go_live(engine,master))std::fprintf(stderr,"audio: audio output device available; sound resumed\n");
    else std::fprintf(stderr,"audio: audio output device found but its voices could not be created; retrying\n");
    std::fflush(stderr);
   }
   go_silent();
  }
  if(SUCCEEDED(com))CoUninitialize();
 }
 // Mutex held: a port without a voice takes the block at the real rate and discards it.
 int silent_output(Port& port,const void* input){
  if(!input)return 0;SIZE_T copied=0;const auto bytes=port.buffer.size()/block_slots;
  unsigned char* block=port.buffer.data()+bytes*(port.submitted_buffers%block_slots);
  if(!ReadProcessMemory(GetCurrentProcess(),input,block,bytes,&copied) || copied!=bytes)return static_cast<int>(0x80260004U);
  // Same backpressure as a device: return once the block fits within queue_depth blocks
  // ahead of the clock; after a long stall the clock restarts instead of catching up in bursts.
  using namespace std::chrono;
  const auto length=duration_cast<steady_clock::duration>(duration<double>(static_cast<double>(port.length)/48000.0));
  const auto now=steady_clock::now();
  if(port.clock<now-length*queue_depth)port.clock=now;
  port.clock+=length;
  const auto ahead=port.clock-now-length*queue_depth;
  if(ahead>steady_clock::duration::zero()){mutex.unlock();std::this_thread::sleep_for(ahead);mutex.lock();if(!port.open)return static_cast<int>(0x80260001U);}
  ++port.submitted_buffers;
  return static_cast<int>(port.length*8);
 }
public:
 ~AudioOutputBackend(){ {std::lock_guard<std::mutex> lock(mutex);stopping=true;}wake.notify_all();if(owner.joinable())owner.join();}
 int initialize(){
  std::lock_guard<std::mutex> lock(mutex);
  if(ready)return static_cast<int>(0x8026000eU);
  if(owner.joinable())throw std::runtime_error("Audio endpoint initialization previously failed");
  std::promise<HRESULT> promise;auto result=promise.get_future();
  started=std::chrono::steady_clock::now();
  owner=std::thread([this,promise=std::move(promise)]()mutable{run(promise);});
  const HRESULT hr=result.get();
  if(no_device(hr)){
   std::fprintf(stderr,"audio: no audio output device (HRESULT=0x%08lx); the game runs without sound until one is connected\n",static_cast<unsigned long>(hr));std::fflush(stderr);
   ready=true;return 0;
  }
  if(FAILED(hr))throw std::runtime_error("XAudio2 real mastering endpoint failed HRESULT="+std::to_string(static_cast<unsigned long>(hr)));
  ready=true;return 0;
 }
 int open(int32_t user,int32_t type,int32_t index,uint32_t length,uint32_t rate,uint32_t param){
  std::lock_guard<std::mutex> lock(mutex);
  if(!ready)return static_cast<int>(0x8026000fU);
  if(!length || length>2048 || (length&255))return static_cast<int>(0x80260006U);
  if(param&0x7ff00000U)return static_cast<int>(0x80260007U);
  if((type<0 || type>4) && type!=126 && type!=127)return static_cast<int>(0x8026000aU);
  if(rate!=48000)return static_cast<int>(0x80260008U);
  if((param&255)>7 || ((param>>16)&15)>2)return static_cast<int>(0x80260007U);
  if((type!=0 && type!=1) || index!=0 || (param!=5 && param!=2))throw std::runtime_error("AudioOutOpen only MAIN/BGM float8 or S16_8CH index0 routing implemented");
  for(unsigned i=(type==1?8u:0u);i<(type==1?9u:8u);++i)if(!ports[i].open){
   auto& port=ports[i];
   WAVEFORMATEX& format=port.format;format.wFormatTag=param==2?WAVE_FORMAT_PCM:WAVE_FORMAT_IEEE_FLOAT;format.nChannels=8;format.nSamplesPerSec=rate;format.wBitsPerSample=param==2?16:32;format.nBlockAlign=static_cast<WORD>(8*(format.wBitsPerSample/8));format.nAvgBytesPerSec=rate*format.nBlockAlign;
   port.buffer.assign(static_cast<size_t>(length)*format.nBlockAlign*block_slots,0);port.length=length;port.user=user;
   if(!silent){const HRESULT hr=create_voice(port);if(FAILED(hr)){port=Port{};throw std::runtime_error("XAudio2 source voice failed HRESULT="+std::to_string(static_cast<unsigned long>(hr)));}}
   port.open=true;return static_cast<int>(0x20000000U|(static_cast<unsigned>(type)<<16)|i);
  }
  return static_cast<int>(0x80260005U);
 }
 int set_volume(int32_t handle,uint32_t flags,const int32_t* value){
  std::lock_guard<std::mutex> lock(mutex);if(!ready)return static_cast<int>(0x8026000fU);
  const unsigned id=static_cast<unsigned>(handle)&255;
  if((static_cast<unsigned>(handle)&0x3f000000U)!=0x20000000U || id>=25)return static_cast<int>(0x80260003U);
  if(((static_cast<unsigned>(handle)>>16)&255)>1)throw std::runtime_error("Audio volume routing only MAIN/BGM implemented");
  int32_t volume=0;SIZE_T read=0;if(!value || !ReadProcessMemory(GetCurrentProcess(),value,&volume,4,&read) || read!=4)return static_cast<int>(0x80260004U);
  if(volume>32768)return static_cast<int>(0x80260009U);
  if(id>=ports.size() || !ports[id].open)return static_cast<int>(0x80260001U);
  if(volume<0)throw std::runtime_error("Negative audio gain semantics unobserved");
  // The port keeps its gains, so a voice created after a device change gets them too.
  auto& port=ports[id];for(unsigned c=0;c<8;++c)if(flags&(1U<<c))port.gains[c]=static_cast<float>(volume)/32768.0f;
  if(port.voice && FAILED(port.voice->SetChannelVolumes(8,port.gains.data())))throw std::runtime_error("XAudio2 channel gain application failed");
  return 0;
 }
 std::array<float,8> volume_control_readback(unsigned id){std::lock_guard<std::mutex> lock(mutex);std::array<float,8> result{};if(id>=ports.size() || !ports[id].voice)throw std::runtime_error("No audio control port");ports[id].voice->GetChannelVolumes(8,result.data());return result;}
 int output(int32_t handle,const void* input){
  std::lock_guard<std::mutex> lock(mutex);if(!ready)return static_cast<int>(0x8026000fU);
  const unsigned id=static_cast<unsigned>(handle)&255;
  if((static_cast<unsigned>(handle)&0x3f000000U)!=0x20000000U || id>=25)return static_cast<int>(0x80260003U);
  if(((static_cast<unsigned>(handle)>>16)&255)>1)throw std::runtime_error("Audio output routing only MAIN/BGM implemented");
  if(id>=ports.size() || !ports[id].open)return static_cast<int>(0x80260001U);auto& port=ports[id];
  // PS4 AudioOut returns once the port can take the next block. Up to queue_depth blocks
  // (about 21 ms at 256 samples) stay queued so host scheduling jitter does not starve
  // the voice; storage rotates over block_slots because XAudio2 reads queued data in place.
  // The owner thread can take the voice away or give it back while this waits.
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);XAUDIO2_VOICE_STATE state{};
  for(;;){
   if(!port.open)return static_cast<int>(0x80260001U);
   if(!port.voice)return silent_output(port,input);
   port.voice->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);if(state.BuffersQueued<queue_depth)break;if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Audio native buffer drain timeout");
   const HANDLE event=port.block_end->event;mutex.unlock();WaitForSingleObject(event,10);mutex.lock();}
  if(!input)return 0;SIZE_T copied=0;const auto bytes=port.buffer.size()/block_slots;
  unsigned char* block=port.buffer.data()+bytes*(port.submitted_buffers%block_slots);
  if(!ReadProcessMemory(GetCurrentProcess(),input,block,bytes,&copied) || copied!=bytes)return static_cast<int>(0x80260004U);
  XAUDIO2_BUFFER buffer{};buffer.AudioBytes=static_cast<UINT32>(bytes);buffer.pAudioData=reinterpret_cast<const BYTE*>(block);buffer.PlayLength=port.length;
  HRESULT hr=port.voice->SubmitSourceBuffer(&buffer);if(SUCCEEDED(hr))hr=port.voice->Start();
  if(FAILED(hr))throw std::runtime_error("XAudio2 actual PCM submission/start failed");
  // Metadata only: bit activity is not loudness or endpoint audibility.
  const bool nonzero=std::any_of(block,block+bytes,[](unsigned char v){return v!=0;});
  ++port.submitted_buffers;if(nonzero)++port.nonzero_buffers;
  if(port.submitted_buffers<5 || !(port.submitted_buffers&(port.submitted_buffers-1))) {
   std::printf("{\"scope\":\"audio_native_PCM_activity\",\"port\":%u,\"buffer_bytes\":%llu,\"submitted_buffers\":%llu,\"nonzero_buffers\":%llu,\"played_frames_before_submit\":%llu,\"audibility_verified\":false}\n",id,static_cast<unsigned long long>(bytes),static_cast<unsigned long long>(port.submitted_buffers),static_cast<unsigned long long>(port.nonzero_buffers),static_cast<unsigned long long>(state.SamplesPlayed));
  }
  return static_cast<int>(port.length*8);
 }
 // Public close contract: shadPS4 audioout.cpp361-420 (validate handle and type,
 // NOT_OPENED for a free port, release the port).
 int close(int32_t handle){
  std::lock_guard<std::mutex> lock(mutex);if(!ready)return static_cast<int>(0x8026000fU);
  const unsigned id=static_cast<unsigned>(handle)&255;
  if((static_cast<unsigned>(handle)&0x3f000000U)!=0x20000000U || id>=25)return static_cast<int>(0x80260003U);
  const unsigned type=(static_cast<unsigned>(handle)>>16)&255;
  if(!(type<=4 || type==14 || type==126 || type==127))return static_cast<int>(0x8026000aU);
  if(id>=ports.size() || !ports[id].open)return static_cast<int>(0x80260001U);
  auto& port=ports[id];if(port.voice){port.voice->Stop();port.voice->FlushSourceBuffers();port.voice->DestroyVoice();}
  port=Port{};return 0;
 }
 uint64_t played_samples_control(unsigned id){std::lock_guard<std::mutex> lock(mutex);XAUDIO2_VOICE_STATE state{};if(id>=ports.size() || !ports[id].voice)throw std::runtime_error("No audio sample control port");ports[id].voice->GetState(&state);return state.SamplesPlayed;}
 bool without_device(){std::lock_guard<std::mutex> lock(mutex);return silent;}
 std::pair<uint32_t,uint32_t> format(){std::lock_guard<std::mutex> lock(mutex);if(!ready)throw std::runtime_error("Audio output not initialized");return {channels,sample_rate};}
};
inline AudioOutputBackend audio_output_backend;
extern "C" int ctr_audio_out_init(uint64_t caller){
 try{int rc=audio_output_backend.initialize();auto format=audio_output_backend.format();
 std::printf("{\"scope\":\"original_AudioOutInit_return\",\"caller\":%llu,\"result\":%d,\"real_mastering_endpoint\":%s,\"channels\":%u,\"sample_rate\":%u,\"guest_PCM_submitted\":false}\n",static_cast<unsigned long long>(caller),rc,audio_output_backend.without_device()?"false":"true",format.first,format.second);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=JfEPXVxhFqA library=libSceAudioOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" int ctr_audio_out_open(int32_t user,int32_t type,int32_t index,uint32_t length,uint32_t rate,uint32_t param,uint64_t caller){
 std::printf("{\"scope\":\"AudioOutOpen_arguments\",\"caller\":%llu,\"user\":%d,\"type\":%d,\"index\":%d,\"length\":%u,\"rate\":%u,\"param\":%u}\n",static_cast<unsigned long long>(caller),user,type,index,length,rate,param);std::fflush(stdout);
 try{int rc=audio_output_backend.open(user,type,index,length,rate,param);std::printf("{\"scope\":\"original_AudioOutOpen_return\",\"caller\":%llu,\"user\":%d,\"length\":%u,\"rate\":%u,\"param\":%u,\"result\":%d,\"guest_PCM\":false}\n",static_cast<unsigned long long>(caller),user,length,rate,param,rc);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=ekNvsT22rsY library=libSceAudioOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_audio_out_open();
extern "C" int ctr_audio_out_open_fixture(int32_t,int32_t,int32_t,uint32_t,uint32_t,uint32_t);
inline int audio_port_control(){
 AudioOutputBackend backend;if(backend.open(255,0,0,256,48000,5)!=static_cast<int>(0x8026000fU) || backend.initialize())return 1;
 if(backend.open(255,0,0,255,48000,5)!=static_cast<int>(0x80260006U) || backend.open(255,0,0,256,44100,5)!=static_cast<int>(0x80260008U))return 1;
 for(unsigned i=0;i<8;++i)if(backend.open(255,0,0,256,48000,5)!=static_cast<int>(0x20000000U|i))return 1;
 if(backend.open(255,0,0,256,48000,5)!=static_cast<int>(0x80260005U))return 1;
 if(backend.open(255,1,0,256,48000,5)!=0x20010008 || backend.open(255,1,0,256,48000,5)!=static_cast<int>(0x80260005U))return 1;
 if(audio_output_backend.initialize() || ctr_audio_out_open_fixture(255,0,0,256,48000,5)!=0x20000000)return 1;
 std::cout<<"{\"scope\":\"audio_port_control\",\"SysV6\":true,\"real_float8_source_voices\":10,\"owned_buffers\":true,\"exhaustion\":true,\"guest_PCM\":false,\"passed\":true}\n";return 0;
}
extern "C" int ctr_audio_out_volume(int32_t handle,uint32_t flags,const int32_t* volume,uint64_t caller){
 try{int rc=audio_output_backend.set_volume(handle,flags,volume);std::printf("{\"scope\":\"original_AudioOutSetVolume_return\",\"caller\":%llu,\"flags\":%u,\"result\":%d,\"native_channel_gains\":true}\n",static_cast<unsigned long long>(caller),flags,rc);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=b+uAV89IlxE library=libSceAudioOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_audio_out_volume();
inline int audio_volume_control(){
 if(audio_output_backend.initialize())return 1;const int handle=audio_output_backend.open(255,0,0,256,48000,5);int32_t volume=16384;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_audio_out_volume),handle,0x55,reinterpret_cast<uint64_t>(&volume)))return 1;
 auto gains=audio_output_backend.volume_control_readback(0);for(unsigned c=0;c<8;++c)if(gains[c]!=((0x55&(1U<<c))?0.5f:1.0f))return 1;
 volume=32769;if(audio_output_backend.set_volume(handle,255,&volume)!=static_cast<int>(0x80260009U))return 1;
 if(audio_output_backend.set_volume(handle,255,nullptr)!=static_cast<int>(0x80260004U) || audio_output_backend.set_volume(0,255,&volume)!=static_cast<int>(0x80260003U))return 1;
 if(audio_output_backend.volume_control_readback(0)!=gains)return 1;
 std::cout<<"{\"scope\":\"audio_volume_control\",\"SysV3\":true,\"native_8channel_readback\":true,\"selective_gain\":true,\"negative_cases\":3,\"guest_PCM\":false,\"passed\":true}\n";return 0;
}
extern "C" int ctr_audio_out_output(int32_t handle,const void* input,uint64_t caller){
 try{int rc=audio_output_backend.output(handle,input);static std::atomic<uint64_t> calls{0};const auto count=++calls;
 if(count<5 || !(count&(count-1)))std::printf("{\"scope\":\"original_AudioOutOutput_return\",\"caller\":%llu,\"result\":%d,\"native_PCM_submitted\":%s,\"call_count\":%llu}\n",static_cast<unsigned long long>(caller),rc,rc>0?"true":"false",static_cast<unsigned long long>(count));std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=QOQtbeDqsT4 library=libSceAudioOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_audio_out_output();
extern "C" int ctr_audio_out_close(int32_t handle,uint64_t caller){
 try{const int rc=audio_output_backend.close(handle);std::printf("{\"scope\":\"original_AudioOutClose_return\",\"caller\":%llu,\"handle\":%d,\"result\":%d}\n",static_cast<unsigned long long>(caller),handle,rc);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=s1--uE9mBFw library=libSceAudioOut caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_audio_out_close();
inline int audio_pcm_control(){
 if(audio_output_backend.initialize())return 1;const int handle=audio_output_backend.open(255,0,0,256,48000,5);std::array<float,2048> pcm{};
 const auto call=[&](const void* data){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_audio_out_output),handle,reinterpret_cast<uint64_t>(data)));};
 if(call(pcm.data())!=2048 || call(nullptr) || audio_output_backend.played_samples_control(0)!=256)return 1;
 if(call(pcm.data())!=2048 || call(nullptr) || audio_output_backend.played_samples_control(0)!=512)return 1;
 if(call(reinterpret_cast<void*>(1))!=static_cast<int>(0x80260004U) || audio_output_backend.output(0,nullptr)!=static_cast<int>(0x80260003U))return 1;
 std::cout<<"{\"scope\":\"audio_pcm_control\",\"SysV2\":true,\"native_XAudio2_consumed_frames\":512,\"channels\":8,\"synthetic_silence\":true,\"retained_buffer\":true,\"invalid_pointer_handle\":true,\"passed\":true}\n";return 0;
}
// S16_8CH public audioout.cpp55-56: signed16, 8 channels, identity channel order.
inline int audio_s16_control(){
 if(audio_output_backend.initialize())return 1;
 const int handle=ctr_audio_out_open_fixture(255,0,0,256,48000,2);if(handle!=0x20000000)return 1;
 auto* pcm=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!pcm)return 1;
 DWORD old=0;if(!VirtualProtect(pcm+4096,4096,PAGE_NOACCESS,&old))return 1;
 const auto call=[&](const void* data){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_audio_out_output),handle,reinterpret_cast<uint64_t>(data)));};
 // Exactly4096 accessible bytes. Float-sized8192 read would cross the guard.
 bool passed=call(pcm)==2048 && call(nullptr)==0 && audio_output_backend.played_samples_control(0)==256;
 passed=passed && call(pcm)==2048 && call(nullptr)==0 && audio_output_backend.played_samples_control(0)==512;
 passed=passed && call(pcm+1)==static_cast<int>(0x80260004U);
 pcm[0]=1;passed=passed && call(pcm)==2048 && call(nullptr)==0 && audio_output_backend.played_samples_control(0)==768;
 VirtualFree(pcm,0,MEM_RELEASE);
 if(!passed)return 1;
 std::cout<<"{\"scope\":\"audio_s16_control\",\"SysV6_open_SysV2_output\":true,\"PCM_signed16_channels8\":true,\"guarded_input_bytes4096\":true,\"native_consumed_frames768\":true,\"synthetic_silence_then_nonzero_LSB\":true,\"audibility_not_claimed\":true}\n";return 0;
}
extern "C" void ctr_sysv_audio_out_init();
inline int audio_output_control(){
 AudioOutputBackend backend;bool rejected=false;try{backend.format();}catch(...){rejected=true;}
 if(!rejected || backend.initialize() || backend.initialize()!=static_cast<int>(0x8026000eU))return 1;
 auto format=backend.format();if(!format.first || !format.second)return 1;
 if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_audio_out_init),0,0))return 1;
 std::cout<<"{\"scope\":\"audio_output_control\",\"real_XAudio2_endpoint\":true,\"duplicate_rejected\":true,\"uninitialized_rejected\":true,\"guest_PCM\":false,\"passed\":true}\n";return 0;
}
}
