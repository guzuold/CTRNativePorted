#include "ctr/gameplay/selected_handle.h"
namespace ctr {
void select_retained_handle(HandleRecord*& output, unsigned char selector,
 const std::array<HandleRecord*,2>* record){
 output=nullptr;
 if(!record)return;
 auto* target=(*record)[selector?0:1];
 if(target)target->flags_and_count.fetch_add(1);
 output=target;
}
}
