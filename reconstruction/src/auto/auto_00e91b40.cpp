// SPDX-License-Identifier: GPL-2.0
// Retail entry:0xe91b40
// eboot SHA256:ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1
// Verification:reports/reconstruction/auto/harness/auto_00e91b40/validation.json
#include "ctr/auto/auto_00e91b40.hpp"
#include <cstring>
namespace ctr {
void auto_00e91b40(std::uint8_t* object){
 std::uint32_t counter;std::memcpy(&counter,object+72,4);
 std::int32_t signedCounter;std::memcpy(&signedCounter,&counter,4);
 if(signedCounter>0){
  counter+=1;
  do {
   const std::uint32_t value=counter-2;counter-=1;
   std::memcpy(object+72,&value,4);
   std::memcpy(&signedCounter,&counter,4);
  }while(signedCounter>1);
 }
}
}
