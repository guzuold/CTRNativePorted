#define NOMINMAX
#include "ctr/typed_game_heap_callback.h"
#include "ctr/game_global_malloc.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ctr {
namespace {
char module_anchor{};
constexpr std::size_t output_size=3*(8+16384+32+256+128)+8+32+256+128+8;
}

struct NativeTypedGameHeapCallback::CodeModuleLease {
 HMODULE module{};
 CodeModuleLease(){if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
      reinterpret_cast<LPCWSTR>(&module_anchor),&module))throw std::runtime_error("Typed callback module lease failed");}
 ~CodeModuleLease(){if(module)FreeLibrary(module);}
};

NativeTypedGameHeapCallback::NativeTypedGameHeapCallback(const std::filesystem::path& original,
 std::span<const std::byte> input) {
 if(input.size()!=input_.size())throw std::invalid_argument("Typed callback fixture extent differs");
 std::memcpy(input_.data(),input.data(),input.size());
 std::uint32_t poison{},debug{};std::memcpy(&poison,input_.data(),4);std::memcpy(&debug,input_.data()+4,4);
 if(poison>1||debug!=1)throw std::invalid_argument("Typed callback fixture settings differ");

 // Build all callable state in this module. The code lease and DSO owner stay
 // alongside the backend until the final shared callable handle is released.
 code_=std::make_shared<CodeModuleLease>();
 dso_=NativeGameDso::load(original);
 std::uintptr_t actual{};const auto selected=dso_->translate(original_game_dso_id,8);
 std::memcpy(&actual,selected.data(),8);
 if(actual!=reinterpret_cast<std::uintptr_t>(selected.data()))throw std::runtime_error("Selected DSO self-pointer does not name its owned bytes");
 backend_=std::make_shared<SystemPoolBackend>();
 MspaceGlobalConfiguration configuration{static_cast<std::uint8_t>(poison),1,1,1,1};
 initialize_preconfigured_system_pool(*backend_,0x4000,configuration);
 const auto descriptor=backend_->original_descriptor_identity;
 const auto physical=backend_->physical;
 const auto heap=backend_->heap.header_token;
 std::memcpy(manager_.data(),&descriptor,8);manager_[8]=static_cast<std::byte>(backend_->initialized_flag);
 std::memcpy(manager_.data()+9,input_.data()+424,7);std::memcpy(manager_.data()+0x10,&physical,8);std::memcpy(manager_.data()+0x18,&heap,8);
 callback_=&NativeTypedGameHeapCallback::invoke_compiled_callback;
 if(!callback_||backend_->original_manager_identity!=original_game_heap_argument_id)
  throw std::runtime_error("Typed callback registration identities differ");
}

GameHeapDestructorResult NativeTypedGameHeapCallback::invoke_compiled_callback(NativeTypedGameHeapCallback& owner){
 return owner.execute_original_contract();
}

GameHeapDestructorResult NativeTypedGameHeapCallback::execute_original_contract(){
 std::uint64_t selected_self{};auto selected=dso_->translate(original_game_dso_id,8);
 std::memcpy(&selected_self,selected.data(),8);
 if(selected_self!=reinterpret_cast<std::uintptr_t>(selected.data())||
    backend_->original_manager_identity!=original_game_heap_argument_id||
    reinterpret_cast<std::uintptr_t>(code_->module)==0)
  throw std::invalid_argument("Typed callback owners or binary identities no longer match");
 std::uint32_t poison{};std::memcpy(&poison,input_.data(),4);
 PreparedGameGlobalMalloc binding{backend_.get(),0x01437990,static_cast<bool>(poison)};
 for(std::size_t i=0;i<4;++i){std::uint64_t size{};std::memcpy(&size,input_.data()+8+i*8,8);
  auto allocation=allocate_game_global_top(binding,size);
  if(allocation.address_token!=reinterpret_cast<std::uint64_t>(allocation.payload.data()))
   throw std::runtime_error("Typed callback backend returned a non-native allocation");
  std::fill(allocation.payload.begin(),allocation.payload.end(),static_cast<std::byte>(0x40+i));
 }
 std::array<std::byte,256> stack{};std::memcpy(stack.data(),input_.data()+40,stack.size());
 std::array<std::uint64_t,16> registers{};std::memcpy(registers.data(),input_.data()+296,128);
 if(!finalizer_frame_){registers[5]=original_game_heap_argument_id;const std::uint64_t stop=0x30000000;
 std::memcpy(stack.data()+248,&stop,8);}
 const auto actual_mutex=reinterpret_cast<std::uintptr_t>(&backend_->heap.mutex->prefix);
 auto result=finalizer_frame_?destroy_registered_game_heap_from_finalizer(*backend_,manager_,registers,stack):
     destroy_registered_game_heap(*backend_,manager_,registers,stack);
 for(std::size_t phase=0;phase<result.before_release.size();++phase){std::uint64_t cell{};
  std::memcpy(&cell,result.before_release[phase].backing.data()+0x398,8);
  if(cell!=(phase<2?actual_mutex:0))throw std::runtime_error("Typed callback snapshot lost original mutex-cell state");}
 return result;
}

