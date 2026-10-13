// SPDX-License-Identifier: GPL-2.0-only
// New owned attribute implementation. Public pinned shadPS4 behavior recorded in
// (development notes)
#pragma once
#include "kernel_mutex.hpp"
namespace ctrport {
struct KernelMutexAttributes { std::uint32_t type=1,protocol=0;std::int32_t ceiling=0; };
static_assert(sizeof(KernelMutexAttributes)==12);
inline std::mutex mutex_attributes_registry_lock;
inline std::unordered_map<std::uint64_t,std::shared_ptr<KernelMutexAttributes>> mutex_attributes_registry;
inline int kernel_mutex_attributes_init(std::uint64_t* cell) {
    if(!cell) throw std::runtime_error("Mutex attributes cell null unsupported");
    require_time_output(cell,8);
    auto object=std::make_shared<KernelMutexAttributes>();
    const auto handle=reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
    mutex_attributes_registry.emplace(handle,object);*cell=handle;return 0;
}
inline int kernel_mutex_attributes_settype(std::uint64_t* cell,std::uint32_t type) {
    if(!cell) return static_cast<int>(0x80020016U); // Orbis EINVAL.
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
    if(!*cell || type<1 || type>=5) return static_cast<int>(0x80020016U);
    const auto found=mutex_attributes_registry.find(*cell);
    if(found==mutex_attributes_registry.end()) throw std::runtime_error("Unknown mutex attributes handle unsupported");
    found->second->type=type;return 0;
}
inline int kernel_mutex_attributes_settype_control() {
    std::uint64_t cell=0;kernel_mutex_attributes_init(&cell);
    unsigned valid=0,invalid=0;
    for(std::uint32_t type=1;type<5;++type) {
        if(kernel_mutex_attributes_settype(&cell,type) || mutex_attributes_registry.at(cell)->type!=type) throw std::runtime_error("Attribute type update failed");
        ++valid;
    }
    for(auto type:{0U,5U,0xffffffffU}) {
        if(kernel_mutex_attributes_settype(&cell,type)!=static_cast<int>(0x80020016U) || mutex_attributes_registry.at(cell)->type!=4) throw std::runtime_error("Invalid type changed attribute");
        ++invalid;
    }
    std::uint64_t empty=0,unknown=0x12345678;
    if(kernel_mutex_attributes_settype(nullptr,2)!=static_cast<int>(0x80020016U) || kernel_mutex_attributes_settype(&empty,2)!=static_cast<int>(0x80020016U)) throw std::runtime_error("Invalid attribute accepted");
    bool rejected=false;try {kernel_mutex_attributes_settype(&unknown,2);} catch(const std::runtime_error&) {rejected=true;}
    if(!rejected) throw std::runtime_error("Foreign attribute handle accepted");
    std::cout<<"{\"scope\":\"host_mutex_attribute_settype\",\"valid_types_verified\":"<<valid<<",\"invalid_types_preserved_state\":"<<invalid<<",\"null_and_empty_einval\":true,\"foreign_handle_rejected\":true}\n";
    return 0;
}
inline int kernel_mutex_init(std::uint64_t* cell,const std::uint64_t* attributes,const char* name) {
    if(!cell) throw std::runtime_error("Mutex output cell null unsupported");
    require_time_output(cell,8);
    std::string owned_name;
    if(name) {
        bool terminated=false;
        for(std::size_t i=0;i<256;++i) {char ch=0;SIZE_T read=0;
            if(!ReadProcessMemory(GetCurrentProcess(),name+i,&ch,1,&read) || read!=1) throw std::runtime_error("Mutex name unreadable");
            if(!ch) {terminated=true;break;}owned_name.push_back(ch);
        }
        if(!terminated) throw std::runtime_error("Mutex name exceeds supported256B bound");
    }
    KernelMutexAttributes values;
    if(attributes) {
        require_time_output(attributes,8);
        std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
        if(*attributes) {
            const auto found=mutex_attributes_registry.find(*attributes);
            if(found==mutex_attributes_registry.end()) throw std::runtime_error("Foreign mutex attributes unsupported");
            values=*found->second;
        }
    }
    if((values.type!=1 && values.type!=2) || values.protocol || values.ceiling) throw std::runtime_error("Mutex type/protocol unsupported");
    auto object=std::make_shared<KernelMutex>();object->type=values.type;object->name=owned_name;
    const auto handle=reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(mutex_registry_lock);
    mutex_registry.emplace(handle,object);*cell=handle;return 0;
}
inline int named_mutex_control() {
    std::uint64_t cell=0;char name[]="original_heap_mutex";
    kernel_mutex_init(&cell,nullptr,name);name[0]='X';
    if(mutex_registry.at(cell)->name!="original_heap_mutex" || kernel_mutex_lock(&cell) || kernel_mutex_unlock(&cell)) throw std::runtime_error("Owned named mutex differs");
    std::uint64_t untouched=123;unsigned rejected=0;
    try {kernel_mutex_init(&untouched,nullptr,reinterpret_cast<const char*>(1));} catch(const std::runtime_error&) {++rejected;}
    std::array<char,256> longname;longname.fill('a');
    try {kernel_mutex_init(&untouched,nullptr,longname.data());} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=2 || untouched!=123) throw std::runtime_error("Invalid name modified output");
    std::cout<<"{\"scope\":\"owned_named_mutex\",\"name_copied\":true,\"actual_lock_unlock\":true,\"negative_rejections\":2}\n";return 0;
}
inline int kernel_recursive_mutex_control() {
    std::uint64_t attr=0,cell=0;
    kernel_mutex_attributes_init(&attr);kernel_mutex_attributes_settype(&attr,2);
    kernel_mutex_init(&cell,&attr,nullptr);
    if(kernel_mutex_lock(&cell) || kernel_mutex_lock(&cell)) throw std::runtime_error("Recursive relock failed");
    std::promise<void> attempting;
    auto worker=std::async(std::launch::async,[&]{
        if(kernel_mutex_unlock(&cell)!=1) throw std::runtime_error("Recursive foreign unlock accepted");
        attempting.set_value();
        if(kernel_mutex_lock(&cell) || kernel_mutex_unlock(&cell)) throw std::runtime_error("Recursive contender failed");
    });
    attempting.get_future().wait();
    const bool before=worker.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout;
    const auto partial=kernel_mutex_unlock(&cell);
    const bool after=worker.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout;
    const auto final=kernel_mutex_unlock(&cell);worker.get();
    if(!before || !after || partial || final || kernel_mutex_unlock(&cell)!=1) throw std::runtime_error("Recursive depth/release differs");
    unsigned rejected=0;
    kernel_mutex_attributes_settype(&attr,3);
    try {kernel_mutex_init(&cell,&attr,nullptr);} catch(const std::runtime_error&) {++rejected;}
    std::uint64_t foreign=0x12345678;
    try {kernel_mutex_init(&cell,&foreign,nullptr);} catch(const std::runtime_error&) {++rejected;}
    try {kernel_mutex_init(&cell,nullptr,"name");} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=3) throw std::runtime_error("Unsupported explicit mutex accepted");
    std::cout<<"{\"scope\":\"host_recursive_mutex\",\"recursive_depth_verified\":2,\"partial_unlock_preserves_contention\":true,\"final_unlock_releases_contender\":true,\"foreign_unlock_eperm\":true,\"unsupported_rejected\":3,\"runtime_bound\":false}\n";
    return 0;
}
inline int kernel_mutex_attributes_destroy(std::uint64_t* cell) {
    if(!cell) return static_cast<int>(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(mutex_attributes_registry_lock);
    if(!*cell) return static_cast<int>(0x80020016U);
    const auto found=mutex_attributes_registry.find(*cell);
    if(found==mutex_attributes_registry.end()) throw std::runtime_error("Foreign/stale mutex attributes unsupported");
    mutex_attributes_registry.erase(found);*cell=0;return 0;
}
inline int kernel_mutex_attributes_destroy_control() {
    std::uint64_t attr=0,cell=0;
    kernel_mutex_attributes_init(&attr);kernel_mutex_attributes_settype(&attr,2);kernel_mutex_init(&cell,&attr,nullptr);
    auto stale=attr;
    if(kernel_mutex_attributes_destroy(&attr) || attr || mutex_attributes_registry.count(stale)) throw std::runtime_error("Attributes not released/cleared");
    if(kernel_mutex_lock(&cell) || kernel_mutex_lock(&cell) || kernel_mutex_unlock(&cell) || kernel_mutex_unlock(&cell)) throw std::runtime_error("Mutex depended on destroyed attributes");
    if(kernel_mutex_attributes_destroy(&attr)!=static_cast<int>(0x80020016U) || kernel_mutex_attributes_destroy(nullptr)!=static_cast<int>(0x80020016U)) throw std::runtime_error("Empty destroy accepted");
    unsigned rejected=0;
    try {kernel_mutex_attributes_destroy(&stale);} catch(const std::runtime_error&) {++rejected;}
    std::uint64_t foreign=0x12345678;
    try {kernel_mutex_attributes_destroy(&foreign);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=2) throw std::runtime_error("Stale/foreign destruction accepted");
    std::cout<<"{\"scope\":\"host_mutex_attribute_destroy\",\"owned_object_removed\":true,\"cell_cleared\":true,\"recursive_mutex_survives\":true,\"null_and_double_destroy_einval\":true,\"stale_foreign_rejected\":2}\n";
    return 0;
}
inline int kernel_mutex_attributes_control() {
    std::uint64_t cells[3]={0xdeadbeef,0,0xabcdef};
    kernel_mutex_attributes_init(&cells[0]);kernel_mutex_attributes_init(&cells[1]);
    if(!cells[0] || cells[0]==cells[1] || cells[2]!=0xabcdef) throw std::runtime_error("Attribute output extent/ownership failed");
    for(unsigned i=0;i<2;++i) {
        const auto object=mutex_attributes_registry.at(cells[i]);
        if(object->type!=1 || object->protocol || object->ceiling) throw std::runtime_error("Attribute defaults differ");
    }
    bool rejected=false;try {kernel_mutex_attributes_init(nullptr);} catch(const std::runtime_error&) {rejected=true;}
    if(!rejected) throw std::runtime_error("Null attribute accepted");
    std::cout<<"{\"scope\":\"host_mutex_attribute_init\",\"distinct_owned_objects\":2,\"defaults_verified\":true,\"output_tail_preserved\":true,\"null_rejected\":true,\"runtime_bound\":false}\n";
    return 0;
}
}
