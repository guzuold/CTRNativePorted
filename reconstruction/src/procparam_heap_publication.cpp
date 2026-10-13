#include "ctr/procparam_heap_publication.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
ProcParamHeapLease retain_procparam_heap_api(std::shared_ptr<const NativeProcParam> parameters,std::shared_ptr<const NativeGameHeapApi> table){
 if(!parameters||!table)throw std::invalid_argument("Actual loaded process metadata and callable table required");
 const auto root=parameters->section(0x02600000),libc=parameters->section(0x02200000);
 const auto& binding=table->binding();const auto source=binding.installation.source->bytes();
 if(std::memcmp(source.data(),root.data(),88)||std::memcmp(source.data()+88,libc.data(),144)||std::memcmp(source.data()+232,libc.data()+0xd8,112)||std::memcmp(source.data()+344,libc.data()+0x148,112))throw std::invalid_argument("Table source differs from actual loaded process graph");
 return {std::move(parameters),std::move(table)};
}
}