std::vector<std::byte> NativeTypedGameHeapCallback::dispatch(std::uint64_t callback_id,
 std::uint64_t argument_id,std::uint64_t dso_id){
 if(callback_id!=original_game_heap_callback_id||argument_id!=original_game_heap_argument_id||
    dso_id!=original_game_dso_id)throw std::invalid_argument("Original callback/argument/DSO identity not registered");
 if(invoking_.test_and_set())throw std::invalid_argument("Typed callback is already invoking or completed");
 if(invoked_){invoking_.clear();throw std::invalid_argument("Original destructor callback is single-use");}
 invoked_=true;
 try {
  // Copy the callable and all three owners locally across invocation.
  const auto callback=callback_;const auto dso=dso_;const auto code=code_;const auto backend=backend_;
  if(!callback||!dso||!code||!backend)throw std::invalid_argument("Typed callback owner is incomplete");
  const auto result=callback(*this);
  bool backing_live=false;try{(void)backend->memory.bytes();backing_live=true;}catch(const std::invalid_argument&){}
  if(backing_live)throw std::runtime_error("Destructor callback did not release its backing");
  if(backend->original_manager_identity||backend->heap.mutex)throw std::runtime_error("Destructor callback did not clear live manager ownership");
  std::vector<std::byte> out;out.reserve(output_size);
  auto append=[&](const auto& value){const auto* first=reinterpret_cast<const std::byte*>(&value);out.insert(out.end(),first,first+sizeof(value));};
  for(std::size_t phase=0;phase<result.before_release.size();++phase){const auto& snapshot=result.before_release[phase];append(snapshot.published_manager);auto backing=snapshot.backing;if(phase<2){const std::uint64_t logical_mutex=0x4000e000;std::memcpy(backing.data()+0x398,&logical_mutex,8);}out.insert(out.end(),backing.begin(),backing.end());out.insert(out.end(),snapshot.manager.begin(),snapshot.manager.end());out.insert(out.end(),snapshot.stack.begin(),snapshot.stack.end());out.insert(out.end(),reinterpret_cast<const std::byte*>(snapshot.registers.data()),reinterpret_cast<const std::byte*>(snapshot.registers.data()+snapshot.registers.size()));}
  append(result.published_manager);out.insert(out.end(),result.manager.begin(),result.manager.end());out.insert(out.end(),result.stack.begin(),result.stack.end());out.insert(out.end(),reinterpret_cast<const std::byte*>(result.registers.data()),reinterpret_cast<const std::byte*>(result.registers.data()+result.registers.size()));const std::uint64_t freed=1;append(freed);
  if(out.size()!=output_size)throw std::runtime_error("Typed callback serialized output extent differs");
  invoking_.clear();return out;
 } catch (...) {invoking_.clear();throw;}
}

void NativeTypedGameHeapCallback::validate_finalizer_frame() const {
 std::uint64_t rsp{},rbp{},arg{},ret{};
 std::memcpy(&rsp,input_.data()+296+14*8,8);std::memcpy(&rbp,input_.data()+296+15*8,8);
 std::memcpy(&arg,input_.data()+296+5*8,8);std::memcpy(&ret,input_.data()+288,8);
 if(rsp!=0x2000fe08||rbp!=0x2000fff0||arg!=original_game_heap_argument_id||ret!=0x500adad9)
  throw std::invalid_argument("Original measured finalizer callback frame required");
}
std::vector<std::byte> NativeTypedGameHeapCallback::dispatch_finalizer(std::uint64_t callback_id,
 std::uint64_t argument_id,std::uint64_t dso_id){
 validate_finalizer_frame();
 finalizer_frame_=true;
 try{auto result=dispatch(callback_id,argument_id,dso_id);finalizer_frame_=false;return result;}
 catch(...){finalizer_frame_=false;throw;}
}

TypedGameHeapCallbackStatus NativeTypedGameHeapCallback::status() const {
 TypedGameHeapCallbackStatus result{};result.invoked=invoked_;
 result.callback_owner_references=static_cast<std::uint64_t>(dso_.use_count());
 if(dso_)result.dso_address=reinterpret_cast<std::uintptr_t>(dso_->page().data());
 if(code_)result.code_module=reinterpret_cast<std::uintptr_t>(code_->module);
 if(backend_){result.backend_released=backend_->original_manager_identity==0&&!backend_->heap.mutex;
  try{(void)backend_->memory.bytes();result.backend_released=false;}catch(const std::invalid_argument&){} }
 return result;
}
}
